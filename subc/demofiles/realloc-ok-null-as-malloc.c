#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// realloc-ok-null-as-malloc NEGATIVE control: realloc(NULL, n) is
// documented (see prim_realloc) to behave exactly like malloc(n). This
// must NOT be flagged, and the returned memory must be usable normally.
// NOTE: subc's own <stdlib.h> declares realloc's first parameter as
// 'void *', so the NULL pointer must actually carry that type -- write it
// as '(void *)0', not as a bare integer literal (which fails to type-check
// against a pointer parameter) and not as a typed 'int *' NULL (subc's
// isNull() check currently only recognises a NULL whose Pointer type is
// exactly 'void *'; see the verification report for details).

int main() {
  long n = 4;
  int *p = (int *)realloc((void *)0, n * sizeof(int));
  assert(p != 0);
  p[0] = 1; p[1] = 2; p[2] = 3; p[3] = 4;
  printf("%d %d %d %d\n", p[0], p[1], p[2], p[3]);
  free(p);
  return 0;
}
