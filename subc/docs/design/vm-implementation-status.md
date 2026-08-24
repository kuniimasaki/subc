# バイトコードVM(`-O`)実装状況

Task 3(`docs/design/task3-vm-audit-and-design.md`)でポインタ安全性フック(`prim_vm_deref`
等)を実装した後、`-O` が実際にどこまでの C コードを実行できるかを `compileOn()` の全 AST
ノード種別を対象に棚卸しし、`demofiles/*.c`+`mydemo/*.c` 全件を `-O` で実行して実測、
見つかったギャップを1つずつ埋めた記録。**この文書は現状のスナップショットであり、
`compileOn()`/`execute()` に手を入れたら更新すること。**

## 現在の状態(2026-08-24 時点)

`demofiles/*.c`(66本)+ `mydemo/*.c`(10本)、計76本を `-O` で実行した結果、
**全76本が `compileOn()`/`execute()` レベルのクラッシュなく最後まで実行できる**
(調査開始時点は15/62)。`make test` は66 passed, 0 failed(うち17本が`FLAGS=-O`の
VM専用回帰テスト、`vm-*.c`/`vm-*-ok.c`)。

以下、実装済みの機能と、判明している既知の残課題をまとめる。

## `compileOn()` のASTノード網羅状況(`_do_types` 55種)

| 状態 | ノード | 備考 |
|---|---|---|
| ✅ 実装済み | `Undefined`, `Input`, `Integer`, `Float`, `Symbol`, `Pair`, `String`, `Closure`, `Call`, `Block`, `Dereference`, `Sizeof`, `Index`, `While`, `For`, `If`, `Switch`/`Case`/`Default`, `Return`, `Continue`, `Break`, `VarDecls`, `TypeDecls`, `Function`, `Primitive`, `Addressof`, `Cast`, `Member` | `Dereference`/`Index`/`Cast`/`Addressof`/`Member`/ポインタ代入・メンバ代入は全て `prim_vm_*` プリミティブ経由(Option B、ツリーウォーカーの既存関数をそのまま再利用)。`VarDecls`/`TypeDecls` はトップレベル出現時に自己 `typeCheck()` する(下記参照)。 |
| ✅ 実装済み(演算子単位) | `Unary` の `NEG`/`NOT`/`COM`/`PREINC`/`PREDEC`/`POSTINC`/`POSTDEC` | `++`/`--` は `prim_vm_incrdecr` 経由。 |
| ✅ 実装済み(演算子単位) | `Binary` の全演算子(`MUL DIV MOD ADD SUB SHL SHR LT LE GE GT EQ NE BAND BXOR BOR LAND LOR`) | `LAND`/`LOR` は短絡評価(JMPF/JMPによる制御フロー)。`ADD` はポインタ/配列+整数のポインタ演算に対応。比較演算子は `compare()`/`equal()` 経由(下記のiJMPF/比較バグ参照)。 |
| ⚠️ 未実装だが通常は踏まない(データ値/パース時専用) | `Array`, `Pointer`, `Struct`, `Memory`, `Reference`, `Tvoid`..`Tetc`(11種), `Scope`, `TypeName`, `Variable`, `Constant`, `Token` | これらは「実行対象のASTノード」としてではなく、他ノードのフィールド(型情報・実行時値)として扱われるため、`compileOn()` が直接呼ばれる経路は無い。**未検証** — 別の構文パターンで踏む可能性は残る。 |

## 実装の勘所

- **`Cast`**: `Cast,converter` は typeCheck が確定させる生の C 関数ポインタ(`cvt_t`)で、
  VMのスタックには乗せられない。`compileOn` は値ではなく `Cast` ノード自身を第2引数として
  `prim_vm_cast` に渡し、`get(castExp, Cast,converter)` で同じ関数ポインタに到達する
  (`eval()` のCastケースの完全なミラー)。
- **`Addressof`/ローカル変数の実体化**: `&x` がローカル変数を指すには、VMの `env`
  (ただの `(symbol . value)` alist)にも「ツリーウォーカーの `Variable` オブジェクト」に
  相当する箱が必要だった。`iDECL` と `iCALL` のパラメータ束縛を、値を直接conすのではなく
  呼び出しごとに新しい `Variable` オブジェクトでラップするように変更(再帰呼び出しが
  互いを壊さないよう、毎回フレッシュに生成)、`iGETGVAR`/`iSETGVAR` も `Variable,value`
  経由に変更。これで `getPointer`/`setPointer` の既存の `case Variable:` がVMのローカルにも
  無変更で効くようになった。
