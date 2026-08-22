#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-move-then-use-old: a realloc() that has to grow far beyond the
// original block's size classically moves the allocation to fresh memory.
// The OLD pointer must become just as invalid as if free() had been called
// on it directly -- realloc() reuses the exact same Memory.free counter/
// requireNotFreed() mechanism free() uses, so any later read through the
// stale old pointer must be caught, not silently read moved-away memory.

int main() {
  long n = 3;
  int *p = (int *)malloc(n * sizeof(int));
  assert(p != 0);
  p[0] = 1; p[1] = 2; p[2] = 3;

  long big = 2000000; // forces GC_realloc to allocate a new block
  int *q = (int *)realloc(p, big * sizeof(int));
  assert(q != 0);
  printf("%d\n", q[0]); // fine: reading through the fresh pointer

  printf("%d\n", p[0]); // use after (moving) realloc: p was invalidated
  return 0;
}
