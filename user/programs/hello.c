/* First independently linked program loaded from /bin/hello by execve(). */
#include "userlib.h"

/* Force the ELF to contain both initialized data and zero-filled BSS. */
static uint32_t initialized_data_cookie = 0x13579BDF;
static uint32_t zero_filled_bss_cookie;

int main(int argc, char **argv)
{
    print("External ELF32 userspace program is running.\n");

    if (initialized_data_cookie == 0x13579BDF && zero_filled_bss_cookie == 0)
        print("ELF data/BSS test: initialized and zero-filled bytes passed.\n");
    else
        print("ELF data/BSS test: segment contents are wrong.\n");
    print_pid_line("hello");

    print("argc = ");
    print_unsigned((uint32_t)argc);
    print("\n");
    for (int index = 0; index < argc; index++)
    {
        print("argv[");
        print_unsigned((uint32_t)index);
        print("] = ");
        print(argv[index]);
        print("\n");
    }

    /* Prove that exec installed a fresh heap derived from the ELF image end. */
    unsigned char *memory = (unsigned char*)sbrk(5000);
    if (memory == (void*)-1)
        print("ELF heap test: sbrk failed.\n");
    else
    {
        memory[0] = 0x3C;
        memory[4999] = 0xC3;
        if (memory[0] == 0x3C && memory[4999] == 0xC3)
            print("ELF heap test: two writable pages passed.\n");
        else
            print("ELF heap test: memory verification failed.\n");
    }

    print("hello is returning status 0 through SYS_EXIT.\n");
    return 0;
}
