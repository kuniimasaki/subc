#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// write-after-free: the write-through-pointer path (setMemory() / the
// direct "*p = rhs" assignment path) must catch this exactly like
// getPointer() already catches a read-after-free, even when the freed
// memory is never read first.

int main() {
  int *ptr = (int *)malloc(sizeof(*ptr));
  assert(ptr != 0);
  free(ptr);
  *ptr = 42;		// use after free (write only, no read first)
  printf("%d\n", *ptr);
  return 0;
}
