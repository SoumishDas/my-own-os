/* A tiny program whose nonzero return value verifies exit/wait propagation. */
#include "userlib.h"

int main(void)
{
    print("Hello from the exit-status demonstration.\n");
    print_pid_line("status");
    print("status: returning 42 to the parent shell.\n");
    return 42;
}
