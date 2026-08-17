/* Verify the file/terminal calls required beneath Newlib stdio. */
#include "userlib.h"

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    int failures = 0;
    file_status_t status;

    if (isatty(0) != 1 || isatty(1) != 1)
    {
        print("abicheck: terminal detection failed\n");
        failures++;
    }
    if (stat_info("/test.txt", &status) < 0 ||
        status.type != FILE_STATUS_REGULAR || status.size != 17)
    {
        print("abicheck: stat failed\n");
        failures++;
    }

    int descriptor = open("/test.txt", OPEN_READ_ONLY);
    if (descriptor < 0 || fstat_info(descriptor, &status) < 0 ||
        status.size != 17)
    {
        print("abicheck: open/fstat failed\n");
        failures++;
    }
    else
    {
        char first[5];
        if (read(descriptor, first, 4) != 4 ||
            lseek(descriptor, 0, SEEK_FROM_START) != 0 ||
            read(descriptor, first, 4) != 4)
        {
            print("abicheck: read/lseek failed\n");
            failures++;
        }
        close(descriptor);
    }

    if (lseek(1, 0, SEEK_FROM_START) != -29)
    {
        print("abicheck: console did not report ESPIPE\n");
        failures++;
    }

    print(failures == 0 ? "abicheck: Newlib kernel ABI passed\n" :
                          "abicheck: failures detected\n");
    return failures;
}
