#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// heap-array-ok-inbounds NEGATIVE control: index strictly within a
// malloc'd array's extent. This must NOT be flagged.

int main() {
  long n = 5;
  int *a = (int *)malloc(n * sizeof(int));
  assert(a != 0);
  for (int i = 0; i < 5; i++) a[i] = i;
  printf("%d\n", a[4]); // last valid index
  free(a);
  return 0;
}
