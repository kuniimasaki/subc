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

### Task 3: VM(`-O`)にポインタ安全性フックを実装

`docs/design/task3-vm-audit-and-design.md` が推奨した Option B(ポインタ操作を新規
opcodeではなく既存の `iCALL`/`Primitive` 経由のプリミティブ呼び出しに脱糖し、
`requireNotFreed()` に到達する既存の `getPointer`/`setMemory` をそのまま再利用する)を実装。

- **`ee48d5b`** — Task 3 の第一歩: `-O` が実は `main()` を一切実行していなかった
  (常にツリーウォーカーの `apply()` が使われ、`-O` の唯一の効果だった単発トップレベル式の
  コンパイルも `return` 文で即クラッシュしていた)という根本的なギャップを解消。
  `compileOn` の `Return` ケース実装、`Function` ケースが自分自身に対して
  `typeCheck()` を実行するように変更(名前・引数・戻り値型の解決と scope への登録が
  `-O` では一切行われていなかったため)、コンパイル済みクロージャを呼び出し名の
  `Symbol.value` に束縛(`iGETGVAR` が実際に読む場所)、`iCALL` のパラメータ束縛が
  生の `Variable` オブジェクトをキーにしていた(`iGETGVAR`/`iSETGVAR` が期待する
  素の `Symbol` とは別物で、関数が自分の引数を一切参照できなかった)バグの修正、
  インタプリタ本体の C `main()` が `-O` 時に `apply()` ではなく `iCALL`/`iRETURN`
  経由で `main()` を実行するように変更。

- **`43872d8`** — ポインタの読み書きを VM 経由でも安全にチェックできるようにする本題。
  - `Dereference`/`Index`(値を読む側)と、ポインタ/添字型の `Assign`(値を書く側)を、
    ツリーウォーカーの `getPointer`/`setMemory`/`getArray`/`setArray`/`setPointer` を
    そのまま呼ぶだけの新規プリミティブ(`prim_vm_deref` 等、ユーザーコードから直接は
    呼べない内部専用)への `Call` にコンパイルするよう変更。新しい opcode は
    ローカル変数導入用の `iDECL` 1個のみ(安全性チェック自体には新opcodeを一切追加していない)。
  - `iGETGVAR` が `Symbol.value`(VMの独自グローバル領域)しか見ておらず、
    `declare()` で登録される側(`malloc`/`free` などの組み込み関数、および上記で
    `-O` コンパイル時に登録されるようになったユーザー関数)を一切見つけられなかった
    ため、`Scope_lookup()` へのフォールバックを追加。
  - `#include <stdlib.h>` から生成される `extern` 宣言(`Primitive` ノード)も、
    `Function` と同様「自分自身に `typeCheck()` を実行してから」でないと
    `_do_primitives` によるC関数ポインタの束縛も `declare()` も一切されないことが判明し、修正。
  - **副次的に発見**: `malloc()` の戻り値(`void*`)をキャストせずに
    `int *p = malloc(...);` と書くと、ツリーウォーカー側でも
    `cannot convert non-NULL pointer 'void *' to 'int *'` で fatal していた
    (今回のVM作業とは無関係の既存バグ、直前のコミットの時点で既に再現することを確認)。
    `initialiseVariable()` の該当チェックが「void* かつ NULL」の場合しか暗黙変換を
    許可しておらず、非NULLの void* → 任意ポインタ型という(実際のCでは合法な)
    暗黙変換を弾いていたのが原因。既存デモが全て明示キャストを書いていたため
    今まで表面化していなかった。修正。
  - **確認できたこと**: `malloc→*p=42→v=*p→free(p)` が `-O` 経由で正しく `42` を返し、
    かつ「`free()` 後に読む」バージョンはツリーウォーカーの `use-after-free.c` と
    **全く同じメッセージ**で検出されることを確認 —— Task 3 の研究目標
    (メモリ安全性チェックがVM経由でも効く)がここで実際に達成された。
  - 追加テスト: `vm-pointer-roundtrip-ok.c`、`vm-use-after-free.c`(修正前バイナリで
    実際にクラッシュ(SIGABRT)することを確認済み)。
  - `./scripts/run-tests.sh` → 52 passed, 0 failed(修正前は50)。

引き続き未実装(`docs/design/task3-vm-audit-and-design.md` が「大規模」と評価した通り):
構造体・配列リテラル・`for`ループ・キャスト・多くの宣言型など。今回実装したのは
「ポインタの読み書きが安全性チェック付きでVM経由で動く」という核心部分のみ。

### VM(`-O`)の実装状況を実測・文書化(コード変更なし、調査のみ)

