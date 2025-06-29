#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// dangling-pointer-2

int main() {
  int *ptr = (int *)malloc(sizeof(*ptr)); //malloc(8);
  assert(ptr != 0);
  *ptr = 42;
  printf("%d\n", *ptr);
  free(ptr);
  printf("%d\n", *ptr);
  return 0;
}
