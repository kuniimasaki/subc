#include <stdio.h>

// compare() tri-state bug: comparing an int-cast pointer ((int*)N) against
// a plain integer used code copy-pasted from equal() that returned a 0/1
// boolean instead of a -1/0/1 ordering, so '<' and '>' against such a
// pointer were always false regardless of the actual values.

int main() {
  int *p = (int *)0 + 50; // Pointer{base=Integer(0), offset=50} -> effective value 50*sizeof(int) = 200

  if (p > 100)
    printf("greater\n");
  else
    printf("not greater\n");

  if (p < 100)
    printf("less\n");
  else
    printf("not less\n");

  return 0;
}
