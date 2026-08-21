# Task 1 Verification Report

Scope: the three known bugs (Pointer.isfree unification, `p[i] = x` for malloc'd
pointers, `compare()` tri-state) plus the broader audit for similar patterns.
Commits: `4d4638f`, `7e189cc`, `cc0f7a3`, `7ab1dac` (canonical regen).

## Toolchain

`leg`/`peg` were not preinstalled anywhere reachable (no package in the sandbox's
apt cache had them enabled by default, no `gcc`/`leg` on the Windows host PATH).
Obtained non-destructively, no root required:

```
apt-get download peg                      # downloads the .deb, no sudo needed
dpkg-deb -x peg_*.deb pegextract           # extract without installing
cp pegextract/usr/bin/{leg,peg} ~/bin/
```

Confirmed `~/bin/leg -o main.c main.leg` run from `subc/` reproduces the
repository's original `main.c` byte-for-byte identical to the tracked copy
before any edits — i.e. the tracked `main.c` really is what `leg` produces from
`main.leg`, so regenerating after edits is trustworthy.

Build: `gcc -std=c99 -Werror -Wall -Wno-unused -g -o main main.c -lgc -lm`
(same flags as the Makefile's default target). Builds clean with no warnings
at every stage below.

## Regression methodology

1. Built the **original** (pre-Task-1) `main.c` from `git show HEAD~4:subc/main.c`
   as `main_orig`, and ran it over every `demofiles/*.c` file that existed before
   Task 1, capturing stdout+stderr and exit code per file → baseline.
2. After each of the three fixes (applied incrementally via the same hunks used
   for the three commits), regenerated `main.c`, rebuilt, reran the full
   `demofiles/*.c` set, and diffed against the baseline.
3. Confirmed the *only* differences across all three stages, and in the final
   state, are:
   - Non-deterministic heap addresses printed in `toString()`/`printiln()`
     output (`<0x...+N>`) — expected noise, changes every run even without any
     code change (GC allocation addresses vary by ASLR).
   - **One intentional change**: `demofiles/pointer-out-of-bounds-2.c`. Before
     the `p[i]=x` fix, the file's *first* statement — `ptr[-2] = 42; // OK`, a
     legitimate in-bounds write — incorrectly fatal'd with
     `invalid rvalue '...' assigning to: ptr[-2]` because `assign()`'s `Index`
     case had no `Pointer` branch at all. After the fix it succeeds, and
     execution correctly reaches the file's actually-illegal
     `ptr[-6] = 666; // illegal` line, which now fails with
     `memory offset is negative` (from `setMemory`'s existing bounds check).
     This is the bug being fixed, not a regression — confirmed by reading the
     demo file's own inline comments.
4. All 17 pre-existing demo files produce identical exit codes at every stage
   (16 fatal with exit 1, `memory-leak.c` exits 0, matching its "no assertion
   failure, just an intentional leak" nature).

## New test cases added

- `demofiles/write-after-free.c` — `free(ptr); *ptr = 42;` with **no read**
  first. Before the fix: silently succeeds (the write goes untracked); the
  bug is only incidentally surfaced by the *next* `printf("%d\n", *ptr)`,
  which trips `getPointer`'s pre-existing read check ("getting freed memory
  was not allowed"). After the fix: the write itself is caught immediately
  ("setting freed memory was not allowed"), before any further corruption.
- `demofiles/pointer-index-assign.c` — `p[0]=10; p[1]=20; p[2]=30;` through a
  malloc'd `int *`. Before: fatals on the very first assignment with
  `invalid rvalue ... assigning to: p[0]`. After: all three assignments and
  the follow-up reads succeed, prints `10 20 30`.
- `demofiles/pointer-compare-cast.c` — builds a Pointer-with-Integer-base via
  `(int *)0 + 50` (effective value 200) and compares it against the plain
  integer 100 with both `>` and `<`. Before: prints `not greater` / `not less`
  (wrong for `>`, since the buggy branch always returned a 0/1 boolean instead
  of an ordering). After: prints `greater` / `not less`.

All three were run against both `main_orig` and the fully-fixed `main` to
confirm the "before" behavior actually reproduces the described bug and the
"after" behavior is the intended fix, not just "some output".

Two dead ends hit while writing these tests, noted so they aren't mistaken for
regressions: `3 * sizeof(int)` and `(int *)200` both fail to type-check in this
interpreter for reasons **unrelated** to Task 1 (`sizeof` returns `long`, and
`int * long` multiplication and `int`-to-pointer casts are not supported by
the existing type-conversion tables) — the test files route around both via a
`long` intermediate variable and `(int*)0 + N` pointer arithmetic respectively.

## Out-of-scope finding (not fixed, flagged for later)

While isolating the `p[i]=x` test, found that **even a minimal
`malloc(...); free(p); return 0;` program reports
"allocated memory not freed at end of program"** despite the explicit `free()`
call — reproduces identically on `main_orig` (pre-Task-1), so it predates all
Task 1 changes and is not a regression. Likely cause (not confirmed): the
`preval`/`eval` double-evaluation of top-level function bodies executing the
`malloc()` call twice, registering two `Memory` blocks in the `heap` list while
`free()` only marks one. Worth a dedicated look before relying on the
leak-detector for grading/teaching purposes.
