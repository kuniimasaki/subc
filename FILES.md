# ファイル一覧・説明

このリポジトリ内の各ファイルが何のためにあるかをまとめたインデックス。

> **メンテナンス方針**: 新しいファイル(ソース、テストの `.c`/`.expect`、ドキュメント等)を
> 追加したときは、このファイルにも1行〜数行の説明を追記すること。削除・リネームした
> ファイルの記述も合わせて更新・削除する。

## ルート

| ファイル | 説明 |
|---|---|
| `README.md` | プロジェクト概要・ビルド方法・変更履歴のサマリ。 |
| `FILES.md` | 本ファイル。全ファイルの説明インデックス。 |
| `.gitignore` | `claude-code-prompt.md`(ユーザー自身の作業用プロンプト、git管理外)を除外。 |

## `subc/`(本体)

| ファイル | 説明 |
|---|---|
| `main.leg` | **正本**。PEG文法定義+評価器(ツリーウォーカー・バイトコードVM)のC実装が同居する単一ソース。手で編集するのはここだけ。 |
| `main.c` | `main.leg` から `leg -o main.c main.leg` で生成される派生物。直接編集しない。 |
| `main` | `main.c` をビルドした実行バイナリ(`gcc -std=c99 -Werror -Wall -Wno-unused -g -o main main.c -lgc -lm`)。 |
| `Makefile` | ビルド(`make`)・テスト実行(`make test`)・デモ一括実行(`make demo`)・個別デモ実行などのターゲット定義。 |
| `CHANGELOG.md` | 日付・タスク単位で構成された詳細な変更履歴(発見したバグ、原因、修正、検証方法を記録)。 |
| `claude-code-prompt.md` | ユーザー自身が管理する元プロンプト文書(このセッションでは編集・参照のみ、git管理外)。 |
| `test.txt` | 基本文法(関数・構造体・配列・ポインタ・typedef 等)を一通り試す手動スモークテスト用ソース(`make smoke` で実行)。 |
| `true.txt` / `false.txt` | それぞれ `return 0;` / `return 1;` だけの最小ソース。終了コードの動作確認用。 |

## `subc/scripts/`

| ファイル | 説明 |
|---|---|
| `run-tests.sh` | `demofiles/*.c` + `*.expect` サイドカーを読み、`EXIT=`/`CONTAINS=`/`FLAGS=` と実行結果を突き合わせて自動判定する回帰テストハーネス。`make test` から呼ばれる。第1引数で任意のバイナリ(例: 修正前バイナリとの比較用)を指定可能。 |

## `subc/include/`(`#include` 用スタブヘッダ)

| ファイル | 説明 |
|---|---|
| `stdlib.h` | `malloc`/`free`/`realloc`/`calloc`/`exit`/`abort`/`atoi` 等の `extern` 宣言のみのスタブ。 |
| `stdio.h` | `printf` の `extern` 宣言のみのスタブ。 |
| `assert.h` | `assert` の `extern` 宣言のみのスタブ。 |
| `stdint.h` | `intptr_t` の `typedef` のみのスタブ。 |
| `string.h` | 空ファイル(現状 `string.h` 系関数は未サポート、`#include` してもエラーにならないためのプレースホルダ)。 |

## `subc/demofiles/`(メモリバグ検出の回帰テスト集)

`<name>.c` + `<name>.expect` が1テストケース。`.expect` は `EXIT=`(期待終了コード)、
`CONTAINS=`(標準出力+標準エラーに含まれるべき文字列)、`FLAGS=`(追加CLIフラグ、
例 `-O` でバイトコードVM経由のテストを指定)を持つ。`-ok-` を含むものは「バグに似ているが
合法なコードで誤検知しないこと」を確認する陰性ケース。

