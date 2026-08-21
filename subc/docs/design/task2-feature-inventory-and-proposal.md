# subc Feature Inventory and Proposal: Memory-Bug Detection Roadmap

Research goal recap: `subc` is a teaching interpreter for a C subset (`main.leg`, PEG grammar
compiled by `leg` into `main.c`, evaluator objects allocated via Boehm GC). The long-term
research aim is to make it detect malloc/free-class memory bugs — use-after-free, dangling
pointers, double-free, out-of-bounds access — at a level useful for teaching, comparable in
spirit to ASan/Valgrind (see `demofiles/evalasan.txt`, `demofiles/evalvar.txt`).

This document is research/design only. No changes were made to `main.leg` or `main.c`.

> **Update (2026-08-21, after this analysis was written):** the "existing gap"
> described below (write-through-pointer use-after-free not checked, `Pointer.isfree`
> unreliable) has been fixed — see commits `4d4638f` (unify on `Memory.free` via
> `requireNotFreed()`, covering `setMemory()` and the `*p = rhs` path) and `7e189cc`
> (`p[i] = x` support). `demofiles/use-after-free.c` line 14 is now caught. The
> realloc/calloc proposals below remain open and are unaffected by this update.

---

## 1. How the interpreter is built (relevant to every proposal below)

Three places have to move together to add any new builtin function:

1. **`main.leg` line 108-109** — the `_do_primitives(_)` macro lists builtin names:
   `printf, assert, malloc, free, exit, abort, atoi, sqrtf`. Adding a name here auto-generates
   the `oop s_<name>` symbol variable (line 111) and its `intern()` initialisation in `main()`
   (line 4830-4832).
2. **A new `oop prim_<name>(int argc, oop *argv, oop env)` C function**, written next to
   `prim_malloc` / `prim_free` (`main.leg:2579-2650`).
3. **`typeCheck`'s `case Primitive:`** (`main.leg:2910-2948`) contains
   `#define _(X) if (s_##X == name) set(exp, Primitive,function, prim_##X); _do_primitives(_)`
   — this is what actually binds an `extern` declaration written in subc source to the C
   function pointer. If a program writes `extern void *realloc(...)` but `realloc` isn't in
   `_do_primitives`, typeCheck fatals with `"external symbol 'realloc' is undefined"`.
4. **`include/stdlib.h`** (the interpreter's own tiny stub, opened via the `include` grammar
   rule at `main.leg:1834-1837`) currently only declares `malloc`, `free`, `exit`, `abort`,
   `atoi`. A new builtin also needs its `extern` prototype added here or user programs can't
   `#include <stdlib.h>` and get it declared.

So "add a builtin" is a 3-file, mechanical change — this is good news for `realloc`/`calloc`.

### Core memory data model (this is the part that matters for bug detection)

```
struct Pointer { type_t _type;  oop type, base;  int offset, isfree; };
struct Memory  { type_t _type;  void *base;  size_t size;  int heap, free; };
```

- A `Pointer` never owns memory directly; it references a `Memory` (or a `Variable`, for
  `&x`, or an `Integer`, for arbitrary/cast pointers) plus an **element-count** `offset`
  (not a byte offset — `getPointer`/`getArray` multiply by `typeSize`).
- Multiple `Pointer` objects can alias the same `Memory` object (e.g. via pointer arithmetic,
  which conserves `base` and only changes `offset` — `main.leg:3902-3916`, `2356-2366`).
- `Memory.heap` marks GC-heap-allocated blocks (set by `malloc`, `main.leg:2590`).
- `Memory.free` is a **counter**, not a bool, incremented by `free()` (`main.leg:2613`); this
  is the field that actually gives alias-safe use-after-free detection, because all aliasing
  pointers share the same `Memory` object.
- `Pointer.isfree` is a *separate*, weaker, per-pointer-object flag set to `-1` unconditionally
  by `prim_free` (`main.leg:2605`), independent of the shared `Memory.free`. It is only
  consulted in `printiln` (`main.leg:1440`) — i.e. essentially only when printing/tracing a
  value, not on ordinary reads or writes.

### An existing gap worth knowing about before adding realloc

- **Read-through-pointer is checked**: `getPointer` (`main.leg:3202-3238`) does
  `if (get(base,Memory,free)) fatal("getting freed memory was not allowed...")` (line 3214).
  This correctly catches use-after-free through *any* alias, which is why
  `demofiles/use-after-free.c` line 13 (`printf("%d\n", *ptr)` after `free(ptr)`) is caught.
