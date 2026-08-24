# subc

C言語サブセットのインタプリタ型実行環境。

`main.leg`(PEGパーサジェネレータ `leg` 用の文法定義+評価器のC実装が同居する単一ファイル)を
`leg` で `main.c` にトランスパイルし、Boehm GC(`libgc`)にリンクしてビルドする。

研究テーマは「malloc/free まわりのメモリバグ(use-after-free, dangling pointer, double-free,
out-of-bounds など)を、ASan/Valgrind 相当に検出できる軽量な学習用インタプリタを作ること」。

## ディレクトリ構成

```
subc/
  main.leg        # 正本。文法定義+評価器のC実装(手で編集するのはここ)
  main.c          # main.leg から leg で生成される派生物(直接編集しない)
  main            # ビルド済みバイナリ
  Makefile        # ビルド・実行用
  demofiles/      # 典型的なメモリバグパターンのサンプル+回帰テスト
  mydemo/         # 一般的なCプログラムのサンプル(fib, eratosthenes, fisr など)
  docs/
    design/       # 設計メモ・調査レポート
    verification/ # 回帰確認・検証レポート
  CHANGELOG.md    # 変更履歴(詳細)
```

各ファイルの詳細な説明は [`FILES.md`](FILES.md) を参照(新規ファイル追加時はそちらにも追記)。

## ビルド

依存: `leg`/`peg`(Ian Piumarta の PEG parser generator)、`gcc`、`libgc-dev`。

```sh
cd subc
make          # main.leg -> main.c -> main をビルド
make test     # demofiles/*.c を自動テストハーネスで一括検証(PASS/FAIL判定)
make demo     # demofiles/*.c を全件実行(目視確認用)
```

`main.leg` を編集した場合は `leg -o main.c main.leg`(または `make`)で必ず `main.c` を
再生成すること。`main.c` を直接編集する運用は想定していない。

## 変更履歴

詳細・各コミットの調査過程は [`subc/CHANGELOG.md`](subc/CHANGELOG.md) を参照。概要:

### 2026-08-21

**メモリバグ検出まわりの既存バグ修正**
- `Pointer.isfree`(エイリアスに伝播せず機能していなかったフラグ)を廃止し、`Memory.free` を見る
  `requireNotFreed()` に一本化。`setMemory()`/`*p = rhs` に free チェックが無く
  「解放後の書き込み」が検出漏れになっていたのを修正。
- `p[i] = x`(mallocしたポインタへの添字代入)が `assign()` に未実装だったため
  正当な代入まで fatal していたのを修正。
- `compare()` のポインタ⇔整数比較が `equal()` からのコピペで tri-state を返せていなかったのを修正。
- `Makefile` 末尾のキャッチオール型パターンルールが `Makefile` 自身にマッチしてしまい
  `make demo` が壊れていたのを修正。
- リーク誤検知(`malloc→free` してもレポートされる)の根本原因(`initialiseVariable()` の
  `break;` 漏れによる初期化式の二重評価)を特定・修正。副次的に `setMemory()` の
  NULLポインタの構造体フィールド格納未対応も修正。

**自動テストハーネス**
- `if`/`while` の条件式でポインタ型を許可(`for` は既に許可済みだったのに揃えた)。
- `scripts/run-tests.sh` + `demofiles/*.expect` + `make test` で、`demofiles/*.c` を
  「期待する終了コード・出力」と突き合わせて自動判定できるようにした。陽性ケース(バグを踏む)
  だけでなく、陰性ケース(似ているが合法なコード、誤検知しないことの確認)6本も追加。

**ドキュメント**
- `docs/design/` に機能拡張(`realloc`/`calloc` 等)と VM(`-O`)監査の設計メモを追加。
- `docs/verification/` に Task 1 の回帰確認レポートを追加。
- パーサ側(AST生成)の監査を実施。`(f)(3)` のような括弧越しの関数呼び出しが型キャストと
  誤認される、`sizeof(typedef名)` が fatal するという2件の実バグを確認(**未修正**、詳細は
  `CHANGELOG.md` 参照)。