- Task 3 完了時点で「具体的にどこまで `-O` が動くか」を可視化するため、`compileOn()` の
  全ASTノード種別(`_do_types` 55種)を読み、`demofiles/*.c` 全52本 + `mydemo/*.c` 全10本
  (計62本)を素朴に `./main -O <file>` で実行して分類した。
  - 最後まで実行できたもの: 15/62。残り47本は `compileOn()` の `assert(!"unimplemented")`
    に当たって `SIGABRT`(終了コード134)。**未実装ノードにヒットした場合はツリーウォーカーの
    `fatal()` のような行儀の良いエラーではなくインタプリタ自体がクラッシュする**ため、
    「バグを検出した」のか「VMがコンパイルできず落ちた」のかを区別する必要がある。
  - クラッシュ原因の内訳(頻度順): `Cast`(明示的ポインタキャスト、malloc系テストの大半が該当、
    最大のボトルネック)、`For` ループ、`Addressof`(`&x`)、`++`/`--`、配列初期化子リスト
    (`{1,2,3}` の `List` ノード)、`Switch`、`Member`(構造体フィールド)、`LAND`/`LOR`
    (`&&`/`||`)、トップレベルの `TypeDecls`。
  - **追加で発見した既存バグ**(クラッシュではなく誤動作): 初期化式の無いローカル配列/構造体
    宣言(`int a[3];`)が、`VarDecls` 実装が使う `prim_vm_coerce` がポインタ型しか
    特別扱いしないため実体を確保されず `nil` にバインドされる。`a[0]=1;` は
    `cannot index-assign type Undefined` で fatal する(ツリーウォーカー側は正常動作)。
  - `make test`(ツリーウォーカー側、`-O` 無し)は 52 passed, 0 failed のまま変化なし
    (今回はコードを一切変更していない調査のみ)。
- 結果を `docs/design/vm-implementation-status.md` にまとめた。ノード別の実装/未実装表、
  クラッシュ実測結果、優先度付き残作業(`Cast` → `For` → `Addressof`/`++`/`--` →
  配列初期化子・ローカル配列実体確保 → `Switch`/`Member`/`LAND`/`LOR`/`TypeDecls` の順)を記載。
  今後 `compileOn()` に変更を加えた際はこの文書も更新する運用とする。

### VM(`-O`)の実装状況調査で見つかったクラッシュ原因を全て実装(15/62 → 68/68 完走)

上記の優先度リストに従い、`compileOn()` の未実装ノードを頻度順に全て実装。各コミットで
`make test`(ツリーウォーカー側)の無回帰を確認しつつ進めた。

- **`c741b4b`** — 実装作業の前に発見した、クラッシュではなく無限ループする静かなバグを先に修正。
  `execute()` の `iJMPF` が条件の偽値を `nil` とだけ比較していたが、`false` の実体は
  `newInteger(0)`(`nil` とは別オブジェクト)。比較・論理演算子は必ず `true`/`false` を
  積むため `nil == cond` は事実上一度も成立せず、`while` ループは本体が1回でも実行されると
  無限ループしていた(`assert(!"unimplemented")` の検知網には引っかからない)。
  `isFalse(cond)` を使うよう修正。`if` の false 分岐も同じ理由で機能していなかったため
  同時に直った。追加テスト: `vm-while-loop-ok.c`、`vm-if-false-branch-ok.c`。

