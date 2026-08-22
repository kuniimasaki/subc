#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-ok-inplace-alias-still-valid NEGATIVE control: when realloc()
// can satisfy the request without moving the block (e.g. a small shrink
// that still fits the allocator's existing size class), prim_realloc
// mutates the SAME Memory object in place rather than allocating a new
// one. Every existing alias of that block -- including ones derived via
// earlier pointer arithmetic, not just the pointer realloc() was called
// with -- must remain perfectly valid afterward. This must NOT be flagged.

int main() {
  long n = 100;
  int *p = (int *)malloc(n * sizeof(int));
  assert(p != 0);
  int *alias = p + 1; // an alias derived by pointer arithmetic
  p[0] = 111; p[1] = 222;

  long n2 = 99; // small shrink: stays within the same allocator size class
  int *q = (int *)realloc(p, n2 * sizeof(int));
  assert(q != 0);
  assert(q == p); // confirms this really did stay in place

  printf("%d\n", p[0]);     // original pointer still valid
  printf("%d\n", alias[0]); // earlier alias (p + 1) still valid too
  return 0;
}
