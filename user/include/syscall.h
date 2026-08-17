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

#define OPEN_READ_ONLY 0

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
    USER_SYS_EXECVE,
    USER_SYS_FORK,
    USER_SYS_EXIT,
    USER_SYS_WAITPID,
    USER_SYS_SIGNAL,
    USER_SYS_KILL,
    USER_SYS_SIGRETURN,
    USER_SYS_LSEEK,
    USER_SYS_FSTAT,
    USER_SYS_STAT,
    USER_SYS_ISATTY
};

#define SEEK_FROM_START   0
#define SEEK_FROM_CURRENT 1
#define SEEK_FROM_END     2

#define FILE_STATUS_REGULAR   1
#define FILE_STATUS_DIRECTORY 2
#define FILE_STATUS_CHARACTER 3

typedef struct file_status
{
    uint32_t type;
    uint32_t size;
    uint32_t inode;
    uint32_t mode;
} file_status_t;

int write(int descriptor, const void *buffer, uint32_t count);
int read(int descriptor, void *buffer, uint32_t count);
int open(const char *path, uint32_t flags);
int close(int descriptor);
int getpid(void);
int clear_screen(void);
int getchar_nonblocking(void);
int readdir_name(uint32_t index, char *name, uint32_t capacity);
uint32_t system_ticks(void);
uint32_t system_memory_kib(void);
int write_hex(uint32_t value);
int write_dec(uint32_t value);
int chdir(const char *path);
int getcwd(char *buffer, uint32_t capacity);
int mkdir(const char *path);
void *sbrk(int32_t increment);
int fork(void);
int execve(const char *path, char *const argv[], char *const envp[]);
int waitpid(int pid, int *status);
int lseek(int descriptor, int32_t offset, int origin);
int fstat_info(int descriptor, file_status_t *status);
int stat_info(const char *path, file_status_t *status);
int isatty(int descriptor);
void _exit(int status) __attribute__((noreturn));

#endif
