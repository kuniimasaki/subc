# バイトコードVM(`-O`)実装状況

Task 3(`docs/design/task3-vm-audit-and-design.md`)でポインタ安全性フック(`prim_vm_deref`
等)を実装した後、`-O` が実際にどこまでの C コードを実行できるかを `compileOn()` の全 AST
ノード種別を対象に棚卸しし、`demofiles/*.c` 全件+`mydemo/*.c` 全件を `-O` で実行して実測した
結果をまとめたもの。**この文書は現状のスナップショットであり、`compileOn()` に手を入れたら
更新すること。**

## 検証方法

- `main.leg` の `compileOn(oop exp, oop program, oop cs, oop bs)` の `switch (getType(exp))`
  を全ケース読み、`assert(!"unimplemented")` になっている枝を洗い出した。
- `demofiles/*.c`(52本)+ `mydemo/*.c`(10本)、計62本を素朴に `./main -O <file>` で実行し、
  正常終了/検出成功したものと、`compileOn` の `assert(!"unimplemented")` に当たって
  `SIGABRT` したものを分類した。
- ツリーウォーカー側(`-O` 無し)は `make test` で 52 passed, 0 failed を確認済み(本調査による
  regressionなし)。

## 結果サマリ

| 分類 | 件数 |
|---|---|
| `-O` で最後まで実行でき、期待通りの結果(検出成功 or 正常終了)だった | 15 / 62 |
| `compileOn()` の `assert(!"unimplemented")` でインタプリタごと `SIGABRT` した | 47 / 62 |

**重要な注意点**: 未実装ノードにヒットした場合、`fatal()` のような「行儀の良い」エラーではなく
C レベルの `assert()` がそのまま失敗して `SIGABRT`(終了コード134)する。ツリーウォーカーが
バグを検出したときのメッセージ(`"getting freed memory was not allowed"` 等)とは性質が異なり、
**「バグを検出した」のではなく「VMがそのコードをコンパイルできず落ちた」**という区別が必要。
現状 `-O` で通ったテストのうち、実際に意図した安全性チェックが発火したのは
`vm-use-after-free.c` の1本のみ(他はキャスト等を使わない単純な正常系)。

## `compileOn()` のASTノード網羅状況(`_do_types` 55種)

| 状態 | ノード | 備考 |
|---|---|---|
| ✅ 実装済み | `Undefined`, `Input`, `Integer`, `Float`, `Symbol`, `Pair`, `String`, `Closure`, `Call`, `Block`, `Dereference`, `Sizeof`, `Index`, `While`, `If`, `Return`, `Continue`, `Break`, `VarDecls`, `Function`, `Primitive` | Task 3までに実装。`Dereference`/`Index`/ポインタ代入は `prim_vm_*` 安全性フック経由。 |
| ✅ 実装済み(演算子単位) | `Unary` の `NEG`/`NOT`/`COM` | 単項マイナス・論理否定・ビット否定。 |
| ✅ 実装済み(演算子単位) | `Binary` の `MUL DIV MOD ADD SUB SHL SHR LT LE GE GT EQ NE BAND BXOR BOR` | 短絡評価が要らない二項演算子は全部実装済み。 |
| ❌ 未実装(実際に踏む) | `Cast` | 明示的なポインタキャスト `(int *)malloc(...)` など。**最大のボトルネック**。malloc系のテストのほぼ全てがこれで落ちる。 |
| ❌ 未実装(実際に踏む) | `Addressof` (`&x`) | `dangling-pointer.c` 等で使用。 |
| ❌ 未実装(実際に踏む) | `Unary` の `PREINC PREDEC POSTINC POSTDEC` (`++`/`--`) | `break-in-if-ok.c` 等、非常によく使われる。 |
| ❌ 未実装(実際に踏む) | `Binary` の `LAND LOR` (`&&`/`\|\|`、短絡評価) | `modulo-and-bitwise-ok.c`。 |
| ❌ 未実装(実際に踏む) | `For` | `char-buffer-oob.c`、`memory-leak.c` 等、ループを使うテストの大半。 |
| ❌ 未実装(実際に踏む) | `Switch` / `Case` / `Default` | `switch-ok.c`。 |
| ❌ 未実装(実際に踏む) | `Member` (`s.field`) | 構造体フィールドアクセス。踏んだテストは無いが `struct-null-pointer-field.c` 相当のコードは通らない。 |
| ❌ 未実装(実際に踏む) | `List`(配列初期化子リスト `{1,2,3}`) | `out-of-bounds-access.c`、`pointer-compare.c` 等。 |
| ❌ 未実装(実際に踏む) | `TypeDecls` | トップレベルの `typedef`/複数宣言。`invalid-pointer.c`、`fisr.c` が使用。 |
| ⚠️ 未実装だが通常は踏まない(データ値/パース時専用) | `Array`, `Pointer`, `Struct`, `Memory`, `Reference`, `Tvoid`..`Tetc`(11種), `Scope`, `TypeName`, `Variable`, `Constant`, `Token` | これらは「実行対象のASTノード」としてではなく、他ノードのフィールド(型情報・実行時値)として扱われるため、`compileOn()` が直接呼ばれる経路が(現状把握している限り)存在しない。**ただし未検証** — 別の構文パターンで踏む可能性は残る。 |

## `compileOn()` を通過しても壊れている既知の追加バグ

`assert` に当たらず実行はできるが、C の意味論と食い違う挙動を確認したもの:

- **ローカル配列/構造体の宣言が初期値なしだと `nil` にバインドされる**: `VarDecls` の実装は
  `prim_vm_coerce` を通すが、これは `Tpointer` の場合しか特別扱いしない(Task 3 で実装した
  `int *p = malloc(...)` 用のポインタ型強制のみ)。`int a[3];` のように初期化式が無い配列/構造体
  宣言は `init` が `nil` のまま `iDECL` され、実際の `Array`/`Struct` オブジェクトが確保されない。
  検証: `int a[3]; a[0]=1; return a[0];` を `-O` で実行すると
  `cannot index-assign type Undefined` で fatal する(クラッシュではなく、ツリーウォーカーの
  `initialiseVariable()` が本来やっている「宣言時に実体を確保する」処理が VM 側に無いことが原因)。
  ツリーウォーカー(`-O` 無し)では同じコードは正しく動作する。

## 優先度付きの残作業(参考: 出現頻度ベース)

`demofiles/*.c` 52本中で実際にクラッシュの原因になった行数を集計した結果、直すインパクトが
大きい順に並べると:

1. **`Cast`** — 圧倒的多数(malloc系テストほぼ全て)。実装すれば一気に通過数が増える。
2. **`For`** — ループを使うテストの大半。
3. **`Addressof`** / **`++`/`--`** — dangling-pointer系、カウンタ変数の更新で頻出。
4. **配列初期化子(`List`)** / **ローカル配列・構造体の実体確保** — 上記の追加バグと合わせて対応が必要。
5. **`Switch`**、**`Member`**、**`LAND`/`LOR`**、**`TypeDecls`** — 個別のテスト1〜2本ずつ。

`docs/design/task3-vm-audit-and-design.md` が推奨した Option B(`prim_*` 経由で安全性チェックを
一本化する)方針は上記いずれについても踏襲可能(`Cast` は `castPointer()`/`converter()` を呼ぶ
プリミティブに、`For` は `While` の展開に、ローカル配列/構造体は `initialiseVariable()` 相当の
ロジックを呼ぶプリミティブに、という形で拡張できる想定)。次にVM対応を進める場合はこの表を
更新すること。
