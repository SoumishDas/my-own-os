/*
 * syscall.h -- Ring-3 API for entering the kernel through interrupt 0x80.
 *
 * ABI: EAX contains the syscall number; EBX, ECX, EDX, ESI and EDI carry up
 * to five arguments; EAX carries the signed result.
 */
#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"

enum syscall_number {
    SYSCALL_WRITE = 0, SYSCALL_PUTCHAR, SYSCALL_CLEAR, SYSCALL_GETCHAR,
    SYSCALL_READDIR, SYSCALL_READFILE, SYSCALL_GETPID, SYSCALL_TICKS,
    SYSCALL_MEMORY_KIB, SYSCALL_WRITE_HEX, SYSCALL_WRITE_DEC,
    SYSCALL_OPEN, SYSCALL_FD_READ, SYSCALL_CLOSE, SYSCALL_FD_WRITE,
    SYSCALL_CHDIR, SYSCALL_GETCWD, SYSCALL_MKDIR, SYSCALL_SBRK,
    SYSCALL_EXECVE, SYSCALL_FORK, SYSCALL_EXIT, SYSCALL_WAITPID,
    SYSCALL_SIGNAL, SYSCALL_KILL, SYSCALL_SIGRETURN,
    SYSCALL_LSEEK, SYSCALL_FSTAT, SYSCALL_STAT, SYSCALL_ISATTY,
    SYSCALL_COUNT
};

/* Only read-only opens are accepted until the VFS gains writable storage. */
#define OPEN_READ_ONLY 0

#define SEEK_FROM_START   0
#define SEEK_FROM_CURRENT 1
#define SEEK_FROM_END     2

#define FILE_STATUS_REGULAR   1
#define FILE_STATUS_DIRECTORY 2
#define FILE_STATUS_CHARACTER 3

typedef struct syscall_file_status
{
    u32int type;
    u32int size;
    u32int inode;
    u32int mode;
} syscall_file_status_t;

void initialise_syscalls(void);
int syscall_write(const char *text);
int syscall_putchar(char character);
int syscall_clear(void);
int syscall_getchar(void);
int syscall_readdir(u32int index, char *name, u32int capacity);
int syscall_readfile(const char *name, char *buffer, u32int capacity);
int syscall_getpid(void);
u32int syscall_ticks(void);
u32int syscall_memory_kib(void);
int syscall_write_hex(u32int value);
int syscall_write_dec(u32int value);
int syscall_open(const char *path, u32int flags);
int syscall_fd_read(int fd, void *buffer, u32int count);
int syscall_close(int fd);
int syscall_fd_write(int fd, const void *buffer, u32int count);
int syscall_chdir(const char *path);
int syscall_getcwd(char *buffer, u32int capacity);
int syscall_mkdir(const char *path);

/*
 * Move this process's userspace heap boundary.  Like traditional sbrk(), the
 * result is the previous break, or (void*)-1 when the request is invalid.
 */
void *syscall_sbrk(s32int increment);

/* Replace the calling process image; returns only when loading fails. */
int syscall_execve(const char *path, char *const argv[], char *const envp[]);
int syscall_fork(void);
void syscall_exit(int status) __attribute__((noreturn));
int syscall_waitpid(int pid, int *status);

#endif
