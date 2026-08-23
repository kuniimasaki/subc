#include <stdlib.h>
int main(void) {
    long n = 3;
    int *arr = (int *)malloc(n * sizeof(int));
    int *p = arr;
    *p = 10;
    p = p + 1;
    *p = 20;
    p = p + 1;
    *p = 30;
    int sum = arr[0] + arr[1] + arr[2]; // 60
    int nullcheck = 0;
    if (p != 0) nullcheck = 1; // p is non-null
    int eqcheck = 0;
    if (arr == arr) eqcheck = 1; // same pointer
    free(arr);
    return sum + nullcheck + eqcheck; // 60+1+1 = 62
}
