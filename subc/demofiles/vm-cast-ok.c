#include <stdlib.h>
int main(void) {
    int *p = (int *)malloc(sizeof(int));
    *p = 7;
    int v = *p;
    long n = 42;
    int m = (int)n;
    free(p);
    return v + m - 49;
}
