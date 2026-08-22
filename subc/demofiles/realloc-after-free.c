#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-after-free: calling realloc() on a pointer that was already
// free()'d must be caught (the block's Memory.free counter is already
// set), the same way a second free() would be -- it must not silently
// resurrect or re-allocate the freed block.

int main() {
  int *p = (int *)malloc(sizeof(int));
  assert(p != 0);
  free(p);

  long n = 4;
  int *q = (int *)realloc(p, n * sizeof(int));
  printf("should not be reached\n");
  return 0;
}