- **`9a3e8fd`** — `Cast`・`For`・`Addressof`・`++`/`--`・`&&`/`||` を実装。
  - `Cast`: `Cast,converter`(typeCheckが確定させる生のC関数ポインタ)はVMのスタックに
    乗せられないため、値ではなく `Cast` ノード自身を第2引数として新設の `prim_vm_cast` に
    渡し、`eval()` のCastケースを完全に踏襲。
  - `For`: 既存の `While` と同じスタック管理に、`continue` が本体をスキップして到達すべき
    「更新式(step)」という別ステージを追加。
  - `Addressof`(`&x`/`&a[i]`): 単なるプリミティブ追加では済まず、VMのローカル変数表現を
    変更する必要があった。それまでVMの `env` は素の `(symbol . value)` alist で、
    ツリーウォーカーの `Variable` オブジェクトに相当する「アドレスを取れる箱」が
    VMのローカルには存在しなかった。`iDECL` と `iCALL` のパラメータ束縛を、値を直接
    consするのではなく毎回フレッシュな `Variable` オブジェクトでラップするように変更
    (再帰呼び出し同士が互いを壊さないよう、呼び出しごとに新規生成)、`iGETGVAR`/
    `iSETGVAR` も `Variable,value` 経由に変更。これにより `getPointer`/`setPointer` の
    既存の `Variable` ケースがVMのローカルにも無変更で効くようになった。
    既知の未対応: ツリーウォーカーの `Scope_end()` によるローカル変数の生存管理
    (`isdead` マーキング)に相当する仕組みがVMには無いため、`dangling-pointer.c` の
    「関数を抜けた後のローカル変数アドレス使用」は `-O` では検出されない
    (`docs/design/vm-implementation-status.md` に既知の残課題として記載)。
  - `++`/`--`: `&x` と同じ「シンボルを評価せず生のまま渡す」トリックで実装。
  - `&&`/`||`: 短絡評価(JMPF/JMPによる制御フロー)。`eval()` と同じセマンティクス
    (`a && b` は `false` または b の**生の値**、`a || b` は `true` または b の生の値、
    どちらも真偽値へのキャストはしない)。
  - **副次的に発見・修正した静かなバグ**: `execute()` の `iADD`/`iLT`/`iLE`/`iGE`/`iGT`/
    `iEQ`/`iNE` が `eval()` のBinaryケースと違いポインタ/配列を一切考慮せず常に
    `integerValue()` に丸投げしていたため、`assert(ptr != 0)` のようなごく普通のNULL
    チェックが「cannot convert Pointer to integer」で fatal していた(Addressofの
    実装で初めて踏めるようになったコードパスで発覚、gdbで追跡)。`iADD` はポインタ/
    配列+整数のポインタ演算に対応、比較演算子は `compare()`/`equal()` 経由に修正。
  - もう一つの副次バグ: 自分で書いた `LAND` の初回実装が `While` の「JMPF の後に
    もう一つpop」という形をそのまま真似てしまい、`iJMPF` 自体が既に条件値をpopする
    ことを見落として二重popになっていた(`if (a && b)` が即座に "stack underflow")。
    コミット前のテストで発覚、余分なpopを削除して修正。
  - 追加テスト: `vm-cast-ok.c`、`vm-for-loop-ok.c`、`vm-addressof-ok.c`、
    `vm-increment-ok.c`、`vm-logical-ops-ok.c`、`vm-pointer-arithmetic-ok.c`。
  - `./scripts/run-tests.sh` → 60 passed, 0 failed(修正前は55)。

- **`37142c5`** — ローカル配列/構造体の実体確保、配列初期化子リスト、`Member`
  (`s.field`)を実装。
  - 初期化式の無い `int a[3];` が `nil` にバインドされる(調査で発見済みの)バグを、
    `VarDecls` を宣言型で分岐させて修正。`Tarray`/`Tstruct` は新設の
    `prim_vm_alloc_array`/`prim_vm_alloc_struct`(`initialiseVariable()` のTarray/
    Tstructケースの移植: `CALLOC`+`newMemory`+`newArray`/`newStruct`、初期化子が
    無ければ `randomise()` で「未初期化変数」検出も再現)を通し、それ以外
    (ポインタ・数値型)は従来通り `prim_vm_coerce`。
  - 配列初期化子(`{1,2,3}`、文字列リテラル)は各要素式(または各文字)をコンパイル時に
    展開して `prim_vm_alloc_array` に渡す。
  - `Member`: 新設の `prim_vm_member`/`prim_vm_store_member` は `eval()`/`assign()` の
    Memberケースの素直な移植。
  - **副次的に発見・修正したバグ**: ファイルスコープでの構造体宣言
    (`struct Point { int x, y; };`)を含むテストで発覚。`replFile` の `-O` 経路は
    トップレベルの各構文要素を `typeCheck()`/`preval()` に一切通さず直接
    コンパイル+実行する(`Function`/`Primitive` は `compileOn` 内で自己
    `typeCheck()` して補っていたが、素の `VarDecls` にはその補完が無かった)。
    タグ宣言のみの `struct Point {...};` は構文上「変数名の無い宣言子」
    (パーサの `nil` プレースホルダ)を1つ持つ `VarDecls` になり、typeCheckが
    それを捨てて `declareTag()` で構造体タグを登録するという処理が一度も
    走らないまま `compileOn` に到達し `expected Variable, got Undefined` で
    fatal していた。「まだ型解決されていないか」を「先頭要素が `Variable` に
    なっていないか」で判定し、必要なら自身に `typeCheck(exp, nil)` を実行してから
    処理を続けるよう修正(タグ宣言のみで実変数が0個になるケースと、無限に
    再typeCheckし続けてしまうケースを区別できるよう、判定条件を慎重に設計)。
  - 追加テスト: `vm-local-array-ok.c`、`vm-struct-member-ok.c`。
  - `./scripts/run-tests.sh` → 62 passed, 0 failed(修正前は60)。

