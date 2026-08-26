# バイトコードVM(`-O`)実装状況

Task 3(`docs/design/task3-vm-audit-and-design.md`)でポインタ安全性フック(`prim_vm_deref`
等)を実装した後、`-O` が実際にどこまでの C コードを実行できるかを `compileOn()` の全 AST
ノード種別を対象に棚卸しし、`demofiles/*.c`+`mydemo/*.c` 全件を `-O` で実行して実測、
見つかったギャップを1つずつ埋めた記録。**この文書は現状のスナップショットであり、
`compileOn()`/`execute()` に手を入れたら更新すること。**

## 現在の状態(2026-08-26 時点)

`demofiles/*.c`(68本)+ `mydemo/*.c`(10本)、計78本を `-O` で実行した結果、
**全78本が `compileOn()`/`execute()` レベルのクラッシュなく最後まで実行できる**
(調査開始時点は15/62)。`make test` は68 passed, 0 failed(うち19本が`FLAGS=-O`の
VM専用回帰テスト、`vm-*.c`/`vm-*-ok.c`)。

さらに踏み込んだ検証として、`make testvm`(`FORCE_FLAGS=-O ./scripts/run-tests.sh`)で
`demofiles/*.c` **全68本**(通常はツリーウォーカーで実行される53本も含めて)を強制的に
`-O` で実行し、**それぞれの `.expect` が期待する終了コード・検出メッセージと完全に
一致することを確認済み**(68 passed, 0 failed)。単に「クラッシュしない」だけでなく、
「ツリーウォーカーと全く同じ結果になる」ところまで検証できている。

さらに、ベンチマークで **VMに深刻な性能バグ**(ループ本体内でのローカル変数宣言が
O(n²) に劣化する)を発見・修正した。詳細は下記「実装の勘所」の
「動的な値/フレームスタック」の次の項目を参照。

以下、実装済みの機能と、判明している既知の残課題をまとめる。

## `compileOn()` のASTノード網羅状況(`_do_types` 55種)