### 2026-08-22

**Task 6: マルチエージェント運用の試行 + Task 2: `realloc()`/`calloc()` 実装**
- 設計→実装→検証の3役パイプラインの運用方針を `docs/design/task6-multi-agent-workflow.md` に提案。
  Task 2 の `realloc()`/`calloc()` 実装を題材に実際に試行(実装エージェント→レビュー→検証エージェント
  →レビュー)。うまくいった点・詰まった点は同ドキュメント参照。
- `realloc()`: ブロックが移動した場合、旧ポインタを `free()` と同じ仕組みで無効化(この研究テーマの
  核心である use-after-free 検出が realloc 経由でも効くようにした)。移動しなかった場合はサイズだけ
  その場で更新し他のエイリアスを壊さない。`calloc()` はオーバーフローチェック付きでゼロ初期化。
- 副次的に発見: `void *pointer` が「唯一の引数ではない」場合に誤って `illegal void parameter` になる
  バグ(組み込み関数・ユーザー定義関数の両方)を修正。
- 回帰テスト12本追加、`make test` → 39 passed, 0 failed。

### 2026-08-23

**Task 2 続き: 境界値カバレッジ拡充と、その過程で見つかった基礎的なバグ**
- 境界値テスト拡充のため `demofiles/` を書く過程で3件の基礎的なバグを発見・修正:
  `malloc()` の負サイズチェックが符号なし変換後にチェックする tautology になっていた、
  `char c = 'a';` のような最も基本的なコードが `converter()` に `char`/`short` への
  変換が無く fatal していた、`typeCheck()` の代入文(`x = y;`)が数値変換を一切
  許可していなかった(`int x; long n=5; x = n;` すら fatal)。これにより `char` 型の
  基本的な使い方が初めて動くようになった。
- ヒープ配列・大きく外れたポインタ演算など境界値テスト6本を追加。`make test` → 45 passed, 0 failed。
- **見つけたが今回は直していない**: 多次元配列・構造体の配列メンバへのアクセスは
  `Array` 構造体にオフセット概念が無いため現状動作しない(中規模のアーキテクチャ変更が必要、
  詳細は `CHANGELOG.md` 参照)。

**Task 2 続き: 複合代入演算子・`switch`/`case`/`default`**
- `+= -= *= /= %= &= |= ^= <<= >>=` を追加。副次的に、`%`・`&`・`^`・`|`・`<=`・`>=`・
  `&&`・`||` の8演算子が `typeCheck()` で未実装のまま放置されていて普通に使うだけで
  クラッシュするバグを発見・修正(既存バグ、今回のセッションの変更とは無関係)。
- `switch`/`case`/`default` を追加(フォールスルー・`break`/`continue`のスコープの違いに対応)。
  副次的に、`typeCheck()` が `Continue`/`Break` を一切扱っておらず
  `while (1) { if (x) break; }` すらクラッシュするバグを発見・修正(同じく既存バグ)。
- `make test` → 49 passed, 0 failed。三項演算子 `?:`・`enum`/`union` は優先度が低いため見送り。

**Task 3: VM(`-O`)にポインタ安全性フックを実装**
- `-O` が実は `main()` を一切実行していなかった根本ギャップを解消(`return` 文の
  コンパイル未実装、`-O` 時に関数が一切 scope に登録されていなかった、等の複合バグ)。
- ポインタの読み書き(`*p`、`p[i]`、代入)を、ツリーウォーカーと**全く同じ**
  `getPointer`/`setMemory` 等を呼ぶだけの新規プリミティブへの呼び出しにコンパイルするよう変更
  (Task 3 設計メモの推奨案どおり、新しい安全性チェック用opcodeは一切追加していない)。
- **確認**: `malloc→書き込み→読み込み→free` が `-O` 経由で正しく動作し、
  `free()` 後に読むケースはツリーウォーカーと**全く同じメッセージ**で検出されることを確認。
  副次的に、`malloc()` の戻り値を明示キャストしないと(ツリーウォーカー側でも)
  fatal していたバグを発見・修正。