- **`d1d35aa`** — `Switch`/`Case`/`Default` とトップレベル `TypeDecls` を実装。
  これで `compileOn()` の未実装ノードによるクラッシュは(今回の調査範囲では)ゼロになった。
  - `Switch`: 条件式を一度だけ評価して隠しローカル変数(`"<switch-cond>"`、識別子として
    構文的に出現し得ない名前)に格納し、各 `Case` の値との比較チェーン
    (`iGETGVAR`+`iEQ`+`iJMPF`)で一致した本体位置へジャンプ。本体はフラットな
    文リストとしてそのまま実行(C言語のフォールスルーがそのまま実現される)。
    `continue` はswitchで捕まえず外側のループへ伝播、`break` は新しい `breaks`
    リストで捕まえる(While/Forと同じ)。
    **副次的に発見・修正したバグ**: 初回実装は `break` で終わる switch を1回
    呼ぶたびにスタックが1個ずつリークした(`switch-ok.c` の
    `classify()`/`fallthrough()` は全分岐が `break` で終わるため、複数回呼ぶと
    "N items on stack at end of execution" で発覚)。`break` は自分の値をpushして
    から脱出ラベルへジャンプするが、脱出ラベルの直後に「フォールスルーで自然に
    文末へ到達した場合」用のpushが無条件で置かれていたため、break経由の到達だと
    二重にpushされていた。3つの脱出経路(break/`Default`無しでの不一致/自然な
    文末到達)がそれぞれ厳密に1回だけpushするよう修正。
  - `TypeDecls`(`typedef long intptr_t;` 等、`#include <stdint.h>` 経由も含む):
    `VarDecls` と同じ「トップレベルでは自己typeCheckが必要」というギャップ。
    同じ判定手法(先頭要素が `TypeName` になっているか)で修正。
  - 追加テスト: `vm-switch-ok.c`(フォールスルー・break・switch内のcontinueが
    外側whileに伝播することを確認)。
  - `./scripts/run-tests.sh` → 63 passed, 0 failed(修正前は62)。
  - `demofiles/*.c`(63本)+`mydemo/*.c`(10本)、計73本を `-O` で実行する非公式な
    網羅性スイープでは全73本が最後まで実行完了(調査開始時点は15/62)。

- **既知の残課題**(`docs/design/vm-implementation-status.md` に詳細)、いずれも
  「クラッシュしない」という今回の目標は達成しつつ、意図的に深追いしなかったもの:
  VMにスコープ終了処理が無く `dangling-pointer.c` のようなバグは `-O` では検出されない、
  VMの値/フレームスタックが固定32要素で `nfib(32)` 相当の深い再帰は `stack overflow`
  する(`nfib(8)=67` は正しく計算できることを確認済み)、`typedef` を挟んだ複雑な
  キャスト連鎖(`invalid-pointer.c`)とビットレベルのfloat/int型パニング
  (`fisr.c`)は数値的な正確性まで踏み込めていない。

## 2026-08-24

### VM(`-O`)にスコープ終了処理(dangling pointer検出)を実装

前日の残課題1件目に対応。ツリーウォーカーの `Scope_end()`/`kill_scpp()`
(関数を抜けるたびにそのスコープの `Variable` を `isdead=1` にマークする仕組み)に
相当する処理をVMにも実装し、`&localvar` が関数の外へ漏れて後から参照される
バグ(`demofiles/dangling-pointer.c`)が `-O` 経由でも検出されるようにした。

- VMの `env` はツリーウォーカーの `scopes`(`Scope_begin`/`Scope_end` で管理される
  入れ子リスト)とは全く別の、呼び出しごとに `iDECL`/パラメータ束縛で積み上がる
  だけの alist なので、「関数呼び出しが終わったら、その呼び出しで束縛された
  ローカルだけを殺す」処理をVM独自に実装する必要があった。
  - `struct Frame` に `baseEnv`(そのクロージャが捕捉していた、パラメータ束縛前の
    環境)を追加。`iCALL` のClosure分岐で、パラメータを束縛する**前**の
    `environment` を保存しておく。
  - `iRETURN` は現在の `env` から `baseEnv` に到達するまで環境チェーンを辿り、
    見つけた全ての `Variable`(パラメータ+`iDECL`されたローカル)を `isdead=1`
    にする。各呼び出しの `baseEnv` は(クロージャ捕捉時点の環境なので)独立して
    いるため、再帰呼び出しで外側の呼び出しのローカルを誤って殺すことはない
    (`nfib(8)` が引き続き正しく `67` を返すことを確認)。
- **副次的に発見・修正したバグ**: `prim_vm_store_deref`(`*p = rhs` のVM実装、
  Task 3で追加)が `assign()` のDereferenceケースが持つ `isdead` チェックを
  丸ごと欠いていた(Task 3時点でのコピー漏れ、今回の作業とは無関係の既存バグ)。
  上記のスコープ終了処理を実装しても、この書き込み側のチェックが無ければ
  検出は発火しないため発覚。`assign()` と同じチェック
  (`"Var '%s' that you tried to deref is already dead "`)を追加して修正。