| 状態 | ノード | 備考 |
|---|---|---|
| ✅ 実装済み | `Undefined`, `Input`, `Integer`, `Float`, `Symbol`, `Pair`, `String`, `Closure`, `Call`, `Block`, `Dereference`, `Sizeof`, `Index`, `While`, `For`, `If`, `Switch`/`Case`/`Default`, `Return`, `Continue`, `Break`, `VarDecls`, `TypeDecls`, `Function`, `Primitive`, `Addressof`, `Cast`, `Member` | `Dereference`/`Index`/`Cast`/`Addressof`/`Member`/ポインタ代入・メンバ代入は全て `prim_vm_*` プリミティブ経由(Option B、ツリーウォーカーの既存関数をそのまま再利用)。`VarDecls`/`TypeDecls` はトップレベル出現時に自己 `typeCheck()` する(下記参照)。 |
| ✅ 実装済み(演算子単位) | `Unary` の `NEG`/`NOT`/`COM`/`PREINC`/`PREDEC`/`POSTINC`/`POSTDEC` | `++`/`--` は `prim_vm_incrdecr` 経由。 |
| ✅ 実装済み(演算子単位) | `Binary` の全演算子(`MUL DIV MOD ADD SUB SHL SHR LT LE GE GT EQ NE BAND BXOR BOR LAND LOR`) | `LAND`/`LOR` は短絡評価(JMPF/JMPによる制御フロー)。`ADD` はポインタ/配列+整数のポインタ演算に対応。比較演算子は `compare()`/`equal()` 経由(下記のiJMPF/比較バグ参照)。 |
| ✅ 到達不可能と確認済み(`assert(!"this cannot happen")`) | `Array`, `Pointer`, `Struct`, `Memory`, `Reference`, `Tvoid`..`Tetc`(12種), `Scope`, `TypeName`, `Variable`, `Constant`, `Token` | 実行対象のASTノードとしてではなく、他ノードのフィールド(型情報・実行時値)として扱われるため、`compileOn()` が直接呼ばれる経路が本当に無いことを、全ての再帰呼び出し箇所(`compileOn(...)`)を洗い出して確認した。以前は「未検証」だった(下記参照)。 |

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
- **ループ本体内のローカル変数宣言によるO(n²)化を修正**(ベンチマークで発見):
  ツリーウォーカー vs VM の速度比較のためエラトステネスの篩(200万要素)を実行したところ、
  VMだけが極端に遅い(ハングと見分けがつかないほど)ことが判明。原因を突き止めると、
  `for (...) { int j; ...; }` のようにループ**本体の中で**ローカル変数を宣言するという、
  ごく普通のCのパターンが、VMでは`iDECL`のたびに `env`(`(symbol . Variable)` のalist)へ
  無条件に consする一方、これを刈り取る仕組みが**関数呼び出しの終了時**(`iRETURN`)にしか
  無かった。ループのボディが1回実行されるたびに `env` が1エントリずつ伸び続け、
  `iGETGVAR`/`iSETGVAR`(`assoc()` による線形探索)が反復回数に比例して遅くなるため、
  本来 O(n) のループが **O(n²)** になっていた(`n=20000` の実測: 46.8秒 → 修正後 0.12秒。
  篩ベンチマーク(200万要素)は修正前は数百秒かかっていたと見られ、修正後は
  ツリーウォーカーの17.9秒よりむしろ速い14.6秒で完走)。
  - ツリーウォーカーは `eval()` の `Block` ケースが `{...}` に入るたびに
    `Scope_begin()`/抜けるたびに `Scope_end()` を呼ぶため、1回の反復ごとに
    新しい(空の)スコープを作って捨てるだけで済み、`scopes` は常にネスト深さ相応の
    サイズに収まっている。VMには「ループの1反復ぶんだけスコープを閉じる」という
    対応する仕組みが無かった。
  - 新しい3つのopcode(`iSAVEENV`/`iRESETENV`/`iDROPENV`)を追加。ループ(`While`/`For`)の
    先頭で現在の `env` を専用のヒープ配列(`envStack`、通常の `REALLOC` で伸びる
    upward-growing、`stack`/`frames`とは無関係の別構造)に退避(`iSAVEENV`)。
    本体が正常に実行を終えた場合と `continue` の両方が合流する地点(`iRESETENV`。
    保存した `env` と現在の `env` の間で束縛された `Variable` を全て `isdead=1` にし、
    `env` を保存値に戻す。マーカー自体はpopしないので次の反復でも再利用できる)、
    ループを抜ける(条件が偽になる、または `break`)地点(`iDROPENV`。同じ後始末を
    行った上でマーカーを最終的にpopする)。
  - `break`/`continue` はどちらも(ループ内で `iDECL` された変数を残したまま)本体の
    途中から直接ジャンプする可能性があるため、`iRESETENV`/`iDROPENV` はジャンプ元の
    位置に関わらず「現在の `env` から保存値まで」を辿って後始末する(部分的にしか
    実行されなかった反復でも正しく片付く)。
  - ブロック単位(`if`/`switch` 等)ではなく**ループ単位**でのみ後始末すれば十分
    (1回の反復中に何回`iDECL`されようと、ループの反復回数に依存しない限り
    O(n²)化は起きないため)。関数呼び出し・再帰は既存の `iRETURN`/`baseEnv` の
    仕組みと独立に動作(ネストしたループ・再帰呼び出しの組み合わせで正しく動作する
    ことを確認)。
  - 追加テスト: `vm-loop-local-decl-ok.c`(`n=20000` でループ本体内に `int j` を
    宣言、`continue` も併用。`-O` で高速に完走し、期待する終了コードになることを
    確認 —— 将来この修正が退行した場合、テストスイート全体が明らかに遅くなることで
    気付けるようにする狙いも兼ねる)。
