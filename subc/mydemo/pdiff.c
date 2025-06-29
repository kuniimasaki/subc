#include <stdio.h>

int main() {
    int arr1[3] = {1, 2, 3};
    int arr2[3] = {4, 5, 6};

    int *p1 = &arr1[0];
    int *p2 = &arr2[0];

    printf("%d\n", p2 - p1);  // ❌ Undefined behavior
    return 0;
}