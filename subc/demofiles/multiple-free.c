#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// multiple-free

int main() {
  int *ptr = (int *)malloc(sizeof(*ptr));
  assert(ptr != 0);
  free(ptr);  printf("%p\n", ptr);
  free(ptr);  printf("%p\n", ptr);
  return 0;
}