- **プレーンな再代入(`x = expr;`)でのポインタ強制変換**: `int *p = malloc(...)`
  のような**宣言時**の初期化子は `prim_vm_coerce` が(`initialiseVariable()` の
  Tpointerケースを模して)coerceするが、それはあくまで宣言のときだけの話。
  `ptr = (int *)(intptr_t)0xDeadD0d0;` のような**再代入**は、`compileOn` の
  `Assign` の `default:`(裸のシンボルへの代入)がただの `iSETGVAR` で、
  型を一切見ずに評価結果をそのまま突っ込んでいた。キャスト
  `(int *)(intptr_t)N` 自体は(`eval()`のCastケースと同じロジックを
  `prim_vm_cast` がそのままミラーしているため)ツリーウォーカーと同じく
  生の `Integer` を返す(この段では正しい、意図的な挙動)。ツリーウォーカーが
  検出できていたのは、`assign()` の `case Variable:` が
  「再代入先がポインタ型なら、右辺の裸の `Integer`/`Array`/`String`/
  型違いの `Pointer` を全て強制的に `Pointer{base=...}` に包む」という
  無条件の変換を再代入のたびに行っているから。この変換ロジックを
  `coerceAssignedValue()` として切り出し、`assign()` の `case Variable:` と
  VM新設の `prim_vm_store_symbol`(裸シンボルへの代入を `iGETGVAR`/`iSETGVAR`
  と全く同じ手順(`env` alist → `Scope_lookup` → 最後の手段として素の
  `Symbol,value`)でVariableボックスを解決してから同じ変換をかける)の
  両方から呼ぶように統一。
  - **副次的に発見**: `prim_vm_store_deref`(`*p = rhs`)の `switch(getType(base))`
    には `case Integer:`(`(void*)(intptr_t)N` のような「変数でもメモリブロック
    でもない、ただの整数をbaseに持つポインタ」)が無く、汎用の
    `"cannot store through pointer"` に落ちていた。`assign()` の
    Dereferenceケースが持つ専用メッセージ
    (`"attempt to store into arbitrary memory location"`)を追加してツリーウォーカー
    と一致させた。
  - 追加テスト: `vm-cast-chain-invalid-pointer.c`(`demofiles/invalid-pointer.c` の
    後半部分と同じキャスト連鎖を `-O` で実行し、ツリーウォーカーと全く同じ
    メッセージで検出されることを確認)。
- **トップレベル `VarDecls`/`TypeDecls` の自己 typeCheck**: `replFile` の `-O` 経路は
  トップレベルの各構文要素を `typeCheck()`/`preval()` に一切通さず直接 `compile`+`execute`
  する(`Function`/`Primitive` は `compileOn` 内で自己 `typeCheck()` して補っていたが、
  素の `VarDecls`(グローバル変数、あるいは `struct Point { int x, y; };` のような
  タグ宣言のみの宣言)と `TypeDecls`(`typedef`)には同様の補完が無かった)。
  「まだ型解決されていないか」を「先頭要素が期待する型(`Variable`/`TypeName`)になって
  いないか」で判定し、必要なら自身に `typeCheck(exp, nil)` を実行してから処理を続ける
  ように修正。
- **残っていた `assert(!"unimplemented")` の棚卸し完了**: `Pointer`/`Array`/`Struct`/
  `List`/`Memory`/`Reference`/`Tvoid`..`Tetc`(12種)/`Scope`/`TypeName`/`Variable`/
  `Constant` は全て、`compileOn()` 内の再帰呼び出し箇所を1つ残らず洗い出した結果、
  「他ノードのフィールドとして `get(...)` で読むだけ、または typeCheck 済みの
  値を `EMITio(iPUSH, ...)` で直接プッシュするだけで、`compileOn()` 自体には
  一度も渡らない」ことを確認できた(パーサがこれらの型のASTノードを構築することは
  無く、typeCheck の書き換えも常に通常のASTノードの上で行われるため)。
  `assert(!"unimplemented")`(まだ実装していないだけ、というニュアンス)を
  `assert(!"this cannot happen")`(到達不可能であることを確認済み、というニュアンス。
  既存の `Token`/`Scope` と同じ表現)に変更。挙動は変わらない(元々どちらも
  到達すればクラッシュする)、`docs/design/vm-implementation-status.md` の
  「未検証」という注記を解消するための、意味の正確化のみの変更。