- **ローカル配列/構造体の実体確保**: `VarDecls` は宣言された型で分岐し、`Tarray`/`Tstruct`
  は `prim_vm_alloc_array`/`prim_vm_alloc_struct`(`initialiseVariable()` のTarray/Tstruct
  ケースの移植、`CALLOC`+`newMemory`+`newArray`/`newStruct`+初期化子代入、初期化式が
  無ければ `randomise()` で「未初期化変数」検出も再現)を通す。それ以外(ポインタ・数値型)は
  従来通り `prim_vm_coerce`。
- **配列初期化子リスト** (`{1,2,3}`, `"literal"`): 各要素式(または文字列の各文字)を
  コンパイル時に展開して `prim_vm_alloc_array` に渡す。
- **`Switch`/`Case`/`Default`**: 条件式を一度だけ評価して隠しローカル変数
  (`"<switch-cond>"`、識別子として構文的に出現し得ない名前なので衝突しない)に格納し、
  各 `Case` の値との比較チェーン(`iGETGVAR`+`iEQ`+`iJMPF`)で一致した本体位置へジャンプ。
  本体はフラットな文リストとしてそのまま実行(C言語のフォールスルーがそのまま実現される)。
  `continue` はswitchでは捕まえず(`cs` をそのまま body に渡す)外側のループへ伝播、
  `break` は新しい `breaks` リストで捕まえる(While/Forと同じ)。
- **`Member`**: `prim_vm_member`/`prim_vm_store_member` は `eval()`/`assign()` の Member
  ケースの素直な移植(`Tstruct,members` から名前でオフセット・型を検索し `getMemory`/
  `setMemory`)。
- **関数ローカルのスコープ終了処理(dangling pointer検出)**: ツリーウォーカーの
  `Scope_end()`/`kill_scpp()` は関数を抜けるたびにそのスコープの `Variable` を
  `isdead=1` にマークし、`&localvar` が外へ漏れて後から参照されるバグ
  (`demofiles/dangling-pointer.c`)を検出する。VMの `env` はツリーウォーカーの
  `scopes`(`Scope_begin`/`Scope_end` で管理される入れ子リスト)とは全く別の、
  呼び出しごとに `iDECL`/パラメータ束縛で積み上がるだけの alist なので、
  「関数呼び出しが終わったら、その呼び出しで束縛されたローカルだけを殺す」処理を
  VM独自に実装する必要があった。`struct Frame` に `baseEnv`(そのクロージャが
  捕捉していた、パラメータ束縛前の環境)を追加し、`iCALL` のClosure分岐で
  パラメータを束縛する**前**の `environment` を保存。`iRETURN` は現在の `env` から
  `baseEnv` に到達するまで環境チェーンを辿り、見つけた `Variable` を全て
  `isdead=1` にする(再帰呼び出しでも各フレームの `baseEnv` は独立しているため、
  外側の呼び出しのローカルを誤って殺すことはない。`nfib(8)` が正しく `67` を
  返すことで確認済み)。
  - **副次的に発見**: `prim_vm_store_deref`(`*p = rhs` のVM実装、Task 3で追加)が
    `assign()` のDereferenceケースが持つ `isdead` チェックを丸ごと欠いていた
    (Task 3時点でのコピー漏れ、今回の作業とは無関係の既存バグ)。上記のスコープ
    終了処理を実装しても、この書き込み側のチェックが無ければ検出は発火しない
    ため、`assign()` と同じチェックを追加して修正。
- **動的に伸びる値/フレームスタック**: `oop stack[32]`/`struct Frame frames[32]`
  という固定長配列を、`MALLOC`+`memcpy` で倍々に伸びるヒープバッファに置き換えた。
  どちらも「先頭(index 0)側が満杯、`sp`/`fp` が容量から数えた残り」という
  **下向きに詰まる**方式だったため、単純な `REALLOC` では既存要素の位置が
  ズレてしまう(`REALLOC` は末尾側に空きを増やすが、この方式で有効な要素は
  常に配列の**上端**に寄っている)。共通ヘルパー `growDownArray()` が
  新しい(倍容量の)バッファを確保し、現在有効な要素だけを**新バッファの上端**に
  詰め直してから `sp`/`fp` を新容量基準で振り直すことで、`stack[sp]`・
  `stack+sp`・`frames[fp]` など既存の相対参照ロジックを一切変更せずに
  住み替えられるようにした。`push()` は満杯(`sp==0`)を検知したら伸長してから
  積む、`iCALL` のクロージャ分岐も同様に `fp==0` で伸長してからフレームを積む。
  深さ5000の再帰(`depth(5000)`)で正しく動作すること、`nfib(32)`
  (以前は深さ32でスタックオーバーフローしていたケース)が最後まで完走することを確認。
