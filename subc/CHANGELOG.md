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
