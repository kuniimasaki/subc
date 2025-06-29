#include <stdio.h>
#include <stdlib.h>

void sieve_of_eratosthenes(int n) {
    if (n < 2)return;
    int *A = (int *)malloc((long)(n + 1) * sizeof(int));

    // 初期化: すべて 1 (true)
    for (int i = 2; i < n-1; i++)
        *(A + i) = 1;
    for (int i = 2; i * i < n-1; i++)
        if (*(A + i)) 
        for (int j = i*i; j < n-1; j = j+i)
        *(A + j) = 0;


    // 結果を出力
    printf("%d:\n", n);
    for (int i = 2; i < n-1; i++)
    if (*(A + i))printf("%d ", i);
    //printf("\n");

    free(A);
}

int main(void) {
    int n = 35000;
    sieve_of_eratosthenes(n);
    return 0;
}
