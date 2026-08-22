# realloc()/calloc() Verification Report

Scope: Task 2's top-recommended feature (`docs/design/task2-feature-inventory-and-proposal.md`,
sections "#1" and "#2"), implemented and verified as a trial run of the
design→implementation→verification pipeline described in
`docs/design/task6-multi-agent-workflow.md`.

## What was implemented

- `prim_realloc`/`prim_calloc` in `main.leg`, next to `prim_malloc`/`prim_free`,
  wired into `_do_primitives` and declared in `include/stdlib.h`.
- `calloc(n, size)`: overflow-checked `n*size`, zero-fills (via the existing
  `CALLOC`/`GC_malloc` macro), same `heap`-list bookkeeping as `malloc`.
- `realloc(pointer, size)`: rejects non-heap or already-freed pointers
  (mirroring `free`'s own validation); on an in-place `GC_realloc` (same raw
  pointer returned), mutates the *existing* `Memory` object's `size` field so
  every alias of that block stays valid; on a moving `GC_realloc`, marks the
  *old* `Memory` object freed via the exact counter `free()` increments (so
  `requireNotFreed()` — already used everywhere reads/writes go through a
  pointer — catches any later access through a stale alias) and wraps the new
  block in a fresh `Memory`+`Pointer`. `realloc(NULL, n)` behaves like
  `malloc(n)`; `realloc(p, 0)` is an explicit `fatal()` rather than replicating
  ambiguous libc behavior.
- **Found and fixed along the way** (not part of the original design doc):
  `typeCheck()`'s `case Primitive:` rejected `void *pointer` as an "illegal
  void parameter" whenever it wasn't the sole parameter, because the bare-void
  check ran against the parameter's *base* type (`Tvoid`) before the pointer
  declarator was applied — harmless for `free(void *pointer)` (its only
  parameter) but broke `realloc(void *pointer, long size)`. The check was
  reordered to run after `makeType()` resolves the full declared type (so
  `void *x` correctly resolves to `Tpointer(Tvoid)`, not `Tvoid`). The
  identical bug existed in the parallel `case Function:` path (user-defined
  functions) and was fixed the same way — confirmed by hand with a
  `void process(void *buf, int len)` repro that fatal'd before the fix and
  prints correctly after.

## What was tested and why

12 new `demofiles/*.c` + `.expect` pairs, following the harness convention
(`EXIT=0|1` plus an optional `CONTAINS=<substring>`):

| File | Checks |
|---|---|
| `realloc-move-then-use-old.c` | Core feature: a moving realloc invalidates the OLD pointer; reading through it is caught exactly like an ordinary use-after-free. |
| `realloc-shrink-then-oob.c` | A shrinking realloc mutates `Memory.size`, so an index valid before the shrink becomes an out-of-bounds write after it. |
| `realloc-then-double-free.c` | After a moving realloc, freeing the new pointer then separately freeing the stale old pointer is a double-free of the same underlying block (shared free-counter). |
| `realloc-after-free.c` | realloc() on an already-freed pointer is rejected. |
| `realloc-invalid-pointer.c` | realloc() on a non-heap pointer (address of a stack variable) is rejected, mirroring `free`'s equivalent check. |
| `realloc-to-zero.c` | `realloc(p, 0)` is an explicit, clear fatal rather than ambiguous libc behavior. |
| `realloc-ok-null-as-malloc.c` (negative) | `realloc(NULL, n)` behaves like `malloc(n)` — must NOT be flagged. |
| `realloc-ok-grow-then-use.c` (negative) | A normal grow-and-use sequence works end to end. |
| `realloc-ok-inplace-alias-still-valid.c` (negative) | When realloc doesn't move the block, an alias derived by earlier pointer arithmetic (`p + 1`), not just the pointer passed to realloc, must remain valid — asserts `q == p` to confirm the in-place path was actually exercised. |
| `calloc-ok-zero-init.c` (negative) | calloc'd memory is zero-filled. |
| `calloc-invalid-negative.c` | A negative element count is rejected rather than silently reinterpreted as a huge unsigned size. |

All 12 were read and cross-checked against `prim_realloc`/`prim_calloc`'s
actual code (not just run) before being accepted.

## Results

```
./scripts/run-tests.sh
...
39 passed, 0 failed, 0 skipped (no .expect)
```

Full `demofiles/*.c` regression (all 39, including every pre-existing file)
diffed against the pre-Task-1 baseline: only the differences already
documented in `docs/verification/task1-verification-report.md` and
`CHANGELOG.md` (nondeterministic heap addresses, and the two earlier,
intentional behavior changes on `pointer-out-of-bounds-2.c` and
`null-pointer.c`/`memory-leak.c`) — nothing new or unexplained.

Build: `gcc -std=c99 -Werror -Wall -Wno-unused -g -o main main.c -lgc -lm`
succeeds with zero warnings.

## Process note (Task 6 trial)

The Implementation and Verification stages were run as separate `Agent`
invocations (see `docs/design/task6-multi-agent-workflow.md` for the full
writeup). The Verification agent was cut off mid-task by an infrastructure
error after producing all 12 test files and running the harness successfully,
but before writing this report — the orchestrating session (not a fresh
agent) reviewed the diff, re-ran the harness and the full regression itself,
read each new test file against the actual implementation, and wrote this
report directly rather than re-spawning a third agent to redo already-
completed, already-verified work.
