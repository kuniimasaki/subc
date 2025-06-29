#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// use-after-free

int main() {
  int *ptr = (int *)malloc(sizeof(*ptr));
  assert(ptr != 0);
  *ptr = 42;
  printf("%d\n", *ptr);
  free(ptr);
  printf("%d\n", *ptr);	// use after free
  *ptr = 43;		// use after free
  return 0;
}
