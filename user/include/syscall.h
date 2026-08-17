/*
 * Userspace view of the small int 0x80 ABI.
 *
 * This header deliberately contains no kernel structures.  Syscall numbers and
 * register assignments are the only contract shared across the privilege
 * boundary.  These wrappers are the seed from which a real libc will grow.
 */
#ifndef USER_SYSCALL_H
#define USER_SYSCALL_H

typedef unsigned int uint32_t;
typedef int int32_t;

enum user_syscall_number
{
    USER_SYS_WRITE_TEXT = 0,
    USER_SYS_PUTCHAR,
    USER_SYS_CLEAR,
    USER_SYS_GETCHAR,
    USER_SYS_READDIR,
    USER_SYS_READFILE,
    USER_SYS_GETPID,
    USER_SYS_TICKS,
    USER_SYS_MEMORY_KIB,
    USER_SYS_WRITE_HEX,
    USER_SYS_WRITE_DEC,
    USER_SYS_OPEN,
    USER_SYS_READ,
    USER_SYS_CLOSE,
    USER_SYS_WRITE,
    USER_SYS_CHDIR,
    USER_SYS_GETCWD,
    USER_SYS_MKDIR,
    USER_SYS_SBRK,
    USER_SYS_EXECVE
};

int write(int descriptor, const void *buffer, uint32_t count);
int getpid(void);
void *sbrk(int32_t increment);
void _exit(int status) __attribute__((noreturn));

#endif
