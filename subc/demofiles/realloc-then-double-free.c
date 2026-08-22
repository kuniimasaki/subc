#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-then-double-free: after a moving realloc(), the OLD Memory block
// has its free-counter incremented exactly as if free() had been called on
// it (see prim_realloc). So freeing the NEW pointer (legitimate) and then
// separately freeing the stale OLD pointer must be reported as a double
// free of the same underlying block, just like calling free() twice on an
// ordinary malloc'd pointer.

int main() {
  long n = 3;
  int *p = (int *)malloc(n * sizeof(int));
  assert(p != 0);

  long big = 2000000; // forces a moving realloc
  int *q = (int *)realloc(p, big * sizeof(int));
  assert(q != 0);

  free(q); // frees the new, current allocation -- fine
  free(p); // p's underlying block was already marked freed by the move
  return 0;
}
