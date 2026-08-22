#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-shrink-then-oob: a shrinking realloc() mutates the block's
// recorded Memory.size (whether or not the underlying allocation actually
// moves), so every existing bounds check that compares against
// Memory.size (here: setMemory(), used for a pointer-indexed write)
// correctly starts rejecting accesses beyond the new, smaller size.  An
// index that was perfectly valid before the shrink must become illegal
// after it.

int main() {
  long n = 5;
  int *p = (int *)malloc(n * sizeof(int));
  assert(p != 0);
  p[0]=1; p[1]=2; p[2]=3; p[3]=4; p[4]=5;

  long smaller = 2;
  int *q = (int *)realloc(p, smaller * sizeof(int));
  assert(q != 0);
  q[0] = 10; // still in bounds (index 0 < 2)
  q[1] = 20; // still in bounds (index 1 < 2)
  printf("%d %d\n", q[0], q[1]);

  q[2] = 99; // out of bounds now that the block has shrunk to 2 elements
  return 0;
}
