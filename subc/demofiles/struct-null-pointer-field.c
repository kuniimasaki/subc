#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// setMemory()'s Tpointer/Pointer-value branch only handled storing a
// pointer whose base is a Memory object (i.e. pointing at real heap/stack
// storage). Storing a pointer whose base is an Integer -- e.g. a typed
// NULL such as the initial value of `struct Link *list = 0;` -- into a
// struct field hit an unconditional assert(0). This is exactly what
// demofiles/use-after-free-2.c's very first `list->next = list;`-style
// linked-list construction does.

struct Link { int data; struct Link *next; };

int main() {
  struct Link *list = 0;
  struct Link *node = (struct Link *)malloc(sizeof(*node));
  assert(node != 0);
  node->data = 1;
  node->next = list; // storing a NULL (Integer-based) pointer into a struct field
  printf("%d\n", node->data);
  free(node);
  return 0;
}
