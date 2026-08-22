#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-to-zero: realloc(p, 0) is ambiguous/deprecated in real libc (it
// may free and return NULL, or return NULL and leave p allocated,
// depending on the C library). subc deliberately rejects it outright with
// a clear, loud error instead of replicating that ambiguity.

int main() {
  int *p = (int *)malloc(sizeof(int));
  assert(p != 0);
  long zero = 0;
  int *q = (int *)realloc(p, zero);
  printf("should not be reached\n");
  return 0;
}
