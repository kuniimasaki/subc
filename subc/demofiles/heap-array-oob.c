#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// heap-array-oob: indexing past the end of a malloc'd (heap) array, as
// opposed to the existing out-of-bounds-access.c/out-of-bounds-access-2.c
// demos, which only ever index stack arrays/variables.

int main() {
  long n = 5;
  int *a = (int *)malloc(n * sizeof(int));
  assert(a != 0);
  for (int i = 0; i < 5; i++) a[i] = i;
  printf("%d\n", a[4]);

  a[5] = 99; // one past the end of the heap block
  return 0;
}