| ファイル | 検出する/確認する内容 |
|---|---|
| `use-after-free.c` | 解放後のポインタ読み出し(use-after-free)を検出。 |
| `use-after-free-2.c` | 連結リストなど、間接的な参照を介した use-after-free を検出。 |
| `use-after-free-ok-reassign.c` | 解放後に別のオブジェクトへ再代入すれば安全、という陰性ケース。 |
| `use-after-free-ok-unused.c` | 解放後にポインタを一切使わなければ問題ない、という陰性ケース。 |
| `write-after-free.c` | 解放後のポインタへの**書き込み**(`setMemory` 経由)を検出(Task 1 で修正した検出漏れの回帰テスト)。 |
| `dangling-pointer.c` | ローカル変数のアドレスを外に持ち出し、スコープを抜けた後にアクセス(dangling pointer)。 |
| `dangling-pointer-2.c` | free 後に読むパターンでの dangling pointer(use-after-free 相当)。 |
| `dangling-pointer-ok-alive.c` | 対象がまだ生存中にアクセスする、陰性ケース。 |
| `multiple-free.c` | 同じポインタを2回 `free()` する二重解放(double-free)を検出。 |
| `multiple-free-ok-distinct.c` | 別々に `malloc` した2つのポインタをそれぞれ1回ずつ `free`、陰性ケース。 |
| `invalid-free.c` | `malloc` していない変数へのポインタを `free()` しようとするケース。 |
| `invalid-pointer.c` | 任意の(malloc 由来でない)アドレスへの書き込みを検出。 |
| `null-pointer.c` | NULL ポインタの参照外し・使用を検出。 |
| `null-pointer-ok-checked.c` | NULL チェックしてから分岐、陰性ケース。 |
| `memory-leak.c` | `malloc` して `free` しないまま終了する典型的なメモリリークを検出。 |
| `uninitialised.c` | 未初期化変数の使用を検出。 |
| `segmentation-fault.c` | NULL アドレス等への書き込みでのクラッシュパターン。 |
| `out-of-bounds-access.c` | スタック配列の範囲外アクセス(OOB)を検出。 |
| `out-of-bounds-access-2.c` | ポインタ演算経由でベースオブジェクトの外を指すケースの OOB。 |
| `out-of-bounds-access-ok-inbounds.c` | 範囲内アクセスのみ、陰性ケース。 |
| `heap-array-oob.c` | ヒープ確保した配列に対する範囲外アクセス(境界値テスト)。 |
| `heap-array-ok-inbounds.c` | ヒープ配列の範囲内アクセス、陰性ケース。 |
| `char-buffer-oob.c` | `char` バッファに対する範囲外アクセス。 |
| `pointer-out-of-bounds.c` | ポインタ演算で確保サイズを超えて書き込むケース。 |
| `pointer-out-of-bounds-2.c` | ポインタ演算でベースより手前(負オフセット)に書き込むケース。 |
| `pointer-arithmetic-overflow.c` | 大きく外れたポインタ演算によるオーバーフロー(境界値テスト)。 |
| `pointer-increment.c` | `p++` 等でベースオブジェクトの範囲を超えて進めるケース。 |
| `pointer-compare.c` | 別々のオブジェクトを指すポインタ同士の比較(未定義動作)を検出。 |
| `pointer-compare-cast.c` | ポインタ⇔整数キャストを介した比較。`compare()` の tri-state 修正(Task 1)の回帰テスト。 |
| `pointer-index-assign.c` | `malloc` したポインタへの `p[i] = x`(添字代入)。Task 1 で未実装だったバグの回帰テスト。 |
| `pointer-init-no-double-eval.c` | ポインタ初期化式が二重評価されないことの確認(リーク誤検知バグの回帰テスト)。 |
| `struct-null-pointer-field.c` | 構造体フィールドへの NULL ポインタ格納(`setMemory` の `Integer` ケース、Task 1 の回帰テスト)。 |
| `malloc-negative-size.c` | `malloc()` に負のサイズを渡すケースを拒否することの確認。 |
| `calloc-ok-zero-init.c` | `calloc()` がゼロ初期化された領域を返すことの確認。 |
| `calloc-invalid-negative.c` | `calloc()` に不正な(負の)引数を渡すケースを拒否することの確認。 |
| `realloc-ok-grow-then-use.c` | `realloc()` でサイズ拡大後、新しい領域を問題なく使えることの確認。 |
| `realloc-ok-inplace-alias-still-valid.c` | その場拡張(ブロック移動なし)の場合、既存のエイリアスが壊れないことの確認。 |
| `realloc-ok-null-as-malloc.c` | `realloc(NULL, size)` が `malloc()` と同等に振る舞うことの確認。 |
| `realloc-move-then-use-old.c` | `realloc()` でブロックが移動した場合、旧ポインタが use-after-free として検出されることの確認。 |
| `realloc-after-free.c` | 解放済みポインタに対する `realloc()` を検出。 |
| `realloc-invalid-pointer.c` | `malloc` 由来でないポインタへの `realloc()` を検出。 |
| `realloc-shrink-then-oob.c` | `realloc()` でサイズ縮小後、縮小前の範囲へのアクセスを OOB として検出。 |
| `realloc-then-double-free.c` | `realloc()` 後、元のポインタと新しいポインタの両方を `free()` する二重解放を検出。 |
| `realloc-to-zero.c` | `realloc(p, 0)` が本実装ではサポート対象外であることの明示(未定義動作の回避)。 |
| `char-literal-assignment-ok.c` | `char c = 'a';` のような基本的な文字リテラル代入、陰性ケース(`converter()` 修正の回帰テスト)。 |
| `compound-assignment-ok.c` | `+= -= *= /= %= &= \|= ^= <<= >>=` 複合代入演算子の動作確認。 |
| `modulo-and-bitwise-ok.c` | `% & ^ \| <= >= && \|\|` 等、`typeCheck()` で未実装だった演算子の動作確認。 |
| `switch-ok.c` | `switch`/`case`/`default`(フォールスルー含む)の動作確認。 |
| `break-in-if-ok.c` | `if` の中の `break` がループのスコープに正しく伝播することの確認(`Continue`/`Break` の `typeCheck()` 対応の回帰テスト)。 |
| `vm-entry-point-ok.c` | `-O`(バイトコードVM)で `main()` が実際に実行され、`return` 値が終了コードになることの確認。 |
| `vm-pointer-roundtrip-ok.c` | `-O` 経由で `malloc→書き込み→読み込み→free` が正しく動くことの確認(明示キャスト無し、void* 暗黙変換の回帰テストも兼ねる)。 |
| `vm-use-after-free.c` | `-O` 経由でも use-after-free がツリーウォーカーと同じメッセージで検出されることの確認(Task 3 の核心デモ)。 |
| `vm-while-loop-ok.c` | `-O` で `while` ループが正しく終了すること(`iJMPF` が `false` を `nil` と取り違え無限ループしていたバグの回帰テスト)。 |
| `vm-if-false-branch-ok.c` | `-O` で `if` の false 分岐(`else`)が実際に実行されることの確認(同じ `iJMPF` バグの回帰テスト)。 |
| `vm-cast-ok.c` | `-O` で明示的なポインタキャスト・数値キャストが動くことの確認。 |
| `vm-for-loop-ok.c` | `-O` で `for` ループ(`continue`/`break` 込み)が正しく動くことの確認。 |
| `vm-addressof-ok.c` | `-O` で `&x`(ローカル変数のアドレス取得)経由の読み書きが正しく動くことの確認。 |
| `vm-increment-ok.c` | `-O` で `++`/`--`(前置・後置)が正しく動くことの確認。 |
| `vm-logical-ops-ok.c` | `-O` で `&&`/`||`(短絡評価)が正しく動くことの確認。 |
| `vm-pointer-arithmetic-ok.c` | `-O` でポインタ演算(`p+1`)とポインタ比較(NULLチェック・同一性比較)が正しく動くことの確認。 |
| `vm-local-array-ok.c` | `-O` でローカル配列の宣言(初期化子あり/なし)と添字アクセスが正しく動くことの確認。 |
| `vm-struct-member-ok.c` | `-O` で構造体宣言(初期化子あり/なし)とメンバアクセス(`s.field`)が正しく動くことの確認。 |
| `vm-switch-ok.c` | `-O` で `switch`/`case`/`default`(フォールスルー・break・switch内のcontinueが外側ループに伝播すること)が正しく動くことの確認。 |
| `vm-dangling-pointer.c` | `-O` でも `dangling-pointer.c` と同じスコープ終了処理(dangling pointer検出)が効くことの確認(`iRETURN`のisdead付与+`prim_vm_store_deref`のisdeadチェック追加の回帰テスト)。 |
| `vm-dangling-pointer-ok-alive.c` | `-O` での陰性ケース: `&i` を渡した別関数がまだ呼び出しスタック上にある間の使用は誤って dead 扱いされないことの確認。 |
| `vm-deep-recursion-ok.c` | `-O` の値/フレームスタックが動的に伸びること(旧: 固定32要素で深い再帰が溢れていたバグ)の確認。深さ5000の再帰が正しく動作することを検証。 |
| `vm-cast-chain-invalid-pointer.c` | `-O` でも `invalid-pointer.c` 後半と同じキャスト連鎖(`(int*)(intptr_t)N` の非ゼロ整数のポインタ再代入)が検出されることの確認(`coerceAssignedValue()`/`prim_vm_store_symbol` 追加の回帰テスト)。 |
| `evalasan.txt` | `demofiles/Makefile` の `asan_run` で記録した、AddressSanitizer による各バグパターンの実行結果ログ(比較用の参考資料)。 |
| `evalvar.txt` | 同 `demo`(Valgrind)ターゲットで記録した実行結果ログ(比較用の参考資料)。 |
| `Makefile` | `demofiles/*.c` を素の gcc / ASan / Valgrind でコンパイル・実行するための補助 Makefile(subc本体のインタプリタとは独立に、比較対象を作るためのもの)。 |
| `dangling-pointer`, `dangling-pointer-2`, `invalid-free`, `invalid-pointer`, `memory-leak`, `multiple-free`, `null-pointer`, `out-of-bounds-access`, `out-of-bounds-access-2`, `pointer-compare`, `pointer-increment`, `pointer-out-of-bounds`, `pointer-out-of-bounds-2`, `segmentation-fault`, `uninitialised`, `use-after-free`, `use-after-free-2` | 上記 `Makefile` の `asan_build`/`demo` ターゲットが対応する `.c` から生成した ELF バイナリ(`evalasan.txt`/`evalvar.txt` 取得時の副産物)。ソースから再生成可能。 |
| `pointer-compare.exe` | 同様に生成された Windows 向けバイナリ(比較用の副産物)。 |
| `pc` | 対応する `.c` ソースが存在しない、由来不明の古いコンパイル済みバイナリ。実体不明のため削除候補(要確認)。 |