- **トップレベル `VarDecls`/`TypeDecls` の自己 typeCheck**: `replFile` の `-O` 経路は
  トップレベルの各構文要素を `typeCheck()`/`preval()` に一切通さず直接 `compile`+`execute`
  する(`Function`/`Primitive` は `compileOn` 内で自己 `typeCheck()` して補っていたが、
  素の `VarDecls`(グローバル変数、あるいは `struct Point { int x, y; };` のような
  タグ宣言のみの宣言)と `TypeDecls`(`typedef`)には同様の補完が無かった)。
  「まだ型解決されていないか」を「先頭要素が期待する型(`Variable`/`TypeName`)になって
  いないか」で判定し、必要なら自身に `typeCheck(exp, nil)` を実行してから処理を続ける
  ように修正。

## 見つけて直した、クラッシュではない静かなバグ

- **`iJMPF` が `nil` としか比較しておらず、`false` を認識できなかった**: `false` の実体は
  `newInteger(0)`(`nil` とは別オブジェクト)。比較・論理演算子は常に `true`/`false` を
  積むため、`iJMPF` の分岐は事実上一度も発火せず、`while` の本体が1回でも実行されると
  無限ループしていた(`assert(!"unimplemented")` の検知網に引っかからない、実行時に
  ハングするだけの静かなバグ)。`isFalse(cond)` を使うよう修正。`If` の false 分岐にも
  同じ効果があった(修正前は分岐そのものが機能していなかった)。
- **ポインタの加算・比較が `integerValue()` に丸投げされていた**: `execute()` の
  `iADD`/`iLT`/`iLE`/`iGE`/`iGT`/`iEQ`/`iNE` は `eval()` のBinaryケースと違い、
  ポインタ/配列を一切考慮せず常に `integerValue()`/`floatValue()` を呼んでいた
  (`assert(ptr != 0)` のようなごく普通のNULLチェックが `cannot convert Pointer to
  integer` で fatal していた)。`iADD` はポインタ/配列+整数のポインタ演算に対応、
  比較演算子は `compare()`/`equal()` 経由に修正(`eval()` と同じ経路)。
- **`Switch` の初期実装がbreak経路でスタックリークしていた**: `break` は自分の値を
  push してからジャンプするが、ジャンプ先(switch脱出ラベル)の直後に「フォールスルーで
  自然に文末へ到達した場合」用のpushが無条件で置かれており、break経由の到達だと
  二重にpushされていた。3つの脱出経路(break/`Default`無しでの不一致/自然な文末到達)が
  それぞれ厳密に1回だけpushするよう修正。

## 既知の残課題(意図的に今回は追わなかったもの)

- **`(int*)(intptr_t)0xDeadD0d0` のような typedef を挟んだキャスト連鎖**
  (`demofiles/invalid-pointer.c`)は、`-O` 経由だとどこかで `Pointer` ではなく
  生の `Integer` になってしまい、`printf("%p", ...)` がツリーウォーカーの
  「不正な書き込み」検出に到達する前に別のエラーで止まる。この特定の複雑な
  キャスト連鎖のみで発生し、この1ファイル以外には影響しない。
- **`mydemo/fisr.c`(高速逆平方根)はクラッシュしなくなったが数値が正しくない**:
  ビットレベルの float/int 型パニングが `-O` 経由では忠実に再現されていない
  (クラッシュはしない、という Task の目的自体は達成しているが、この特定の
  高度な構文の数値的な正確性までは踏み込んでいない)。

## 検証方法

- `main.leg` の `compileOn(oop exp, oop program, oop cs, oop bs)` の
  `switch (getType(exp))` を全ケース読み、未実装/バグの箇所を洗い出した。
- `demofiles/*.c`+`mydemo/*.c` を素朴に `./main -O <file>` で実行し、正常終了したもの、
  検出成功したもの、`assert(!"unimplemented")` でクラッシュしたものを分類。
  静かなバグ(ハング、誤った結果)は個別のテストプログラムで手動確認
  (`while`ループの反復回数を変えたテスト、gdbでの `fatal()` へのブレークポイント等)。
- ツリーウォーカー側(`-O` 無し)は各コミットごとに `make test` で回帰確認。