- **`make testvm`(全 demofiles を強制的に `-O` で実行)で見つかったメッセージ不一致2件**:
  クラッシュ検出網では引っかからない(検出はできているが、メッセージが違うだけの)
  真の意味論的乖離。
  - `prim_vm_store_deref` の `case Memory:` が `setMemory()` にそのまま委譲していたため、
    ポインタ演算で範囲外に出た書き込み(`pointer-out-of-bounds.c`、
    `pointer-arithmetic-overflow.c`)が `setMemory()` 自身の境界チェックメッセージ
    (`"memory offset out of bounds"`)で止まっていた。ツリーウォーカーの `assign()`
    のDereference/Memoryケースはこれとは別に、もっと早い段階で
    (`offset < 0 || offset*scale > size-scale`)という独自の境界チェックを行い、
    `"assigning to out-of-bounds pointer"` という別メッセージで fatal する。
    `prim_vm_store_deref` にも同じ境界チェック+メッセージを追加(`setMemory()` への
    委譲はそのチェックを通過した後段で維持、`Tpointer`/`Tstruct` 書き込みなど
    `setMemory()` が持つ広い型対応は失わない)。
  - `iGETGVAR` がローカル変数の値を読むとき、`nil`(未代入)かどうかを一切
    チェックしていなかった。ツリーウォーカーの `eval()` の `Symbol` ケースは
    `Variable,value` が `nil` なら `"use of uninitialised variable"` で fatal する
    (`initialiseVariable()` の scalar `default:` ケースは初期化式が無ければ何もせず、
    宣言時に入っている `nil` がそのまま「未初期化」の目印として残る仕組み)。VM側は
    この目印を一度も見ておらず、`int a, b = a;` が `-O` だと何の検出もせず正常終了
    していた(`demofiles/uninitialised.c`)。`iGETGVAR` の2つの解決経路(`env` alist、
    `Scope_lookup`)双方に同じ `nil` チェックを追加。
  - どちらも `make testvm` で単に全 `demofiles/*.c` を `-O` 強制実行しただけで
    見つかった(クラッシュはしないので、それまでの「クラッシュ有無」だけを見る
    網羅性スイープでは検出できなかった)。修正後、67本全てが完全一致するように
    なった。
- **構造体のポインタフィールド経由の読み出しでMemory追跡情報が失われる**
  (`getMemory()`、ツリーウォーカー・VM共通の既存バグ): `mydemo/kitchen-sink.c`
  (実装した機能をできるだけ広く使う統合テスト、後述)を書く過程で発見。
  ```c
  struct Node *dead = head;
  head = head->next;   // ← ここ
  free(dead);            // "freed memory was not allocated in the heap" で fatal
  ```
  `head`(`malloc`で確保)自体は正しく`free`できるのに、`head->next`のように
  構造体のポインタフィールド経由で読み出したポインタは`free`できなかった。
  `getMemory()` の `Tpointer` ケースが、構造体フィールドからポインタを読むたびに
  `newMemory(value, typeSize(target), 0)` で**真新しい(heapフラグ常に0の)
  `Memory`オブジェクトを毎回作り直していた**ため。単に`free()`できないだけでなく、
  同じフィールドを2回読むと実体は同じメモリでも追跡上は別物として扱われる
  ため、**片方の読み出し経由で`free()`しても、もう片方の読み出し経由での
  アクセスはuse-after-freeとして検出されない**という、この研究テーマの核心に
  関わる検出漏れも引き起こしていた(実際に2回読んで検証、修正前は検出漏れ、
  修正後は正しく検出されることを確認)。
  - 修正: `malloc`/`calloc`/`realloc` は必ず割り当てを `heap`(グローバルな
    `List`)に登録しているので、`getMemory()`のTpointerケースで生アドレスを
    ラップする前に、まず`heap`リストから同じ`.base`を持つ既存の`Memory`
    オブジェクトを探す `findHeapMemory()` を新設し、見つかればそれを再利用する
    (見つからない場合のみ従来通り新規ラップにフォールバック)ように修正。
  - `getPointer()`(ポインタ経由の直接デリファレンス)には元々 `Tpointer` を
    指す `Tpointer` のケース自体が無く(=ポインタのポインタの直接デリファレンスは
    別途未対応、既知の制限として現状維持)、影響範囲は`getMemory()`
    (構造体メンバアクセス経由)のみと確認。
  - ツリーウォーカー・VM共通のコードなので両方に同時に効く。`make test`/
    `make testvm` → 引き続き74 passed, 0 failed(無回帰)を確認。
