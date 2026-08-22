#include <stdlib.h>

// vm-use-after-free: confirms the core memory-safety research goal --
// use-after-free detection -- actually fires when running through the
// bytecode VM (-O), not just the tree-walker. Reading through a freed
// pointer via prim_vm_deref (which calls getPointer(), which calls
// requireNotFreed()) must be caught exactly like the tree-walker's
// demofiles/use-after-free.c is, with the identical message -- this is
// the whole point of Option B: one checked code path, reached from
// either engine.

int main() {
  int *p = malloc(sizeof(int));
  *p = 42;
  free(p);
  int v = *p; // use after free
  return v;
}
