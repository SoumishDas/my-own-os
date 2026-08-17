/* Prove descriptor stdin sleeps instead of reporting false end-of-file. */
#include "userlib.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char character;
    print("readcheck: press one key; this process is blocked meanwhile...\n");
    int amount = read(0, &character, 1);
    if (amount != 1)
    {
        print("readcheck: read failed\n");
        return 1;
    }
    print("readcheck: woke and read '");
    write(1, &character, 1);
    print("'\n");
    return 0;
}
