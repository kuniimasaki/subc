# Changelog

`subc`(Cサブセット・インタプリタ)の変更履歴。日付は作業日、コミットハッシュは短縮形。

## 2026-08-21

### 修正 (Task 1: メモリバグ検出まわりの既存バグ修正)

- **`4d4638f`** — `Pointer.isfree` を廃止し、`Memory.free`(解放対象メモリブロックが持つ共有フラグ)を見る
  `requireNotFreed(oop base, char *action)` に一本化。
  - `isfree` は `free()` に渡された「その特定のポインタオブジェクト」だけに立つフラグで、同じメモリ領域を指す
    別名(エイリアス)のポインタには伝播せず、実質機能していなかった。
  - `getPointer()`(読み取り)・`setMemory()`・`*p = rhs` の直接代入パス・`printiln()` に一貫適用。
  - **発見した実害**: `setMemory()` と `*p = rhs` には free チェックが一切無く(死んだコメントアウトの残骸のみ)、
    `free(p); *p = 42;` のような「解放後の書き込み」が検出漏れになっていた。今回で修正。
  - 追加テスト: `demofiles/write-after-free.c`(読み取りを挟まずいきなり書き込むケース)。

- **`7e189cc`** — `p[i] = x`(mallocしたポインタへの添字代入)に対応。
  - `assign()` の `Index` ケースは `case Array:`(固定長配列)しか処理しておらず `case Pointer:` が無かった。
  - `setPointer()` を新設し、読み取り側の `getPointer()` と対称な形で書き込みを実装。
  - **発見した実害**: `demofiles/pointer-out-of-bounds-2.c` の `ptr[-2] = 42; // OK`(本来は正当な代入)すら
    `invalid rvalue ...` で fatal していた。修正後は正しく成功し、後続の実際に不正な `ptr[-6] = 666;` だけが
    正しく `memory offset is negative` として検出されるようになった。
  - 追加テスト: `demofiles/pointer-index-assign.c`。

- **`cc0f7a3`** — `compare()` のポインタ⇔整数比較で `equal()` からのコピペと思われる 0/1 真偽値を返すバグを修正。
  - `compare()` は `<` `<=` `>=` `>` に使う tri-state(-1/0/1)であるべきところ、int キャストポインタ vs 整数の
    分岐だけ真偽値を返しており、`<` `>` が常に false になっていた。
  - 追加テスト: `demofiles/pointer-compare-cast.c`(修正前 `not greater`/`not less` → 修正後 `greater`/`not less`)。

- **`7ab1dac`** — `main.c` / `main` を `subc/` ディレクトリで `leg` を直接実行して再生成し直す軽微な整形。
  作業中に一時ディレクトリ経由で生成した際に埋め込まれた `#line` のパスを `main.leg` に戻した(ロジック変更なし)。

- **`def36f4`** — `Makefile` 末尾のキャッチオール型パターンルール `% : main` を修正。
  - 制約なしに任意のターゲット名にマッチしてしまい、GNU Make が自身の再生成チェックの際に
    ターゲット名 `Makefile` にまでマッチさせてしまうバグがあった(`make demo` 実行時に
    `./main -vv mydemo/Makefile.c: No such file or directory` で失敗)。
  - `main.leg` を含む循環依存の誤検出(`Circular main.leg <- main dependency dropped.`)も併発していた。
  - Task 1 の変更とは無関係の既存バグ(修正前の Makefile でも再現)。今回のビルド作業でファイルの
    更新時刻の関係が変わり顕在化したとみられる。
  - `% : main mydemo/%.c` とし、`mydemo/` 配下に実在するデモファイルにしかマッチしないよう修正。

### 追加修正 (リーク誤検知の根本原因調査・修正)

- **`db54b89`** — `setMemory()` の `Tpointer` ケースで、格納する値が「base が `Integer`(型付きNULLなど)の
  `Pointer`」である場合を未対応のまま `assert(0)` していたのを修正。`base` が `Memory` の場合しか
  扱っておらず、`struct Link *list = 0;` のような型付きNULLを構造体フィールドへ代入すると
  クラッシュしていた(次のコミット単独では表面化しない、現状では無害な先行修正)。

