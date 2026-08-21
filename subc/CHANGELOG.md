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

### 検証方法(共通)

各コミットについて、`leg` で `main.leg` から `main.c` を再生成 → `gcc -std=c99 -Werror -Wall -Wno-unused -g` で
ビルド → `demofiles/*.c` 全件を実行し、修正前の出力(非決定的なヒープアドレスを除く)との差分が
意図した変更のみであることを確認した。
