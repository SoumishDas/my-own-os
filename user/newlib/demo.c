/* First program linked against an unmodified upstream Newlib libc.a. */
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

static volatile sig_atomic_t caught_signal;

static void catch_user_signal(int signal_number)
{
    caught_signal = signal_number;
}

int main(int argc, char **argv)
{
    printf("Newlib userspace program is PID-capable and running! argc=%d\n", argc);
    if (argc > 0)
        printf("argv[0] = %s\n", argv[0]);

    if (signal(SIGUSR1, catch_user_signal) == SIG_ERR ||
        raise(SIGUSR1) != 0 || caught_signal != SIGUSR1)
    {
        puts("Newlib signal/raise integration failed.");
        return 5;
    }
    puts("Newlib signal/raise/sigreturn passed.");

    char *allocation = malloc(6000);
    if (allocation == NULL)
    {
        perror("malloc");
        return 1;
    }
    strcpy(allocation, "malloc + strcpy passed");
    printf("%s at %p\n", allocation, (void*)allocation);

    FILE *file = fopen("/test.txt", "r");
    if (file == NULL)
    {
        perror("fopen /test.txt");
        free(allocation);
        return 2;
    }

    char line[64];
    if (fgets(line, sizeof(line), file) == NULL)
    {
        perror("fgets");
        fclose(file);
        free(allocation);
        return 3;
    }
    printf("fgets read: %s", line);

    if (fseek(file, 0, SEEK_SET) != 0)
    {
        perror("fseek");
        fclose(file);
        free(allocation);
        return 4;
    }

    fclose(file);
    free(allocation);
    puts("printf/malloc/fopen/fgets/fseek/free all passed.");
    return 0;
}