- 追加テスト: `vm-dangling-pointer.c`(`demofiles/dangling-pointer.c` と同じバグを
  `-O` で実行、`EXIT=1`+`already dead` を確認)、
  `vm-dangling-pointer-ok-alive.c`(陰性ケース: `&i` を取ったローカル変数がまだ
  呼び出しスタック上にある間に別関数へ渡して使う、`touch()` が戻っても呼び出し元
  `main()` のローカルまで誤って殺されないことを確認)。
- `./scripts/run-tests.sh` → 65 passed, 0 failed(修正前は63)。
  `demofiles/*.c`(65本)+`mydemo/*.c`(10本)、計75本の `-O` 網羅性スイープは
  全75本が引き続きクラッシュなく完走。
- `docs/design/vm-implementation-status.md` を更新(既知の残課題からこの項目を削除、
  実装の勘所に追記)。

### VM(`-O`)の値/フレームスタックを動的に伸びる構造に変更

前日の残課題2件目に対応。`oop stack[32]`/`struct Frame frames[32]` という固定長配列を、
`MALLOC`+`memcpy` で倍々に伸びるヒープバッファに置き換えた。

- どちらも「先頭(index 0)側が満杯、`sp`/`fp` は容量から数えた残り」という
  **下向きに詰まる**方式だったため、単純な `REALLOC`(末尾側に空きを増やす)では
  既存要素の位置がズレてしまう。共通ヘルパー `growDownArray()` を新設し、新しい
  (倍容量の)バッファを確保して現在有効な要素を**新バッファの上端**に詰め直してから
  `sp`/`fp` を新容量基準で振り直すことで、`stack[sp]`・`stack+sp`(プリミティブ呼び出し
  時の引数配列渡しに使用)・`frames[fp]` など既存の相対参照ロジックを一切変更せずに
  住み替えられるようにした。`push()` は満杯(`sp==0`)を検知したら伸長してから積む、
  `iCALL` のクロージャ分岐(関数呼び出し)も同様に `fp==0` で伸長してからフレームを積む。
  `iHALT` の「実行終了時にスタックに残っているべき項目数」チェックも、ハードコードされた
  `32` ではなく `stackCap - sp` という一般化した式に変更。
- 確認: 深さ5000の線形再帰(`depth(5000)`)が正しく動作、`nfib(32)`
  (以前は深さ32でスタックオーバーフローしていたケース)が完走することを確認
  (`nfib(8)=67` の回帰も確認)。
- 追加テスト: `vm-deep-recursion-ok.c`(`depth(5000) % 256` が `-O` 経由で正しく `136`
  になることを確認)。
- `./scripts/run-tests.sh` → 66 passed, 0 failed(修正前は65)。
  `demofiles/*.c`(66本)+`mydemo/*.c`(10本)、計76本の `-O` 網羅性スイープは
  全76本が引き続きクラッシュなく完走。
- `docs/design/vm-implementation-status.md` を更新(既知の残課題からこの項目を削除、
  実装の勘所に追記)。

### VM(`-O`)にtypedefを挟んだキャスト連鎖(`invalid-pointer.c`)の検出漏れを修正

残課題3件目に対応。`ptr = (int *)(intptr_t)0xDeadD0d0;` のような、非ゼロ整数を
ポインタ型変数へ再代入するキャストが `-O` 経由だと `Pointer` にならず生の
`Integer` のままになり、後続の `printf("%p", ...)` が `%p conversion of
non-pointer: Integer` で(ツリーウォーカーの「不正な書き込み」検出とは
無関係な理由で)止まっていたバグを修正。

- 調査の結果、キャスト自体(`prim_vm_cast`)はツリーウォーカーの `eval()` の
  Castケースと寸分違わず同じロジックで、`(int *)(long)N` の変換テーブル
  エントリが恒等関数 `cvt_` である(=キャストだけでは `Integer` のまま)のは
  **両方の経路で意図された挙動**と判明。ツリーウォーカー側が実際に検出できて
  いたのは、`assign()` の `case Variable:`(再代入)が「代入先がポインタ型
  なら、右辺の裸の `Integer`/`Array`/`String`/型違いの `Pointer` を無条件で
  `Pointer{base=...}` に包む」という変換を毎回行っていたから。VM側の
  `compileOn` の `Assign` `default:`(裸シンボルへの代入)はただの `iSETGVAR`
  で、この変換を一切行っていなかった(宣言時の初期化子だけを扱う
  `prim_vm_coerce` とは別の、再代入専用の変換ロジックが丸ごと欠けていた)。
