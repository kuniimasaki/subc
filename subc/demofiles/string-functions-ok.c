#include <stdio.h>
#include <string.h>

// string-functions-ok: correctness check for the string.h primitives
// (strlen/strcpy/strcat/strcmp/memcpy/memset). Note the syntax this
// subset language actually requires: a string literal must first be
// assigned to a declared char* (`char *hello = "hello";`) rather than
// passed directly as a call argument, and a local array must be passed
// as `&arr[0]` rather than its bare name (arrays don't implicitly decay
// to pointers for strict non-variadic argument type matching here).

int main() {
    char buf[20];
    char *hello = "hello";
    char *world = " world";
    strcpy(&buf[0], hello);
    strcat(&buf[0], world);
    printf("%s %ld\n", &buf[0], strlen(&buf[0]));

    char *abc = "abc";
    char *abd = "abd";
    printf("%d %d %d\n", strcmp(abc, abc), strcmp(abc, abd) < 0, strcmp(abd, abc) > 0);

    int i;
    char src[5];
    for (i = 0; i < 5; i = i + 1) src[i] = i + 1;
    char dst[5];
    memcpy(&dst[0], &src[0], (long)5);
    printf("%d %d\n", dst[0] == 1, dst[4] == 5);

    char mbuf[10];
    memset(&mbuf[0], 65, (long)10);
    printf("%d %d\n", mbuf[0] == 'A', mbuf[9] == 'A');

    return 0;
}
