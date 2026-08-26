// kitchen-sink: exercises as much of subc's implemented feature/library
// surface as possible in one program -- control flow (if/while/for/
// switch/break/continue), recursion, structs+typedef, arrays, pointer
// arithmetic, the heap (malloc/calloc/realloc/free, a small linked
// list), string.h, stdio.h (including sprintf/snprintf), stdlib.h,
// math.h, ctype.h, compound assignment, and int<->long/pointer casts.
// Used as an integration smoke test comparing the tree-walker and -O
// (`make kitchensink` vs `make kitchensinkvm`): both must print
// identical output. Found two real bugs during development (see
// CHANGELOG.md 2026-08-24/25): getMemory()'s Tpointer case losing heap-
// allocation tracking when reading a pointer back out of a struct field
// (`head = head->next; free(head);`), and a rare, GC-timing-sensitive
// -O-only crash from execute()'s per-call stack/frame arrays being
// freshly GC_malloc'd and abandoned on every single top-level
// declaration's own throwaway execute() call.
//
// Known subset limitations this file deliberately avoids exercising
// (see the design docs for details, not bugs introduced by this file):
// no int<->float cross-family casts (converter() has no such table
// entries; mydemo/calc.c documents this separately), no do-while/
// ternary/enum/union/goto, arrays must be passed as &arr[0] rather than
// their bare name, string literals must be assigned to a char* variable
// before being passed as a function argument.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

typedef struct Point { int x, y; } Point;

struct Node {
    int value;
    struct Node *next;
};

int fib(int n) {
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}

int sum_array(int *arr, int n) {
    int total = 0;
    int i;
    for (i = 0; i < n; i = i + 1) total = total + arr[i];
    return total;
}

struct Node *push(struct Node *head, int value) {
    struct Node *node = (struct Node *)malloc(sizeof(struct Node));
    node->value = value;
    node->next = head;
    return node;
}

int main() {
    int i;
    int total = 0;
    for (i = 0; i < 10; i = i + 1) {
        if (i % 2 == 0) continue;
        if (i == 9) break;
        total += i;
    }
    printf("loop total=%d\n", total);

    switch (total) {
        case 16: printf("switch: sixteen\n"); break;
        default: printf("switch: other\n"); break;
    }

    printf("fib(10)=%d\n", fib(10));

    int arr[5];
    for (i = 0; i < 5; i = i + 1) arr[i] = i * i;
    printf("sum_array=%d\n", sum_array(&arr[0], 5));
    int *p = &arr[0];
    printf("*(p+2)=%d\n", *(p+2));

    Point pt;
    pt.x = 3;
    pt.y = 4;
    printf("point=(%d,%d)\n", pt.x, pt.y);

    struct Node *head = 0;
    for (i = 0; i < 3; i = i + 1) head = push(head, i);
    int listsum = 0;
    struct Node *cur = head;
    while (cur) {
        listsum = listsum + cur->value;
        cur = cur->next;
    }
    printf("listsum=%d\n", listsum);
    while (head) {
        struct Node *dead = head;
        head = head->next;
        free(dead);
    }

    int *heapArr = (int *)calloc((long)5, sizeof(int));
    heapArr = (int *)realloc(heapArr, (long)10 * sizeof(int));
    heapArr[9] = 42;
    printf("heapArr[9]=%d\n", heapArr[9]);
    free(heapArr);

    char buf[64];
    char *hello = "hello";
    char *world = " world";
    strcpy(&buf[0], hello);
    strcat(&buf[0], world);
    printf("strcat=%s strlen=%ld\n", &buf[0], strlen(&buf[0]));
    char *a = "abc";
    printf("strcmp=%d\n", strcmp(a, a));
    char src[4];
    src[0]=1; src[1]=2; src[2]=3; src[3]=4;
    char dst[4];
    memcpy(&dst[0], &src[0], (long)4);
    printf("memcpy_ok=%d\n", dst[3]==4);
    char mbuf[4];
    memset(&mbuf[0], 9, (long)4);
    printf("memset_ok=%d\n", mbuf[0]==9 && mbuf[3]==9);

    char sbuf[32];
    int n = sprintf(&sbuf[0], "n=%d f=%.2f", 7, 2.5);
    printf("sprintf=%s (n=%d)\n", &sbuf[0], n);
    char tbuf[8];
    int n2 = snprintf(&tbuf[0], (long)8, "this will be truncated");
    printf("snprintf=%s wouldbe=%d\n", &tbuf[0], n2);
    putchar('O'); putchar('K'); putchar('\n');
    puts(hello);

    char *num1 = "123";
    char *num2 = "456";
    printf("atoi=%d atol=%ld\n", atoi(num1), atol(num2));
    char *fnum = "3.5";
    printf("atof=%f\n", atof(fnum));
    printf("abs=%d\n", abs(-9));
    srand(7);
    int r1 = rand();
    srand(7);
    int r2 = rand();
    printf("rand_deterministic=%d\n", r1==r2);

    printf("sqrtf=%f fabsf=%f floorf=%f ceilf=%f powf=%f\n",
           sqrtf(9.0), fabsf(-2.5), floorf(1.9), ceilf(1.1), powf(2.0, 5.0));

    printf("isalpha=%d isdigit=%d isupper=%d toupper=%c\n",
           isalpha('z'), isdigit('9'), isupper('A'), toupper('m'));

    int bits = 5;
    bits <<= 2;
    bits |= 1;
    bits &= 0xFF;
    printf("bits=%d\n", bits);
    long li = 42;
    int ci = (int)li;
    printf("cast=%d\n", ci);

    return 0;
}