- `assign()` の `case Variable:` から変換ロジックを `coerceAssignedValue()`
  として切り出し、新設の `prim_vm_store_symbol`(裸シンボルへの代入を
  `iGETGVAR`/`iSETGVAR` と全く同じ手順(`env` alist → `Scope_lookup` →
  最後の手段として素の `Symbol,value`)でVariableボックスを解決してから
  同じ変換をかける)と両方から呼ぶように統一。ツリーウォーカーとVMが同じ
  1本のロジックを共有するようになったため、今後この種の乖離が再発しにくい。
- **副次的に発見**: `prim_vm_store_deref`(`*p = rhs`)の `switch(getType(base))`
  に `case Integer:`(`(void*)(intptr_t)N` のような、変数でもメモリブロック
  でもない整数をbaseに持つポインタ)が無く、汎用の `"cannot store through
  pointer"` に落ちていた。`assign()` のDereferenceケースが持つ専用メッセージ
  (`"attempt to store into arbitrary memory location"`)を追加してツリーウォーカー
  と一致させた。
- 追加テスト: `vm-cast-chain-invalid-pointer.c`(`demofiles/invalid-pointer.c`
  の後半と同じキャスト連鎖を `-O` で実行し、ツリーウォーカーと全く同じ
  メッセージで検出されることを確認)。
- `./scripts/run-tests.sh` → 67 passed, 0 failed(修正前は66)。
  `demofiles/*.c`(67本)+`mydemo/*.c`(10本)、計77本の `-O` 網羅性スイープは
  全77本が引き続きクラッシュなく完走。`assign()` を触ったため、ツリーウォーカー側
  (`-O` 無し)の全回帰(既存67テスト中の非VMテスト)も無変化であることを確認。
- `docs/design/vm-implementation-status.md` を更新(既知の残課題からこの項目を削除、
  実装の勘所に追記)。残るは「`fisr.c` の数値精度」1件のみ。

### `mydemo/fisr.c` の数値精度問題を調査(コード変更なし)、`compileOn()` の残り棚卸し

- **`fisr.c` 調査**: `-O` 無し・`-O` あり双方で `Q_rsqrt(100.0)` が同一の
  誤ったガベージ値を返すことを確認。**VM固有の遅れではなく、ツリーウォーカー
  側にも同じ形で存在する既存の制限**と判明(`i = *(long *)&y;` のような
  ビットレベルのfloat/int型パニングを、ポインタの参照先が「変数」の場合に
  `getPointer`/`setMemory` が一切実装していないため)。修正には
  ツリーウォーカー・VM両方の意味論に影響する変更が必要なため、ユーザーに
  確認の上、対応不要な既知の制限として `docs/design/vm-implementation-status.md`
  に記録するに留めた(コード変更なし)。
- **`compileOn()` に残っていた `assert(!"unimplemented")` の棚卸し**:
  `Pointer`/`Array`/`Struct`/`List`/`Memory`/`Reference`/`Tvoid`..`Tetc`
  (12種)/`Scope`/`TypeName`/`Variable`/`Constant` について、`compileOn()`
  内の再帰呼び出し箇所を1つ残らず洗い出し、これらの型のASTノードは
  パーサが構築することも、typeCheck が生成することも無く、
  `compileOn()` に渡ることが構造的に無いことを確認。
  `assert(!"unimplemented")`(未実装、というニュアンス)を
  `assert(!"this cannot happen")`(到達不可能と確認済み、というニュアンス。
  既存の `Token`/`Scope` と同じ表現)に変更。挙動は変わらない
  (元々どちらも到達すればクラッシュする)、意味の正確化のみ。
  `./scripts/run-tests.sh` → 67 passed, 0 failed(無変化)。`-O` 網羅性
  スイープも全77本クラッシュなし(無変化)。

### ツリーウォーカー vs VM の簡易ベンチマーク、`make testvm`/`demovm` の新設

- **ベンチマーク**: 再帰呼び出しが多い処理(`nfib(28)`、約100万回の関数呼び出し)で
  ツリーウォーカー 3.58秒 → VM(`-O`)0.92秒、**約3.9倍高速**。
- **`scripts/run-tests.sh`** に `FORCE_FLAGS` 環境変数を追加。設定すると各
  `.expect` の `FLAGS=` を無視して全テストを指定フラグ(例 `-O`)で強制実行できる。
  **`Makefile`** に `testvm`(`FORCE_FLAGS=-O ./scripts/run-tests.sh`、
  `demofiles/*.c` 全67本を強制的にVM経由で実行し、通常の `.expect` 判定基準
  (終了コード・検出メッセージ)と完全一致するか検証)と `demovm`
  (`demofiles/*.c`+`mydemo/*.c` を `-O` で一括実行する目視確認用、`demo`/`demov`
  のVM版)を追加。
