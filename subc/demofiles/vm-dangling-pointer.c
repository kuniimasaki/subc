#include <stdio.h>
#include <assert.h>

// same bug as demofiles/dangling-pointer.c, run under -O: a pointer to a
// local variable escapes the function's return, so the local is dead by
// the time the caller writes through it.

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
