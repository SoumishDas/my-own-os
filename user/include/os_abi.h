/*
 * os_abi.h -- Declaration-free int 0x80 contract for Newlib glue.
 *
 * It intentionally does not declare read/open/write, because Newlib declares
 * those names itself. Keep this number list synchronized with src/syscall.h.
 */
#ifndef USER_OS_ABI_H
#define USER_OS_ABI_H

enum os_syscall_number
{
    OS_SYS_WRITE_TEXT = 0, OS_SYS_PUTCHAR, OS_SYS_CLEAR, OS_SYS_GETCHAR,
    OS_SYS_READDIR, OS_SYS_READFILE, OS_SYS_GETPID, OS_SYS_TICKS,
    OS_SYS_MEMORY_KIB, OS_SYS_WRITE_HEX, OS_SYS_WRITE_DEC,
    OS_SYS_OPEN, OS_SYS_READ, OS_SYS_CLOSE, OS_SYS_WRITE,
    OS_SYS_CHDIR, OS_SYS_GETCWD, OS_SYS_MKDIR, OS_SYS_SBRK,
    OS_SYS_EXECVE, OS_SYS_FORK, OS_SYS_EXIT, OS_SYS_WAITPID,
    OS_SYS_SIGNAL, OS_SYS_KILL, OS_SYS_SIGRETURN,
    OS_SYS_LSEEK, OS_SYS_FSTAT, OS_SYS_STAT, OS_SYS_ISATTY
};

#define OS_OPEN_READ_ONLY 0
#define OS_SEEK_SET 0
#define OS_SEEK_CUR 1
#define OS_SEEK_END 2

#define OS_FILE_REGULAR   1
#define OS_FILE_DIRECTORY 2
#define OS_FILE_CHARACTER 3

typedef struct os_file_status
{
    unsigned int type;
    unsigned int size;
    unsigned int inode;
    unsigned int mode;
} os_file_status_t;

#endif