- `make test` → 52 passed, 0 failed。構造体・配列・`for`・キャストなどはまだ未対応
  (詳細は `CHANGELOG.md`/`docs/design/task3-vm-audit-and-design.md` 参照)。

**VM(`-O`)の実装状況を実測・文書化 → 見つかったクラッシュ原因を全て実装**
- 調査: `demofiles/*.c` + `mydemo/*.c` 計62本を `-O` で実行し分類: 最後まで動くもの15本、
  `compileOn()` 未実装ノードで `SIGABRT` するもの47本(コード変更は無し、調査のみ)。
- 実装: 頻度順に `Cast`・`For`・`Addressof`(`&x`)・`++`/`--`・`&&`/`||`・ローカル配列/構造体の
  実体確保・配列初期化子リスト・`Member`(`s.field`)・`Switch`/`Case`/`Default`・トップレベル
  `TypeDecls` を全て実装。`demofiles/*.c`+`mydemo/*.c` 計73本が全て `-O` でクラッシュせず
  実行完了するようになった(調査開始時点は15/62)。`make test` → 63 passed, 0 failed
  (VM専用回帰テスト14本を含む)。
  - 副次的に発見・修正した静かなバグ(クラッシュではなく無限ループ/誤動作)3件:
    `iJMPF` が `false`(実体は`Integer(0)`)を `nil` と取り違えて分岐が機能せず `while`
    ループが無限ループしていた/ポインタの加算・比較が `integerValue()` に丸投げされ
    `assert(ptr != 0)` のようなNULLチェックすら fatal していた/`switch` の初回実装が
    `break` 経路でスタックを1個ずつリークしていた。
  - 副次的に発見・修正した既存バグ1件: トップレベルの `VarDecls`/`TypeDecls`
    (グローバル変数宣言、構造体タグ宣言のみの宣言、`typedef`)が `-O` では
    typeCheck を一切通らず、ファイルスコープでの構造体宣言を含むプログラムが
    `expected Variable, got Undefined` で fatal していた。
  - 既知の残課題(意図的に深追いせず): VMにスコープ終了処理が無くローカル変数の
    生存管理バグは検出されない、VMのスタックが固定32要素で深い再帰は溢れる、
    typedefを挟んだ複雑なキャスト連鎖とビットレベルの型パニングは数値的な正確性が
    未検証。詳細は `docs/design/vm-implementation-status.md` 参照。

### 2026-08-24

**VM(`-O`)にスコープ終了処理(dangling pointer検出)を実装**
- 前日の既知の残課題1件目に対応。ツリーウォーカーの `Scope_end()`(関数を抜けるたびに
  ローカル変数を `isdead=1` にマークする仕組み)に相当する処理をVM独自に実装。
  `struct Frame` に `baseEnv`(クロージャ捕捉時点の環境)を追加し、`iRETURN` が
  現在の `env` から `baseEnv` までの間に束縛されたローカルを全て `isdead=1` にする
  (再帰呼び出しでも各フレームの `baseEnv` が独立しているため、外側の呼び出しの
  ローカルを誤って殺すことはない。`nfib(8)=67` が引き続き正しいことを確認)。
- 副次的に発見: `prim_vm_store_deref`(Task 3で追加した `*p = rhs` のVM実装)が
  `assign()` と同じ `isdead` チェックを欠いていた(Task 3時点のコピー漏れ)。
  これが無いとスコープ終了処理を実装しても検出が発火しないため、合わせて修正。
- 追加テスト: `vm-dangling-pointer.c`(検出成功を確認)、
  `vm-dangling-pointer-ok-alive.c`(まだ呼び出しスタック上にあるローカルへのポインタを
  誤って殺さないことを確認する陰性ケース)。
- `make test` → 65 passed, 0 failed(修正前は63)。`demofiles/*.c`+`mydemo/*.c`
  計75本の `-O` 網羅性スイープは引き続き全件クラッシュなし。

各修正は独立したコミットに分割し、コミットのたびに `demofiles/*.c` 全件を実行して
既存の検出結果(非決定的なヒープアドレスを除く)が意図せず変化していないことを確認している。
