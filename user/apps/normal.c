/* A normal hosted C program: no freestanding flags or private runtime API. */
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    printf("normal: compiled automatically with i686-myos-gcc\n");
    printf("normal: argc=%d, argv[0]=%s\n", argc, argv[0]);

    int *numbers = malloc(4 * sizeof(*numbers));
    if (numbers == NULL)
    {
        perror("normal: malloc");
        return 1;
    }
    for (int index = 0; index < 4; index++)
        numbers[index] = index * index;
    printf("normal: malloc works; last square=%d\n", numbers[3]);
    free(numbers);
    return 0;
}