- **`11486ea`** — リーク誤検知の根本原因を修正。`initialiseVariable()` の `switch(getType(type))` の
  `case Tpointer: { ... }` に `break;` が無く、直後の `default:` ケースに fallthrough して
  初期化式を**2回評価**していた。
  - `int *p = malloc(...);` のような宣言で、1回目の評価(型チェック込みで正しくキャストされた結果)が
    捨てられ、2回目の評価(生の結果で上書き)が `p` に入る。`malloc()` のような非冪等な初期化式では
    1回目のブロックが孤立し、実際には正しく `free()` しているのに「解放し忘れ」と誤検知されていた。
  - デバッグ用の `fprintf` を一時的に `prim_malloc`/`prim_free`/`apply`/`eval(Block)` に仕込んで
    スクラッチ環境でビルド・実行し、`malloc` が1回の宣言に対し2回呼ばれることを実測で確認してから
    本修正・除去。
  - この `break;` 追加だけを先に入れると `demofiles/use-after-free-2.c` で新たに
    (上記 `db54b89` が直した)`assert(0)` が発生するため、コミット順序を意図的に入れ替えている。
  - 副次的な出力変化(いずれもリグレッションではなく、このバグの誤動作が是正された結果): 
    `demofiles/memory-leak.c` の未解放件数が 20→10(意図通りの件数)に、
    `demofiles/null-pointer.c` の失敗メッセージが「%s conversion of non-string」(型不一致)から
    「cannot get string from pointer:」(NULLポインタを文字列として読もうとして失敗、より正確)に変化。
  - 追加テスト: `demofiles/pointer-init-no-double-eval.c`、`demofiles/struct-null-pointer-field.c`。

- **`87848bc`** — `main.c`/`main` を `subc/` で再生成し直す軽微な整形(`7ab1dac` と同種)。

### ドキュメント

- **`8bece49`** — `docs/design/` に設計メモを2件追加(バックグラウンド調査エージェントによる並行調査)。
  - `task2-feature-inventory-and-proposal.md`: 現状サポートしているC言語機能/組み込み関数の棚卸しと、
    `realloc()`/`calloc()` を筆頭とする拡張提案。
  - `task3-vm-audit-and-design.md`: `-O` バイトコードVMの監査(実は `main()` の実行に一切使われておらず、
    ASTノードの多くが未実装であることが判明)と、安全性チェックをVM側にも効かせるための設計比較。
  - いずれも設計・提案のみで、`main.leg`/`main.c` への実装は含まない。

- **`dcfe0a6`** — `docs/verification/task1-verification-report.md` を追加。
  - ビルド手順(`leg`/`peg` の非破壊的な入手方法含む)、回帰確認の方法論、`demofiles/*.c` 全件の
    before/after 差分結果、新規テストケースの検証結果を記録。
  - 副産物として発見した既存バグ(Task 1 と無関係)も記録: 最小限の `malloc→free→return` プログラムでも
    「解放し忘れ」が誤検知される問題(原因未特定、`preval`/`eval` の二重評価が疑わしい)。
    → **`db54b89`/`11486ea` で修正済み**(下記参照。実際の原因は `preval`/`eval` の二重評価ではなく
    `initialiseVariable()` の `break;` 漏れだった)。

### Task 5: 自動テストハーネス

- **`b25cc37`** — `if`/`while` の条件式でポインタ型を許可(`for` は既に許可していたのと揃える)。
  - `typeCheck()` の `If`/`While` ケースは条件の型が厳密に `t_int` であることを要求していたが、
    `For` は `t_int != cond && !is(Tpointer, cond)` と既にポインタ型も許可しており、実行時側の
    `toBoolean()`(if/while/for共通)も元々ポインタを問題なく扱えていた。`null-pointer.c` の
    陰性テスト(`if (ptr) ...` でNULLチェック)を書く過程で発覚。`For` と同じ条件チェックに揃えた。
  - 追加テスト(陰性ケース6本): `use-after-free-ok-unused.c`(解放後に触らない)、
    `use-after-free-ok-reassign.c`(解放後すぐ新しい値を再代入してから使う)、
    `multiple-free-ok-distinct.c`(別々のポインタをそれぞれ1回ずつ解放)、
    `dangling-pointer-ok-alive.c`(まだ生きている変数へのポインタ)、
    `null-pointer-ok-checked.c`(使う前にNULLチェック、上記修正に依存)、
    `out-of-bounds-access-ok-inbounds.c`(範囲内アクセスのみ)。

