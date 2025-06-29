#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// use-after-free-2

struct Link {
  int          data;
  struct Link *next;
};

struct Link *newLink(int data, struct Link *next) {
  struct Link *link =
     (struct Link *)malloc(sizeof(*link));
  link->data = data;
  link->next = next;
  return link;
}

int main()
{
  struct Link *list = 0;

  // create linked list of 10 elements
  for (int i = 0;  i < 10;  ++i)
    list = newLink(i*i, list);

  // print contents of list
  for (struct Link *ptr = list;
       ptr;
       ptr = ptr->next) {
    printf("%d\n", ptr->data);
  }

  // deallocate list
  for (struct Link *ptr = list;
       ptr;
       ptr = ptr->next) { // ptr is dangling here
    free(ptr);            // after this free
  }

  return 0;
}