- **`make testvm` で新たに2件のメッセージ不一致を発見・修正**(いずれも
  「検出はできているが、ツリーウォーカーとメッセージが違う」ため、それまでの
  クラッシュ有無だけを見る網羅性スイープでは見つからなかったもの):
  1. `prim_vm_store_deref` の `case Memory:` が `setMemory()` に丸投げしていたため、
     ポインタ演算で範囲外に出た書き込み(`pointer-out-of-bounds.c`、
     `pointer-arithmetic-overflow.c`)が `"memory offset out of bounds"`
     (`setMemory()` 自身の境界チェック)で止まっていた。ツリーウォーカーの
     `assign()` のDereference/Memoryケースが持つ、より早い段階での独自の境界
     チェック(`"assigning to out-of-bounds pointer"`)を追加。
  2. `iGETGVAR` がローカル変数を読むとき `nil`(未代入)かどうかを一切チェック
     しておらず、`int a, b = a;` が `-O` だと何の検出もせず正常終了していた
     (`demofiles/uninitialised.c`)。ツリーウォーカーの `eval()` の `Symbol`
     ケースが持つ `"use of uninitialised variable"` チェックを追加。
  - `make testvm` → **67 passed, 0 failed**(修正前は3件failed)。`demofiles/*.c`
    **全67本**が、通常はツリーウォーカーで実行される53本も含めて強制的にVM経由で
    実行しても完全に一致することを確認 —— 「クラッシュしない」だけでなく
    「ツリーウォーカーと全く同じ結果になる」ところまで検証できたことになる。
  - `docs/design/vm-implementation-status.md` を更新(この検証結果と2件の修正を記録)。

### VMの重大な性能バグ(ループ本体内のローカル変数宣言でO(n²)化)を発見・修正

ループ・配列アクセス中心のベンチマーク(エラトステネスの篩、200万要素)でツリーウォーカー
vs VM を比較しようとしたところ、VMだけが極端に遅く(数分経っても終わらず、ハングと
見分けがつかない)なることが判明。原因を切り分けた結果、ハングではなく本物の
計算量の劣化と分かった。

- **根本原因**: `for (...) { int j; ...; }` のように、ループ**本体の中で**ローカル変数を
  宣言するという、ごく普通のCのパターンが、VMの `env`(`(symbol . Variable)` の
  alist)には合わない構造だった。`iDECL` は無条件に `env` の先頭へconsするだけで、
  これを刈り取る仕組みは**関数呼び出しの終了時**(`iRETURN` の `baseEnv` 巻き戻し)
  にしか無かった。ループが1回まわるたびに `env` が1エントリずつ伸び続け、
  `iGETGVAR`/`iSETGVAR`(`assoc()` の線形探索)が反復回数に比例して遅くなるため、
  本来 O(n) のループが **O(n²)** になっていた。
  - ツリーウォーカーは `eval()` の `Block` ケースが `{...}` に入るたびに
    `Scope_begin()`/抜けるたびに `Scope_end()` を呼ぶため、1反復ごとに新しい
    (空の)スコープを作って捨てるだけで済み、影響を受けない。
  - 実測(切り分け用の最小再現コード、`n=20000`): ループの**外**で `int j;` を
    宣言 → 0.11秒。ループの**中**で宣言(篩と同じパターン)→ **46.8秒**
    (400倍以上遅い)。
- **修正**: 新しいopcode3つ(`iSAVEENV`/`iRESETENV`/`iDROPENV`)を追加。ループ
  (`While`/`For`)の先頭で現在の `env` を専用のヒープ配列(`envStack`、`stack`/
  `frames`とは別構造、通常の `REALLOC` で伸びる)に退避(`iSAVEENV`)。本体が
  正常に終えた場合と `continue` が合流する地点で、保存した `env` から現在の `env`
  まで束縛された `Variable` を全て `isdead=1` にしつつ `env` を保存値へ戻す
  (`iRESETENV`。マーカー自体はpopしないので次の反復でも使う)。ループを抜ける
  (条件が偽になる、または `break`)地点で同じ後始末をした上でマーカーを
  最終的にpopする(`iDROPENV`)。`break`/`continue` は本体の途中から直接
  ジャンプし得るため、どちらの opcode も「ジャンプ元の位置に関わらず現在の
  `env` から保存値までを辿って後始末する」形にして、部分的にしか実行されなかった
  反復でも正しく片付くようにした。ブロック単位(`if`/`switch`等)ではなく
  **ループ単位**でのみ後始末すれば、1回の反復中に何回 `iDECL` されようと
  反復回数には依存しないため十分と判断(実装をループ2ケースだけに限定)。
