#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *hello = "hello";

int main(void)
{
    int *num = (int *)malloc(sizeof(int));
    if (num == NULL) {
        exit(1);
    }

    *num = 42;
    printf("Num Init: %d\n", *num);
    free(num);
    num = NULL;

    int *num_of_three = (int *)malloc(sizeof(int) * 3);
    if (num_of_three == NULL) {
        exit(1);
    }

    int nums[3] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        num_of_three[i] = nums[i];
        printf("Num of three: idx - %d \n value - %d \n", i, num_of_three[i]);
    }

    size_t hello_len = strlen(hello);
    char *copy = malloc(hello_len + 1);

    if (copy == NULL) {
        exit(1);
    }
    strcpy(copy, hello);
    printf("%s\n", copy);
    free(copy);
    copy = NULL;
}
