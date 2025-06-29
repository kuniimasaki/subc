#include <stdio.h>
#include <assert.h>

// pointer-compare

int main() {
  int array[5] = { 0, 1, 2, 3, 4 };
  int brray[5] = { 0, 1, 2, 3, 4 };
  int *p = array + 2;
  int *q = array + 4;
  int *r = brray + 4;
  if (p > q) printf("compare KO\n");
  else       printf("compare OK\n");
  // illegal comparison
  if (p < r) printf("compare KO\n");
  else       printf("compare KO\n");
  assert(!"this should not be reached");
  return 0;
}
