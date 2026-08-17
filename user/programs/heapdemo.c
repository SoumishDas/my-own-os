/* Demonstrate that every exec image receives a fresh independent sbrk heap. */
#include "userlib.h"

int main(void)
{
    print("Hello from the userspace heap demonstration.\n");
    print_pid_line("heapdemo");

    unsigned char *first = (unsigned char*)sbrk(9000);
    if (first == (void*)-1)
    {
        print("heapdemo: sbrk failed.\n");
        return 1;
    }

    first[0] = 11;
    first[4096] = 22;
    first[8999] = 33;
    if (first[0] != 11 || first[4096] != 22 || first[8999] != 33)
    {
        print("heapdemo: page verification failed.\n");
        return 2;
    }

    print("heapdemo: three-page allocation passed.\n");
    return 0;
}