## `subc/mydemo/`(一般的なCプログラムのサンプル・雑多な検証用スクリプト)

| ファイル | 説明 |
|---|---|
| `fib.c` | 再帰フィボナッチ(`nfib`)。ベンチマーク・動作確認用。 |
| `fib.py` | 同ロジックの Python 版(比較用リファレンス)。 |
| `eratosthenes.c` | エラトステネスの篩。`malloc` した配列を使うサンプル。 |
| `fisr.c` | 高速逆平方根(Quake III の `Q_rsqrt`)。ビット演算・キャストの多いサンプル。 |
| `calc.c` | int→long/float キャストの検証用(未完成、一部コメントアウト)。 |
| `cast.c` | 型キャストの動作確認用の小さなサンプル。 |
| `pdiff.c` | 別々の配列を指すポインタ同士の減算(未定義動作)を試すサンプル。 |
| `shl.c` / `shr.c` | 左シフト `<<` / 右シフト `>>` の動作確認用の最小サンプル。 |
| `uim.c` | `malloc` した領域を初期化せず読む(uninitialized memory)の確認用サンプル。 |
| `unin.c` | 構造体フィールドの一部だけ初期化し、残りを読む(未初期化アクセス)の確認用サンプル。 |
| `makefile` | `mydemo/*.c` を素の gcc でビルドするための補助 Makefile(`subc` 本体とは独立)。 |
| `fib`, `eratosthenes`, `fisr` | 対応する `.c` をビルドした実行バイナリ。 |

