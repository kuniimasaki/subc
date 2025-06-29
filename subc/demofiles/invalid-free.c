#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// invalid-free

int *alloc() {
  int i;
  return &i;
}

int main() {
  int *ptr = alloc();
  free(ptr);
  return 0;
}
