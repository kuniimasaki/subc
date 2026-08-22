#include <stdio.h>

// switch-ok: subc's grammar had no switch/case/default at all. Implemented
// as a flat statement list (Case/Default are just marker nodes within it),
// scanned once to find the matching entry point, then executed straight
// through -- giving real C fallthrough semantics for free, with `break`
// (via the existing NLR_BREAK mechanism) exiting the switch, and
// `continue` correctly propagating past the switch to the nearest
// enclosing loop rather than being caught by the switch itself.

int classify(int x) {
  switch (x) {
    case 1:
      printf("one\n");
      break;
    case 2:
    case 3:
      printf("two-or-three\n");
      break;
    default:
      printf("other\n");
      break;
  }
  return 0;
}

int fallthrough(int x) {
  int result = 0;
  switch (x) {
    case 0:
      result += 1;
      // fall through
    case 1:
      result += 10;
      break;
    case 2:
      result += 100;
      break;
  }
  return result;
}

int main() {
  classify(1);
  classify(2);
  classify(3);
  classify(99);
  printf("%d\n", fallthrough(0)); // 11: falls through case 0 into case 1
  printf("%d\n", fallthrough(1)); // 10
  printf("%d\n", fallthrough(2)); // 100

  // continue (inside the switch) must target the enclosing while, not be
  // caught by the switch; break (inside the switch) must exit only the
  // switch, not the while.
  int i = 5, total = 0;
  while (i > 0) {
    switch (i) {
      case 3:
        i--;
        continue;
      case 1:
        break;
    }
    total += i;
    i--;
  }
  printf("%d\n", total); // 12

  return 0;
}