- **`-O`固有の、まれでタイミング依存の`SIGSEGV`**(`execute()`のスタック/フレーム
  配列がGC回収のタイミングによっては壊れる): `kitchen-sink.c`のようにトップレベルの
  `extern`宣言・関数定義が多いプログラムを`-O`で実行すると、構文解析中
  (`List_append`→`GC_realloc`)で確率的にクラッシュすることがあった
  (再現条件を絞り込んだ最小ケースで15/15回、フルの`kitchen-sink.c`では30回中
  26回)。GDB/Valgrindで追跡した結果、クラッシュ地点自体(`0x20`という明らかに
  不正なアドレスへのアクセス)は無関係な別の`List`オブジェクトが壊れていることを
  示しており、本当の原因は別の場所にあると判断。`execute()`は`-O`ではプログラム
  全体で1回ではなく、**トップレベルの宣言・定義1つごとに個別に呼ばれる**
  (Task 3以来の設計)ため、その都度`stack`/`frames`/`envStack`(いずれも
  `MALLOC`で確保するローカル変数)が生成されては`execute()`が戻るたびに破棄
  されていた。この「使い捨てのGC_malloc」を`execute()`の呼び出し回数ぶん
  繰り返すことが、保守的(conservative)GCであるBoehm GCに対して、まだ生きている
  はずの何かを誤って回収・再利用させてしまう機会を与えていたとみられる
  (タイミング依存で再現率が揺れるのはconservative GCの偽陰性に典型的な挙動)。
  - 修正: `stack`/`frames`/`envStack`とその容量変数を`static`にし、
    バッファ自体を`execute()`の呼び出しをまたいで永続化(`sp`/`fp`/`esp`の
    インデックスだけを毎回リセット)するように変更。`execute()`が再入(自分自身を
    間接的にでも呼び出す)することは無い(インタプリタ内の関数呼び出し・再帰は
    全て同一`execute()`呼び出し内のバイトコードレベルの`iCALL`/`frames[]`で
    処理される)ため安全。
  - 確認: 修正前は15/15・26/30で再現していた最小ケース・フルケースの両方が、
    修正後は合計75回の実行で0回に(20回・30回・25回の3バッチ)。副作用として
    トップレベル宣言のたびに発生していたmalloc/free churnも無くなり、わずかに
    効率も向上。
  - `make test`/`make testvm` → 74 passed, 0 failed(無回帰)、`nfib(8)=67`・
    `depth(5000)%256=136`(動的スタック伸長の回帰確認)も引き続き正しいことを確認。

## 実装済み機能を横断的に使う統合テスト: `mydemo/kitchen-sink.c`