- **Write-through-pointer is NOT checked.** In `assign()`'s `Dereference` case
  (`main.leg:3455-3479`) the free-check is present in the source but **commented out**:
  `//if(get(ptr, Pointer, isfree))fatal("freed memory deref assign");` (line 3459). Likewise
  `setMemory` (`main.leg:3270-3313`, used for `Tpointer`-typed struct/array member stores) has
  no `Memory,free` check at all. And the `Dereference`-as-rvalue eval path also has its
  check commented out (`main.leg:3805`).
  - Concretely: `demofiles/use-after-free.c` line 14, `*ptr = 43;` (a **write** after `free`),
    is currently **not** flagged by subc even though both ASan and Valgrind flag it
    (`evalasan.txt` / `evalvar.txt` only report the read on line 13; a write-after-free
    program would be caught by them too but subc has no equivalent check on the write path).
  - This is directly relevant to realloc: any "old pointer invalidated after a moving
    realloc" design should share one consistent invalidation check used on *both* read and
    write paths, not silently rely on the read-only one that exists today.

---

## 2. Currently supported vs. not supported

### Grammar / control structures (`main.leg` grammar rules, ~line 1822-2140)

| Construct | Status | Grammar rule / evidence |
|---|---|---|
| `if` / `else` | Supported | `stmt` rule, `main.leg:1928-1931` |
| `while` | Supported | `main.leg:1923` |
| `for` (incl. C99 decl-in-init) | Supported | `main.leg:1924-1927` |
| `return` / `break` / `continue` | Supported | `main.leg:1932-1934` |
| blocks `{ }` | Supported | `main.leg:1917-1921` |
| `do { } while ()` | **Not supported** | no `DO` token/rule; only `while`/`for` in `stmt` |
| `switch` / `case` / `default` | **Not supported** | no `SWITCH`/`CASE` tokens at all |
| ternary `?:` | **Not supported** | no `?`/`:`-as-operator token; `assign`/`logor` chain has no conditional-expr rule |
| compound assignment `+= -= *= /= %= &= |= ^= <<= >>=` | **Not supported** | `assign` rule only recognises bare `ASSIGN` (`=`), `main.leg:1946` |
| pre/post `++`/`--` | Supported | `unary`/`postfix`, `main.leg:1992-1993, 2008-2009` |
| `,` comma operator (expr-level) | **Not supported** | `COMMA` only used as a list separator (args, decls), not as an expression operator |
| `goto` / labels | **Not supported** | no tokens |

### Types

