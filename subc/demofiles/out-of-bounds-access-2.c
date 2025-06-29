#include <stdio.h>

// out-of-bounds-access-2

int main() {
  int i = 0, j = 42, k = 0, *pj = &j;
  printf("%d\n", *pj);
  printf("%d\n", pj[0]);
  printf("%d\n", *(pj+1)); // out of bounds
  printf("%d\n", pj[1]);   // out of bounds
  return 0;
}
