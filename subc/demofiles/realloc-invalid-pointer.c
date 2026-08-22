#include <stdlib.h>
#include <stdio.h>

// realloc-invalid-pointer: realloc() on a pointer that was never returned
// by malloc/calloc/realloc (here: the address of a stack variable) must be
// rejected, mirroring free()'s existing "attempt to free pointer to
// variable" check (see invalid-free.c).

int main() {
  int x;
  int *p = &x;
  long n = 8;
  int *q = (int *)realloc(p, n);
  printf("should not be reached\n");
  return 0;
}
