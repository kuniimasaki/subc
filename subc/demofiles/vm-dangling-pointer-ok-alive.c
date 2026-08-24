#include <stdio.h>
#include <assert.h>

// same negative control as demofiles/dangling-pointer-ok-alive.c, run
// under -O: take the address of a local variable and use it through
// another function call while the variable's enclosing function is still
// on the call stack. Must NOT be flagged dead -- this specifically
// exercises that iRETURN's "kill locals bound since this call started"
// logic only kills the returning call's OWN locals, not the caller's.

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
