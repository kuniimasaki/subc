#include <stdlib.h>

// vm-pointer-roundtrip-ok: Task 3's core deliverable -- pointer read/write
// through the bytecode VM (-O), not just the tree-walker. Dereference and
// Index compile down to Call nodes targeting prim_vm_deref/prim_vm_index/
// prim_vm_store_deref/prim_vm_store_index (docs/design/task3-vm-audit-and-
// design.md's "Option B"), which are thin wrappers around the exact same
// getPointer()/setMemory()/getArray()/setArray()/setPointer() the tree-
// walker's eval()/assign() call. malloc() returns a void*-typed pointer;
// compileOn's VarDecls case now coerces it to the declared int* via the
// same cast initialiseVariable() already does for the tree-walker.

int main() {
  int *p = malloc(sizeof(int));
  *p = 42;
  int v = *p;
  free(p);
  return v;
}