## `subc/docs/design/`(設計・調査メモ)

| ファイル | 説明 |
|---|---|
| `task2-feature-inventory-and-proposal.md` | Task 2 用の機能ギャップ調査+拡張提案(`realloc`/`calloc` の設計案を含む)。 |
| `task3-vm-audit-and-design.md` | Task 3 用のバイトコードVM(`-O`)監査+安全性フック設計案(Option A/B比較、Option B採用)。 |
| `task6-multi-agent-workflow.md` | Task 6 用の設計→実装→検証の3役パイプライン提案+`realloc`/`calloc`実装を題材にした試行レポート。 |
| `vm-implementation-status.md` | `-O`(バイトコードVM)の実装状況スナップショット。`compileOn()` の全ASTノード種別の実装/未実装状況、`demofiles/*.c`+`mydemo/*.c` 全件を `-O` で実行した実測結果、優先度付き残作業をまとめたもの。`compileOn()` に変更を加えたら更新すること。 |

## `subc/docs/verification/`(検証レポート)

| ファイル | 説明 |
|---|---|
| `task1-verification-report.md` | Task 1(3件の既知バグ修正)の検証手順・結果レポート。 |
| `realloc-calloc-verification.md` | `realloc()`/`calloc()` 実装(Task 2)の検証レポート。 |
