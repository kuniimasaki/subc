#include <stdio.h>
#include <stdlib.h>

int main() {
    // Allocate memory for 5 integers using malloc
    int *arr = (int *)malloc(sizeof(int));

    // Print values without initializing
    printf("Uninitialized values:\n%d\n",*arr);// Undefined behavior (garbage values)

    // Free the allocated memory
    free(arr);

    return 0;
}