- **確認**: 切り分け用の再現コード(`n=20000`)が 46.8秒 → **0.12秒** に短縮。
  ネストしたループ+`continue`/`break`+各階層でのローカル変数宣言という複合
  パターンでもツリーウォーカーと完全に同じ結果になることを確認。再帰
  (`nfib(8)=67`)への影響が無いことも確認。
  篩ベンチマーク(200万要素)を実際に完走できるようになり、**ツリーウォーカー
  17.9秒 → VM 14.6秒(むしろVMの方が速い)**、両者とも同じ結果(`count%256=197`)。
- 追加テスト: `vm-loop-local-decl-ok.c`(`n=20000`、`continue`併用、`-O`で
  高速に完走することを確認。将来この修正が退行した場合、テストスイート全体が
  明らかに遅くなることでも気付けるようにする狙いも兼ねる)。
- `./scripts/run-tests.sh`・`make testvm` → 68 passed, 0 failed(修正前は67)。
- `docs/design/vm-implementation-status.md`・`README.md` のベンチマーク数値を更新。

**副次的に発見・修正**: ユーザーが `make sievevm`(`-O -vv` で `mydemo/eratosthenes.c`
を実行するターゲット、ユーザー自身がMakefileに追加)を実行したところ
`"cannot convert Closure to string"` で fatal。`toStringOn()` に `case Closure:` が
無く、`-O` のトップレベル関数定義の「文としての値」(`Closure`。ツリーウォーカーでは
代わりに `toStringOn()` が扱える生の `Function` ノード)を `-vv` の結果表示
(`"=> %s"`)に渡すと落ちていた(`-O` 単体・`-vv` 単体では踏まない組み合わせ)。
`Closure,function` へ委譲するだけの `case Closure:` を追加して解決。

### `include/string.h` を拡充: `strlen`/`strcpy`/`strcat`/`strcmp`/`memcpy`/`memset` を実装

`docs/design/task2-feature-inventory-and-proposal.md` が「未対応、追加すべき」と明記していた
項目(特に `strcpy`/`strcat` は文字列バッファオーバーフローという典型的なメモリバグパターンを
検出できるようにする狙いで推奨されていた)に対応。

- `strcpy`/`strcat` は1バイトずつ `setMemory()` を経由して書き込む(生の `memcpy` で
  バックエンドのバッファ同士をコピーするのではなく)。これにより、確保先バッファより
  長い文字列をコピーすると、他の全ての subc の書き込みと**全く同じ**
  `"memory offset out of bounds"` 検出が発火する —— 本物のlibcなら黙って隣接メモリを
  破壊するところを、検出できるようにするというこの追加の一番の狙い。
- `strlen`/`strcpy`/`strcat`/`strcmp` は共通ヘルパー `cStringBase()`/`cStringLen()`
  経由でポインタを解決。`pointerString()`(既存の `atoi`/`printf("%s",...)` が使う
  ヘルパー)と違い、ポインタ自身の `,offset` を尊重する(`pointerString()` は常に
  バッファの**先頭**から走査するため、`strcpy(buf+5, "x")` のような前進済みポインタでは
  誤動作する、既存の別のバグ。今回は触れていない)。`requireNotFreed()` も経由するため、
  解放済みポインタへの `strlen()` 等は use-after-free として検出される。
- `memcpy`/`memset` は `Memory` ブロックの生バイト単位で動作し、範囲外アクセスは
  専用のメッセージで fatal する。
- **副次的に発見・修正した既存バグ**: `char *s = "literal";` のような**宣言時**の
  文字列リテラル代入が、ツリーウォーカーの `initialiseVariable()` では生の `String`
  オブジェクトをそのまま `Pointer,base` として包んでしまい(`Memory` ブロックを
  経由しない)、VMの `prim_vm_coerce` に至っては `String` を全く処理せず値をそのまま
  通していた(`assign()` の**再代入**時の同じケースは正しく `Memory` ブロックへ変換
  していたのと対照的)。今回の string.h 実装で初めてこの不整合が表面化(`cStringBase()`
  が「バッファへのポインタではない」と正しく拒否 → その拒否メッセージを組み立てる
  `toString()` 自体が壊れた `Pointer` を渡されて別のエラーで落ちる、という形で発見)。
  両方とも `assign()` のString処理(`STRDUP`+`newMemory`)と同じロジックに揃えて修正。
  `char *s = "literal";` という基本的な宣言が、ツリーウォーカー・VM問わず初めて
  正しく動くようになった。
- 追加テスト: `string-functions-ok.c`(6関数の正しさを一括確認)、
  `strcpy-buffer-overflow.c`(バッファオーバーフロー検出)、
  `strlen-use-after-free.c`(解放済みポインタへの `strlen()` がuse-after-freeとして
  検出されることを確認)。
- `./scripts/run-tests.sh`・`make testvm` → 71 passed, 0 failed(修正前は68)。