- **`37b7645`** — `demofiles/` を正式な自動テストスイート化。
  - `scripts/run-tests.sh`: 各 `demofiles/<name>.c` に対応する `demofiles/<name>.expect`
    (`EXIT=0|1` + 任意の `CONTAINS=<部分文字列>`)を読み、実行結果と突き合わせて PASS/FAIL 判定。
    `.expect` が無いファイルはスキップ(未整備として扱う、段階的導入が可能)。
  - `make test` を新設(旧 `test` ターゲット、単発の `./main -vv test.txt` は `make smoke` に改名して温存)。
  - 既存の `demofiles/*.c` 全件(元からあるバグデモ+今回追加した修正検証用デモ)に `.expect` を追加。
  - **ハーネス自体の検証**: 修正前バイナリ(`main_orig`)に対して実行し、今回のセッションで直した
    バグに対応する6件がちゃんと FAIL することを確認(それ以外は変化なし)。この過程で
    `pointer-compare-cast.expect` の `CONTAINS=greater` がバグ版の出力 `"not greater"` にも
    部分文字列として一致してしまい**偽陽性(false pass)になっていたことが判明** →
    デモの出力を `GT_YES`/`GT_NO`/`LT_YES`/`LT_NO` という曖昧さのないトークンに変更して修正。

### Task 6: マルチエージェント運用(設計→実装→検証パイプラインの試行)

- `docs/design/task6-multi-agent-workflow.md` — 3役(設計/実装/検証)の分担方法(同一セッション内の
  Agent呼び出し vs worktree分離 vs 別セッション/ブランチ)の比較・推奨、成果物の受け渡し方法、
  レビューチェックポイントの提案。加えて、Task 2 の `realloc()`/`calloc()` 実装を題材に実際に
  パイプラインを一度通しで試行し、うまくいった点・詰まった点を記録。