ユーザーからの依頼で、実装済みの機能(制御構文・再帰・構造体・配列・ポインタ演算・
ヒープ・`string.h`・`stdio.h`(`sprintf`/`snprintf`含む)・`stdlib.h`・`math.h`・
`ctype.h`・複合代入・キャスト)をできるだけ広く1本のプログラムで使う統合テストを作成
(`make kitchensink`/`make kitchensinkvm`)。上記の2件の重大バグはいずれもこの
テストを書く過程で発見された。ツリーウォーカー・VMで出力が完全に一致することを
確認済み。

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
- **`toStringOn()` に `Closure` のケースが無かった**: ツリーウォーカーでは
  トップレベルの関数定義の「文としての値」は `preval()` が返す生の `Function`
  ノード(`toStringOn()` に既存の `case Function:` で表示できる)だが、`-O` の
  `compileOn` のFunctionケースは代わりに `Closure`(`Function`+捕捉環境)を
  スタックに残す。`-O -vv` で実行すると `replFile` の `"=> %s"` 結果表示が
  この `Closure` を `toStringOn()` に渡し、対応するケースが無く
  `"cannot convert Closure to string"` で fatal していた(`-O` 単体、または
  `-vv` 単体では踏まない組み合わせのため、他の検証では見つからなかった。
  ユーザーが `make sievevm`(`-O -vv` で `mydemo/eratosthenes.c` を実行する
  ターゲット)を実行して発見)。`case Closure:` を追加し、`Closure,function`
  (中身の `Function` ノード)へ委譲するだけで解決(表示ロジックの重複無し)。
- **`Switch` の初期実装がbreak経路でスタックリークしていた**: `break` は自分の値を
  push してからジャンプするが、ジャンプ先(switch脱出ラベル)の直後に「フォールスルーで
  自然に文末へ到達した場合」用のpushが無条件で置かれており、break経由の到達だと
  二重にpushされていた。3つの脱出経路(break/`Default`無しでの不一致/自然な文末到達)が
  それぞれ厳密に1回だけpushするよう修正。

## 既知の残課題(意図的に今回は追わなかったもの)

- **`mydemo/fisr.c`(高速逆平方根)の数値が正しくない**: 調査の結果、これは
  **VM固有の遅れではなく、ツリーウォーカー側にも全く同じ形で存在する既存の
  制限**と判明(`-O` 無し・`-O` あり両方で同一の誤ったガベージ値を返す)。
  `i = *(long *)&y;` のようなビットレベルのfloat/int型パニング(同じメモリを
  型を変えて再解釈する常套手段)は、ポインタの参照先が「変数(`Pointer.base`
  が `Variable`)」の場合、`getPointer()`/`setMemory()` が `Pointer.type` を
  無視して単に `Variable.value` を返す/上書きするだけで、生バイトの再解釈を
  一切行っていない(メモリブロック経由のポインタなら `*(long*)addr` で正しく
  バイト再解釈されるが、ローカル変数直参照の経路には無い)。修正には
  `getPointer`/`setMemory` の `Variable` ケースに型パニングを実装する必要があり、
  ツリーウォーカー・VM 両方の意味論に影響する、本ドキュメントの主眼(VMを
  ツリーウォーカーに追いつかせる)を超えた変更になるため、ユーザー確認の上で
  今回はスコープ外とした。

## 検証方法

- `main.leg` の `compileOn(oop exp, oop program, oop cs, oop bs)` の
  `switch (getType(exp))` を全ケース読み、未実装/バグの箇所を洗い出した。
- `demofiles/*.c`+`mydemo/*.c` を素朴に `./main -O <file>` で実行し、正常終了したもの、
  検出成功したもの、`assert(!"unimplemented")` でクラッシュしたものを分類。
  静かなバグ(ハング、誤った結果)は個別のテストプログラムで手動確認
  (`while`ループの反復回数を変えたテスト、gdbでの `fatal()` へのブレークポイント等)。
- ツリーウォーカー側(`-O` 無し)は各コミットごとに `make test` で回帰確認。
- `make testvm`(`scripts/run-tests.sh` の `FORCE_FLAGS` 環境変数で全テストの
  `FLAGS=` を上書きし、`.expect` は一切変えずに全 `demofiles/*.c` を強制的に
  `-O` で実行する)で、クラッシュ有無だけでなく期待する終了コード・メッセージまで
  ツリーウォーカーと一致するかを検証できる。`make demovm` は `demofiles/*.c`+
  `mydemo/*.c` を `-O` で一括実行する目視確認用(`make demo`/`demov` のVM版)。
