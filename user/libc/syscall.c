/*
 * Minimal userspace syscall veneer.  This is not yet a complete libc: it only
 * packages C arguments into the register ABI expected by interrupt 0x80.
 */
#include "syscall.h"

int write(int descriptor, const void *buffer, uint32_t count)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_WRITE), "b"((uint32_t)descriptor),
                   "c"((uint32_t)buffer), "d"(count)
                 : "memory");
    return result;
}

int getpid(void)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_GETPID)
                 : "memory");
    return result;
}

void *sbrk(int32_t increment)
{
    uint32_t result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_SBRK), "b"((uint32_t)increment)
                 : "memory");
    return (void*)result;
}

void _exit(int status)
{
    /*
     * SYS_EXIT and process states are the next process-lifecycle milestone.
     * Until then a returned main() becomes an inert, preemptible user loop.
     */
    (void)status;
    for (;;)
        asm volatile("pause");
}
