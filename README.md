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

## ビルド

依存: `leg`/`peg`(Ian Piumarta の PEG parser generator)、`gcc`、`libgc-dev`。

```sh
cd subc
make          # main.leg -> main.c -> main をビルド
make demo     # demofiles/*.c を全件実行
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

**ドキュメント**
- `docs/design/` に機能拡張(`realloc`/`calloc` 等)と VM(`-O`)監査の設計メモを追加。
- `docs/verification/` に Task 1 の回帰確認レポートを追加。
- パーサ側(AST生成)の監査を実施。`(f)(3)` のような括弧越しの関数呼び出しが型キャストと
  誤認される、`sizeof(typedef名)` が fatal するという2件の実バグを確認(**未修正**、詳細は
  `CHANGELOG.md` 参照)。

各修正は独立したコミットに分割し、コミットのたびに `demofiles/*.c` 全件を実行して
既存の検出結果(非決定的なヒープアドレスを除く)が意図せず変化していないことを確認している。
