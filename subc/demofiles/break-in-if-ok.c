#include <stdio.h>

// break-in-if-ok: typeCheck() had no case for Continue or Break at all --
// meaning even the most basic `while (1) { if (x) break; }` crashed with
// "cannot convert Break to string" (typeCheck's generic fallback tried to
// print the unhandled node in a diagnostic message, and toStringOn had no
// case for it either, so even the crash message itself failed to render).
// Confirmed this predates any change this session (main_orig crashes
// identically). A `break`/`continue` as the direct, unblocked consequent
// of an `if` (not wrapped in its own `{ }` block) reaches typeCheck() as
// a bare Continue/Break node, which is exactly what was unhandled.

int main() {
  int i = 0;
  while (1) {
    if (i == 3) break;
    i++;
  }
  printf("%d\n", i);
  return 0;
}