- **`2f05bc0`** — `realloc()`/`calloc()` を実装(Task 2 提案 #1・#2)。実装エージェントに設計メモ
  (`docs/design/task2-feature-inventory-and-proposal.md`)を渡して実装させ、レビュー時に発見・修正:
  - `realloc()` で確保先ブロックが移動した場合、旧 `Memory` を `free()` と同じ仕組みで
    「解放済み」にマークすることで、`requireNotFreed()` が旧ポインタへの以後のアクセスを
    既存のuse-after-free検出と同じ経路で検出できるようにした(この研究テーマの核心機能)。
  - 移動しなかった場合(GC_reallocが同じアドレスを返す)は既存の `Memory` オブジェクトの
    `size` をその場で書き換えるだけにし、他のエイリアス(ポインタ演算で作った別ポインタ等)を
    無効化しない設計。
  - **副次的に発見・修正したバグ**: `typeCheck()` の仮引数チェックで、`void *pointer` のような
    「唯一の引数ではない `void*` 仮引数」がポインタ修飾子適用前の素の `void` 型と誤認され
    `illegal void parameter` になっていた(`free(void *pointer)` は唯一の引数だったため
    今まで表面化していなかった)。`case Primitive:`(組み込み関数)と `case Function:`
    (ユーザー定義関数)の両方に同じバグがあることをレビューで確認し、両方修正。

- **`e2d2b10`** — realloc/calloc の回帰テスト12本を追加(`demofiles/realloc-*`・`calloc-*`)。
  検証エージェントに Task 5 のハーネス形式で書かせ、レビューで内容を実装コードと突き合わせて確認。
  `./scripts/run-tests.sh` → 39 passed, 0 failed。

- **パイプライン試行で分かったこと**(詳細は `docs/design/task6-multi-agent-workflow.md` 参照):
  実装エージェントへの設計メモが具体的(正確な関数名・疑似コード・エッジケース列挙)であれば
  ほぼ手戻りなく実装できる一方、既に根本原因まで特定済みの小さい修正(Task 1のisfreeの件など)を
  あえて3役に分割するのはコンテキスト再構築のコストが見合わない。検証エージェントはAPIエラーで
  タスク完了前に中断したため、最終レポートはオーケストレーター(このセッション)が
  実装・テストファイルをレビューした上で代わりに作成した。

### 調査のみ(未修正)— パーサ側(AST生成)の監査

バックグラウンド調査エージェントによる `main.leg` の文法規則(セマンティックアクション)監査。
演算子優先順位・結合則・コンストラクタ引数の対応は全て正しいことを確認。実装ミスは見つからず、
以下は「あえて実装されていない/型名を静的に判別する仕組みが無い」ことに起因する既知の曖昧性:

1. **(要修正・実害あり、直接確認済み)** `(f)(3)` のような「括弧で囲んだ関数名経由の呼び出し」が、
   `f` を型名として解釈する `cast` 規則に奪われ `identifier 'f' does not name a type` で fatal する
   (`main.leg` の `unary` 規則で `cast | postfix` の順に試すため。`tname` の最終候補が任意の識別子を
   無条件に受理してしまうことが根本原因)。`cast | postfix` を `postfix | cast` に入れ替えることで
   builtin型・struct型へのキャストを壊さずに直せる見込みだが、`typedef` した型名へのキャスト
   (`(myint)x` 形式)は同じ曖昧性が残るため、根本対応には型名テーブル追跡(いわゆる lexer hack)が必要。
2. **(要修正・実害あり、直接確認済み)** `sizeof(myint)`(`myint` が `typedef` された型名)が
   `cannot typecheck value of type TypeName` で fatal する。文法上の順序問題ではなく、
   `typeCheck()` の `Sizeof` ケースが「解決したら `TypeName` だった」場合を想定していないことが原因。
3. (優先度低・意図的な簡略化の可能性あり) 先頭が `0` の整数リテラル(`010` など)が8進数として
   解釈されず10進数(10)になる。文字/文字列リテラル内の `\0nn` 8進エスケープは正しく処理されている。

上記1・2は未修正。対応する場合は別コミットとして切り出す想定。

### 検証方法(共通)

各コミットについて、`leg` で `main.leg` から `main.c` を再生成 → `gcc -std=c99 -Werror -Wall -Wno-unused -g` で
ビルド → `demofiles/*.c` 全件を実行し、修正前の出力(非決定的なヒープアドレスを除く)との差分が
意図した変更のみであることを確認した。

## 2026-08-23

### Task 2 続き: `demofiles/` の境界値カバレッジ拡充と、その過程で見つかった基礎的なバグ

- **`18cbe5e`** — 3件の関連バグを修正(境界値テストの拡充作業中に発見):
  1. `prim_malloc` の負サイズチェックが tautology(常に真)になっていた。
     `size_t size = _integerValue(arg)` で符号なし型に変換した**後**に `size >= 0` を
     チェックしていたため、このチェックは意味を成していなかった。実際には別の
     10MB上限チェックが偶然(符号あり→符号なし変換でラップアラウンドした巨大な値が
     上限を超えるため)`malloc(-1)` を弾いていただけだった。符号なしに変換する前の
     `long` の時点で符号をチェックするよう修正(`calloc` 側は元々正しく実装されていた)。
  2. `converter()` の型変換テーブルに `char`/`short` への変換が一切登録されておらず、
     **`char c = 'a';` という最も基本的なコードすら** `cannot convert 'int' to 'char'`
     で fatal していた(文字リテラルは実際のC言語仕様通り `int` 型としてパースされるため)。
     `int->char`・`int->short` を追加(実際のメモリへの書き込み時点で `setMemory` が
     C言語のセマンティクス通り正しく切り詰めるため、型チェック側で変換を許可するだけで良い)。
  3. `typeCheck()` の `case Assign:`(宣言時の初期化子ではなく、`x = y;` という**代入文**)は
     型が完全一致するかポインタ互換でない限り一切の数値変換を許可していなかった
     (`int x; long n=5; x = n;` すら fatal していた)。`VarDecls` の初期化子と同じく
     `converter()` にフォールバックするよう修正。
  - この3つにより、`char` 型の最も基本的な使い方(宣言・再代入・バッファへの1文字ずつの
    書き込み)が初めて動くようになった。
  - 追加テスト: `char-literal-assignment-ok.c`(修正前バイナリで実際に fatal することを確認済み)。

- **`1c4a1ba`** — 境界値テスト6本を追加。
  - `heap-array-oob.c`/`heap-array-ok-inbounds.c`: 既存のOOBデモ4本は全てスタック配列・
    変数のみを対象にしていたため、`malloc()` したヒープ配列への範囲外アクセスを追加。
  - `malloc-negative-size.c`: 上記1の修正を検証。
  - `char-buffer-oob.c`: `char` バッファへの手動インデックスでのオーバーフロー
    (上記2・3の修正があって初めて書けるようになったテスト)。
  - `pointer-arithmetic-overflow.c`: 既存のポインタOOBデモは1〜6要素分のズレしか
    見ていなかったため、100万要素分ズレたポインタ演算でも同じ境界チェックが効くことを確認。
  - `./scripts/run-tests.sh` → 45 passed, 0 failed(修正前は39)。

- **見つけたが今回は直していない既知の欠落**(`docs/design/task2-feature-inventory-and-proposal.md`
  が「要確認」としていた項目の答え合わせで発覚): 多次元配列(`int a[3][4]; a[i][j]`)と、
  構造体メンバの固定長配列(`struct S { int buf[4]; }; s.buf[i]`)は現状どちらも
  `cannot load 'Tarray' from array/memory` で失敗する。`getArray()`/`getMemory()` が
  「要素型がさらに配列」というケースを一切扱っておらず、`Array` 構造体自体に
  `Pointer` の `offset` に相当するフィールドが無い(=「より大きなメモリブロックの
  一部を指す配列ビュー」を表現できない)ことが根本原因。`Array` に offset を追加し、
  `getArray`/`setArray`/`getMemory`/`setMemory`/`initialiseVariable`/`declareTag` 等
  複数箇所に波及する中規模のアーキテクチャ変更が必要なため、今回はテスト追加止まりの
  作業とは切り離し、別途着手することとした。

### Task 2 続き: 複合代入演算子・`switch`/`case`/`default`(#5・#6)

- **`28d7666`** — 複合代入演算子(`+= -= *= /= %= &= |= ^= <<= >>=`)を追加。
  `l OP= r` をパース時に `newAssign(l, newBinary(OP, l, r, t), t)` へ脱糖するだけで、
  新しいASTノードも評価器の変更も不要(ただし副作用のあるlvalue、例えば `arr[i++] += 1` は
  lvalueの部分木を2回評価してしまうため副作用も2回起きる、という一般的な脱糖方式の
  既知の制限は残る)。
  - **副次的に発見**: `typeCheck()` の `Binary` ケースで `%`・`&`・`^`・`|`・`<=`・`>=`・
    `&&`・`||` の8個の演算子が `assert(!"unimplemented")` のまま放置されており、
    複合代入とは無関係に**普通の `x % 4` や `a <= b` ですら常にクラッシュしていた**
    (修正前バイナリでも再現、今回のセッションの変更とは無関係の既存バグ)。
    `%` は `*`/`/` と同じ int/long/float/double の厳密一致チェックに、残り7個は
    既存の `<`/`>`/`==`/`!=`/`<<`/`>>` と同じ緩い `t_int` 固定スタブに合わせて修正。
  - 追加テスト: `compound-assignment-ok.c`、`modulo-and-bitwise-ok.c`。
  - `./scripts/run-tests.sh` → 47 passed, 0 failed(修正前は45)。

- **`710ee68`** — `switch`/`case`/`default` を追加。
  - `Switch{condition,statements}` の `statements` は `Case`/`Default` を目印として
    含んだ**フラットな**文リストとして表現(`Block` と同じ発想)。評価時は条件を1回だけ
    評価し、一致する `Case`(無ければ `Default`)を探してその位置から順に実行するだけで、
    C言語の「フォールスルー」がそのまま実現できる。`break` は既存の `NLR_BREAK` 機構
    (`while`/`for` と同じ)で switch だけを抜け、`continue`/`return` はあえて
    catchせずそのまま外側(直近のループ/関数)へ伝播させている(switch内の`continue`は
    switchではなくループを継続させる、という実際のC言語仕様通りの挙動)。
  - **副次的に発見**: `typeCheck()` に `Continue`/`Break` のケースが一切無く、
    `while (1) { if (x) break; }` という最も基本的なコードすら
    `cannot convert Break to string` でクラッシュしていた(修正前バイナリでも再現、
    今回のセッションの変更とは無関係の既存バグ)。`typeCheck()` に
    `case Continue: return nil;`・`case Break: return nil;` を追加して修正。
  - 新しい列挙値(`Switch`/`Case`/`Default`)は `_do_types` マクロの**末尾**に追加。
    途中に挿入すると既存の全型の列挙値が繰り下がり、`demofiles/*.c` のデバッグ出力
    (`basetype:N`/`vartype:N`)がことごとく変化してしまうことに回帰確認で気付いたため。
  - 追加テスト: `switch-ok.c`(フォールスルー・複数case・break/continueの scope の違いを確認)、
    `break-in-if-ok.c`。
  - `./scripts/run-tests.sh` → 49 passed, 0 failed(修正前は47)。

Task 2 の残り(三項演算子 `?:`、`enum`/`union`)は優先度が低い項目として今回は見送り。
