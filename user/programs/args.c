/* Print the PID and every argument installed by execve's initial stack. */
#include "userlib.h"

int main(int argc, char **argv)
{
    print("Hello from the argument demonstration program.\n");
    print_pid_line("args");
    print("argc = ");
    print_unsigned((uint32_t)argc);
    print("\n");

    for (int index = 0; index < argc; index++)
    {
        print("  argv[");
        print_unsigned((uint32_t)index);
        print("] = ");
        print(argv[index]);
        print("\n");
    }
    return 0;
}
