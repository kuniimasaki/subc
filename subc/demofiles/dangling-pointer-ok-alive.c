#include <stdio.h>
#include <assert.h>

// dangling-pointer NEGATIVE control: take the address of a local variable
// and use it while that variable is still in scope (its enclosing function
// has not returned). This must NOT be flagged as dangling.

void touch(int *p) {
  *p = 42;
}

int main() {
  int i;
  int *ptr = &i;
  touch(ptr); // i is still alive here; ptr is not dangling
  printf("%d\n", *ptr);
  return 0;
}
