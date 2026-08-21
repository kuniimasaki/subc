#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// A local pointer-typed declaration with an initializer must evaluate that
// initializer exactly once. initialiseVariable()'s Tpointer case was
// missing a `break;`, so execution fell through into the following
// `default:` case and evaluated the initializer a SECOND time, discarding
// the first (correctly type-checked/cast) result. For a non-idempotent
// initializer like malloc(), this orphaned the first allocation (reported
// as an unfreed leak even though the program frees the pointer it actually
// uses) and stored the raw un-cast second result instead.

int main() {
  int *p = (int *)malloc(sizeof(int));
  assert(p != 0);
  *p = 42;
  printf("%d\n", *p);
  free(p);
  return 0;
}
