#include <stdio.h>
#include <assert.h>

// dangling-pointer

int *alloc() {
  int i;
  return &i;
}

int main() {
  int *ptr = alloc();
  assert(ptr != 0);
  *ptr = 42;
  printf("%d\n", *ptr);
  return 0;
}