| Type | Status | Evidence |
|---|---|---|
| `void, char, short, int, long, float, double` | Supported | `tname` rule, `main.leg:1850-1856` |
| pointers (`*`, arbitrary depth) | Supported | `decltor`/`funid`, `main.leg:1877-1884, 1910-1913` |
| arrays, incl. **multi-dimensional** (`int a[3][4]`) | Supported | `ddector`'s `(LBRAK e:expropt RBRAK)*` loop, `main.leg:1882-1884` — repeats, so nested `Tarray` is built |
| `struct` | Supported (definition + `.`/`->` member access) | `struct`/`members` rules, `main.leg:1860-1869`; `Member` eval, `main.leg:3422-3454` |
| `union` | **Not supported** despite reserved keyword | `UNION` token defined (`main.leg:2094`) and keyword-listed (`2069`) but **no grammar rule ever consumes it** — only `STRUCT` drives the `struct` production (`main.leg:1860`). A union tag would fail to parse. |
| `enum` | **Not supported** despite reserved keyword | Same situation: `ENUM` token defined/keyword-listed (`2095, 2069`) but no production references it; no `Tenum` in `_do_types` (`main.leg:63-74`). |
| function pointers | Parses (`decltor`/`ddector` allow `(*)()`) but **calling through one is unimplemented** | `Tfunction` case in `compileOn`/typeCheck is present for declarations, but no demo/test exercises calling through a `Pointer`-to-`Tfunction`; worth verifying before relying on it |
| `const` qualifier | **Not supported** | no `CONST` token anywhere |
| bitfields (`int x : 3;`) | **Not supported** | `vardecl`/struct `members` have no `:` width syntax |
| `typedef` | Supported | `typedec` rule, `main.leg:1841-1844` |
| variadic functions (`...`) | Supported (used internally for `main`, `printf`) | `ETC`, `t_etc`, `main.leg:2108, 1889` |
| `sizeof` on expressions and on type-names | Supported | `unary` rule's `SIZEOF` alternatives, `main.leg:1994-1997` — both `sizeof expr` and `sizeof(type)` |
| `sizeof` on struct/array/pointer types | Supported | `typeSize`, `main.leg:2323-2354` handles `Tstruct`, `Tarray`, `Tpointer` |
| `sizeof` on enum/union/bitfield | N/A (types don't exist) | — |

### Operators

Full binary/unary operator set is implemented: `+ - * / % << >> < <= >= > == != & ^ | && ||`
(`_do_binaries`, `main.leg:79-82`) and `- ! ~ ++ -- (pre/post) & (address-of) * (deref)`
(`_do_unaries`, `main.leg:76-77`). Pointer arithmetic (`ptr + int`, `array + int` decaying to
pointer, pointer comparison via `compare()`) is implemented (`main.leg:3902-3925`). Missing:
compound-assignment forms of all binary operators, and the ternary conditional operator.

### Builtins (`_do_primitives`, `main.leg:109`)

| Builtin | Status |
|---|---|
| `printf` | Supported (subset of format specifiers, `main.leg` around 2470-2565) |
| `assert` | Supported |
| `malloc` | Supported, with heap-tracking (`Memory.heap=1`, appended to global `heap` list for leak reporting at exit, `main.leg:4925-4930`) |
| `free` | Supported, with double-free and free-of-non-heap detection |
| `exit`, `abort` | Supported |
| `atoi` | Supported |
| `sqrtf` | Supported |
| `realloc` | **Not supported** — not in `_do_primitives`, no `prim_realloc`, not in `include/stdlib.h` |
| `calloc` | **Not supported** — same gaps |
| `strdup`/`strlen`/`strcpy`/`memcpy`/other string/mem builtins | **Not supported** (only what's listed above exists; `STRDUP` C macro exists at line 37 for internal string-literal handling but isn't exposed as a callable `strdup`) |

### demofiles/ coverage today (17 files)

`dangling-pointer[.c/-2.c]`, `invalid-free.c`, `invalid-pointer.c`, `memory-leak.c`,
`multiple-free.c`, `null-pointer.c`, `out-of-bounds-access[.c/-2.c]`, `pointer-compare.c`,
`pointer-increment.c`, `pointer-out-of-bounds[.c/-2.c]`, `segmentation-fault.c`,
`uninitialised.c`, `use-after-free[.c/-2.c]`.

Patterns exercised: stack-array OOB read (both off-the-end, both directions:
`pointer-out-of-bounds-2.c` deliberately checks `ptr[-2]` "OK" vs `ptr[-6]` "illegal"),
heap use-after-free (read only — see gap above), double-free, dangling pointer to a
returned-stack-local, free of a stack/arbitrary pointer, unfreed heap memory
(leak), pointer comparison across unrelated objects, pointer increment past a variable.
`use-after-free-2.c` builds a small linked list (`struct`-based) and frees nodes while
iterating — the most "realistic" test in the set.

`mydemo/` is general-purpose (fib, eratosthenes, fast-inverse-sqrt, a calculator, casts,
shifts) and only `unin.c` touches `struct`. Nothing in either directory exercises `switch`,
`do/while`, ternary, compound assignment, `union`, `enum`, multi-dimensional arrays, or
function pointers — consistent with the grammar gaps above.

---

## 3. Prioritized proposal

### #1 (top recommendation) — `realloc()` support with old-pointer invalidation

**Why it matters most**: `realloc` is the single most bug-prone allocator function in real C
— the canonical mistakes (using the old pointer after a move, leaking on a "shrink" that
returns a different pointer never checked, double-free after passing the same pointer to
`realloc` twice) are all use-after-free/double-free variants, i.e. exactly this project's
research theme. `demofiles/` currently has *zero* coverage of this because the builtin
doesn't exist at all. Adding it is also the best way to force the write-side use-after-free
gap (section 1) to get fixed, since a moved-realloc test is meaningless if writes aren't
checked.

**Design sketch**:

1. **Primitive plumbing** (mechanical, per section 1): add `_(realloc)` to `_do_primitives`
   (`main.leg:109`); add `extern void *realloc(void *pointer, long size);` to
   `include/stdlib.h`; write `prim_realloc`.
2. **`prim_realloc(argc, argv, env)` behavior**:
   ```
   arg0 = pointer, arg1 = new size (Integer)
   if arg0 is not a Pointer -> fatal (matches prim_free's "argument is not a pointer")
   base = arg0->Pointer.base
   if getType(base) != Memory or !base->Memory.heap
       -> fatal("realloc: pointer was not returned by malloc/calloc/realloc")
   if base->Memory.free
       -> fatal("realloc: pointer has already been freed")      // catches realloc-after-free
   oldraw  = base->Memory.base
   oldsize = base->Memory.size
   newraw  = REALLOC(oldraw, newsize)     // GC_realloc under USEGC
   if newraw == oldraw:
       // in-place: mutate the *same* Memory object
       set(base, Memory, size, newsize)
       return arg0 unchanged (same Pointer, same Memory, aliases stay valid)
       // NOTE: if newsize < oldsize, every existing bounds check (getPointer/getArray/
       // getMemory/setMemory, all of which compare against Memory.size) now correctly
       // starts rejecting accesses beyond the new, smaller size for *all* aliases, for free.
   else:
       // moved: the old Memory block must become "dead" for every pointer that still
       // references it, while the new block is fresh
       set(base, Memory, free, base->Memory.free + 1)   // reuse the exact same counter
                                                          // free() uses, so every existing
                                                          // check (getPointer line 3214,
                                                          // and any new write-side check
                                                          // from the fix below) already
                                                          // treats the old block as freed
       newmem = newMemory(newraw, newsize, /*heap=*/1)
       List_append(heap, newmem)                          // keep leak-tracking list accurate
       return newPointer(arg0->Pointer.type, newmem, 0)   // caller gets a fresh, valid Pointer
   ```
   Key insight: because every aliasing `Pointer` shares one `Memory` object, marking the old
   `Memory.free` counter is suficient to invalidate *all* existing aliases/derived pointers
   (e.g. ones produced by earlier pointer arithmetic on the old pointer) — no pointer-graph
   walk is needed. This reuses the exact mechanism `free()` already established.
3. **Distinguishing the error message** (optional, low cost): since `Memory.free` is already
   an integer counter rather than a bool, a moved-by-realloc invalidation could store a
   sentinel (e.g. `-2`) distinct from a normal free-count so `fatal()` messages can say
   "read through pointer invalidated by a moving realloc" vs. "read of freed memory" — nice
   for teaching diagnostics, not required for correctness.
4. **Fix the write-side gap as a co-requisite**: uncomment/re-implement the `Memory.free`
   check in `assign()`'s `Dereference` case (`main.leg:3459`) and add the equivalent check to
   `setMemory` (`main.leg:3270`) and to the `Dereference`-as-rvalue eval path
   (`main.leg:3805`) — otherwise `*old_ptr = 1;` after a moving `realloc` silently succeeds
   and corrupts/writes into GC'd-but-still-mapped memory instead of being reported.
5. **Edge cases to decide explicitly** (teaching value in making these fatal/loud rather than
   silently matching libc): `realloc(NULL, n)` (== malloc — should probably be supported
   since it's extremely common), `realloc(p, 0)` (glibc-dependent/deprecated behavior in C23
   — recommend making this a clear `fatal("realloc to size 0 is not supported by subc")`
   rather than replicating ambiguous libc behavior).

**Size/risk**: Medium. No grammar changes needed (it's just a new builtin call). The riskiest
part is correctly wiring the `heap` list bookkeeping so the end-of-program leak report
(`main.leg:4925-4930`, which iterates `heap` and fatals — actually just prints — for any
`Memory` with `heap` set and `free` unset) doesn't double-report the old, now-freed block as
both "freed" and "still leaked", and doesn't lose track of the new block. Needs a few new
demofiles (`realloc-move-then-use-old.c`, `realloc-shrink-then-oob.c`,
`realloc-then-double-free.c`) to pin down behavior.

### #2 — `calloc()` support

**Why it matters**: cheap, low-risk win, and it's the natural companion to `realloc` for
completing the standard allocator family; also lets programs exercise "was this memory
zero-initialized" reasoning (a different bug class from UAF — reading `malloc`'d-but-not-set
memory is exactly what `demofiles/uninitialised.c` already probes, but that test currently
uses stack variables; `calloc` gives a heap-side comparison point).

**Design sketch**: same 3-file plumbing as `realloc`. `prim_calloc(argc, argv, env)`:
validate `argc==2`, both `Integer`; compute `size_t size = n*sz` (recommend an explicit
overflow check — `fatal("calloc: size overflow")` if `n != 0 && size/n != sz` — this is
itself a nice teaching example of an integer-overflow-into-heap-overflow bug class that ASan
also flags); `CALLOC(N,S)` macro already exists (`main.leg:34/40`, uses `GC_malloc((N)*(S))`
under GC — note GC_malloc zero-fills by default, so behavior is correct, but the explicit
multiply-overflow check should be added in `prim_calloc` regardless, since the `CALLOC` macro
itself does not check it); wrap the result exactly like `prim_malloc` (`newMemory(...,heap=1)`,
append to `heap` list, return `newPointer(t_pvoid, mem, 0)`).

**Size/risk**: Small.

### #3 — Broaden array/pointer out-of-bounds coverage in `demofiles/`

**Why it matters**: this is pure test-content work (no interpreter changes), directly grows
confidence that existing bounds checks (`getArray`/`setArray`/`getMemory`/`setMemory`/
`getPointer`, all of which already fatal correctly on OOB) generalize across cases, and
would immediately benefit from realloc/calloc once added. Gaps identified by diffing
existing 4 OOB-flavored files against common ASan/Valgrind bug corpora:

- **Heap-array OOB** (all 4 existing OOB demos use **stack** arrays/vars; none allocate an
  array with `malloc` and then index past its extent) — e.g. `int *a = malloc(5*sizeof(int)); a[5] = 1;`
- **OOB via a `realloc`-shrunk block** (depends on #1): index that was valid before a
  shrinking `realloc` becomes OOB after — this is the demo that most directly showcases the
  "shrink mutates `Memory.size` in place" design from #1.
- **Multi-dimensional array OOB** (`int a[3][4]; a[3][0]` or `a[0][4]`) — untested, and
  currently *untestable* until confirmed the multi-dim grammar path is fully wired end-to-end
  in the evaluator (it parses per section 2, but no demo exercises it, so this doubles as a
  correctness check of an under-exercised code path).
- **Struct-array / array-of-struct member OOB** (index into `struct S arr[N]` past `N`, or a
  struct containing a fixed-size array member, e.g. `struct { int buf[4]; } s; s.buf[4]=1;`).
- **Negative/huge `malloc` size** feeding into an out-of-bounds access (e.g.
  `malloc(-1)` — currently `prim_malloc` at `main.leg:2583-2585` checks `is(Integer,arg)` and
  `size >= 0`, but `size_t size = _integerValue(arg)` converts a negative `long` to an
  enormous `size_t` *before* that check — worth a demo to confirm the existing
  `size > 10*1024*1024` cap actually catches this rather than the `size >= 0` check being
  silently defeated by the unsigned wraparound).
- **String/char-buffer OOB** (off-the-end write via `char buf[N]` and manual indexing,
  since there's no `strcpy`/`strcat` yet to trigger the classic string-overflow pattern —
  this also motivates eventually adding those builtins).
- **Pointer arithmetic that overflows the element-count `offset` far past the block**
  (e.g. `ptr + 1000000`) vs. the existing tests which only probe by 1-6 elements.

**Size/risk**: Small (test-authoring only, zero interpreter risk). Recommend doing this
alongside #1/#2 so the new allocator demos can be added in the same pass.

### #4 — Fix the write-side use-after-free gap (independent of realloc)

**Why it matters**: described in section 1. This is a real, currently-shipping detection gap
— `*ptr = 43;` after `free(ptr)` in `demofiles/use-after-free.c` line 14 is silently allowed
today (only the read on line 13 is caught). It's small, self-contained, and it's a
prerequisite for #1's write-path claims to actually hold.

**Design sketch**: re-enable/rewrite the three commented-out checks (`main.leg:3459, 3805`)
and add the missing one in `setMemory` (`main.leg:3270`), all keyed off `Memory.free` (not
`Pointer.isfree`, which per section 1 doesn't generalize across aliases).

**Size/risk**: Small, but touches core eval paths — needs the full existing demofiles/mydemo
suite re-run to confirm no false positives (e.g. legitimate writes through pointers that
happen to alias `Memory` objects that were never freed must still pass).

### #5 — `switch`/`case`/`default`

**Why it matters less for the memory-bug theme, but matters for "teaching C subset"
completeness**: `switch` is common in real code and its absence forces every demo/test
program into `if`/`else if` chains. Lower priority than #1-#4 because it's orthogonal to
memory-bug detection.

**Design sketch**: grammar — add `SWITCH`, `CASE`, `DEFAULT`, `COLON` tokens; extend `stmt`
(`main.leg:1923`) with a `SWITCH cond LBRACE (CASE expr COLON stmt* | DEFAULT COLON stmt*)* RBRACE`
production; add a `Switch` type to `_do_types` (`main.leg:63-74`) with `condition` and a list
of `(value, statements)` cases; add eval/typeCheck/compileOn cases mirroring the existing
`If`/`While` handling (`main.leg` eval switch around 4213-4230 for reference pattern, and the
`If` eval case for control flow). Fallthrough semantics (C's default) vs. `break`-to-exit need
to reuse the existing `Break` NLR mechanism (`main.leg:4723-4730`, `nlrPush`/`NLR_BREAK`).

**Size/risk**: Medium — new grammar + new AST node + eval/typeCheck/compile (VM) support in
three separate switch statements in the C preamble.

### #6 — Compound assignment operators (`+= -= *= /= %= &= |= ^= <<= >>=`)

**Why it matters less directly**: doesn't add new bug classes, but its absence makes several
natural realloc/array-growth idioms (`size *= 2;`) awkward to write in demo programs, and it's
very cheap to add.

**Design sketch**: add tokens (`PLUSEQ`, etc.) and extend the `assign` rule
(`main.leg:1946-1947`) to desugar `l OP= r` into `newAssign(l, newBinary(OP, l, r, t), t)` at
parse time (reusing the existing `l` sub-tree is fine since `l` here is just an lvalue
reference, not yet evaluated) — no new AST node type needed, no evaluator changes at all.

**Size/risk**: Small.

### #7 — Ternary `?:`

**Design sketch**: add `QUESTION`/`COLON` tokens; insert a `ternary` rule between `assign` and
`logor` (`cond = logor (QUESTION expr COLON cond)?`); reuse the existing `If`-like 3-way
eval/typeCheck/compile pattern, or represent it directly as an `If`-with-value if the `If`
struct/eval path can be made to return a value (currently `If`'s `compileOn` case at
`main.leg:4714` shows `Return` is `assert(!"unimplemented")` in the VM path, and `If` itself
doesn't currently produce a value in the tree-walking eval either — check
`main.leg` eval's `case If:` before assuming this is purely additive).

**Size/risk**: Medium — touches the expression grammar precedence chain and needs a decision
on whether `If` becomes value-producing or a separate `Ternary` node type is added (the latter
is safer/lower-risk).

### #8 — `enum` and `union`

**Why it matters less for this project's core theme**: neither directly creates new
memory-safety bug classes subc doesn't already model (an `enum` is just named ints; a
`union` mainly matters for type-punning/aliasing bugs, which *could* be an interesting later
research direction — e.g. "wrote as one union member, read as another, sizes differ" is a
real bug class — but it's a stretch goal, not core).

**Design sketch**: `enum` — add an `ENUM` production in `tname` similar to `struct`
(`main.leg:1857-1864`), producing a lightweight type whose "members" are `Constant` bindings
(`_do_types` already has `Constant`, `main.leg:73`) with sequential or explicit integer
values; `enum` values then typecheck as `Tint`. `union` — could reuse the existing `Tstruct`
type but with a flag making all members start at offset 0 (currently `members`/struct-size
computation, likely somewhere near `newTstruct`, always advances offsets sequentially — would
need to special-case union layout there); if later doing the type-punning research idea,
`union` read/write would need its own bounds/aliasing checks separate from `Tstruct`'s.

**Size/risk**: Large for `union` if the type-punning bug-detection angle is pursued
(otherwise medium as a pure parsing/layout feature); `enum` alone is small-medium.

---

## Summary table

| # | Feature | Memory-bug relevance | Size |
|---|---|---|---|
| 1 | `realloc` + old-pointer invalidation | Highest — direct core theme | Medium |
| 2 | `calloc` | Medium (uninitialized-memory comparisons, overflow) | Small |
| 3 | More OOB demofiles | High (coverage, zero interpreter risk) | Small |
| 4 | Fix write-side UAF check gap | High (closes a real, current gap) | Small |
| 5 | `switch/case` | Low (general completeness) | Medium |
| 6 | Compound assignment | Low (ergonomics) | Small |
| 7 | Ternary `?:` | Low (ergonomics) | Medium |
| 8 | `enum` / `union` | Low now, possible later (union type-punning bugs) | Medium/Large |

Recommended order: **#4 then #1** (fix the write-path gap first so realloc's invalidation
claim is meaningful), **#2** (cheap, same plumbing as #1), **#3** (test-authoring, can be
done in parallel), then #5-#8 as general C-subset completeness work if time remains.
