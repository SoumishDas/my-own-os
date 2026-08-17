/*
 * Minimal userspace syscall veneer.  This is not yet a complete libc: it only
 * packages C arguments into the register ABI expected by interrupt 0x80.
 */
#include "syscall.h"
#include "signal.h"

/* Assembly entry used as the artificial return address of every handler. */
extern void __signal_trampoline(void);

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

int read(int descriptor, void *buffer, uint32_t count)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_READ), "b"((uint32_t)descriptor),
                   "c"((uint32_t)buffer), "d"(count)
                 : "memory");
    return result;
}

int open(const char *path, uint32_t flags)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_OPEN), "b"((uint32_t)path), "c"(flags)
                 : "memory");
    return result;
}

int close(int descriptor)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_CLOSE), "b"((uint32_t)descriptor)
                 : "memory");
    return result;
}

int clear_screen(void)
{
    int result;
    asm volatile("int $0x80" : "=a"(result) : "0"(USER_SYS_CLEAR) : "memory");
    return result;
}

int getchar_nonblocking(void)
{
    int result;
    asm volatile("int $0x80" : "=a"(result) : "0"(USER_SYS_GETCHAR) : "memory");
    return result;
}

int readdir_name(uint32_t index, char *name, uint32_t capacity)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_READDIR), "b"(index),
                   "c"((uint32_t)name), "d"(capacity)
                 : "memory");
    return result;
}

uint32_t system_ticks(void)
{
    uint32_t result;
    asm volatile("int $0x80" : "=a"(result) : "0"(USER_SYS_TICKS) : "memory");
    return result;
}

uint32_t system_memory_kib(void)
{
    uint32_t result;
    asm volatile("int $0x80" : "=a"(result) : "0"(USER_SYS_MEMORY_KIB) : "memory");
    return result;
}

int write_hex(uint32_t value)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_WRITE_HEX), "b"(value)
                 : "memory");
    return result;
}

int write_dec(uint32_t value)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_WRITE_DEC), "b"(value)
                 : "memory");
    return result;
}

int chdir(const char *path)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_CHDIR), "b"((uint32_t)path)
                 : "memory");
    return result;
}

int getcwd(char *buffer, uint32_t capacity)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_GETCWD), "b"((uint32_t)buffer), "c"(capacity)
                 : "memory");
    return result;
}

int mkdir(const char *path)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_MKDIR), "b"((uint32_t)path)
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

int fork(void)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_FORK)
                 : "memory");
    return result;
}

int execve(const char *path, char *const argv[], char *const envp[])
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_EXECVE), "b"((uint32_t)path),
                   "c"((uint32_t)argv), "d"((uint32_t)envp)
                 : "memory");
    return result;
}

int waitpid(int pid, int *status)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_WAITPID), "b"((uint32_t)pid),
                   "c"((uint32_t)status)
                 : "memory");
    return result;
}

int lseek(int descriptor, int32_t offset, int origin)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_LSEEK), "b"((uint32_t)descriptor),
                   "c"((uint32_t)offset), "d"((uint32_t)origin)
                 : "memory");
    return result;
}

int fstat_info(int descriptor, file_status_t *status)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_FSTAT), "b"((uint32_t)descriptor),
                   "c"((uint32_t)status)
                 : "memory");
    return result;
}

int stat_info(const char *path, file_status_t *status)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_STAT), "b"((uint32_t)path),
                   "c"((uint32_t)status)
                 : "memory");
    return result;
}

int isatty(int descriptor)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_ISATTY), "b"((uint32_t)descriptor)
                 : "memory");
    return result;
}

sighandler_t signal(int signal_number, sighandler_t handler)
{
    uint32_t result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_SIGNAL), "b"((uint32_t)signal_number),
                   "c"((uint32_t)handler), "d"((uint32_t)__signal_trampoline)
                 : "memory");
    return (sighandler_t)result;
}

int kill(int process_id, int signal_number)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_KILL), "b"((uint32_t)process_id),
                   "c"((uint32_t)signal_number)
                 : "memory");
    return result;
}

int raise(int signal_number)
{
    return kill(getpid(), signal_number);
}

/* Called only by signal.s; successful restoration never returns to C. */
int __sigreturn(void *context)
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(USER_SYS_SIGRETURN), "b"((uint32_t)context)
                 : "memory");
    return result;
}

void _exit(int status)
{
    asm volatile("int $0x80"
                 :
                 : "a"(USER_SYS_EXIT), "b"((uint32_t)status)
                 : "memory");
    /* The kernel never returns an exited process; retain a defensive fallback. */
    for (;;)
        asm volatile("pause");
}
