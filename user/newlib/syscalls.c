/*
 * Newlib system-call glue for this OS.
 *
 * Newlib implements malloc, printf, FILE, strings, math, and most of libc in
 * ordinary userspace code. Functions whose names begin with '_' are the small
 * machine/OS boundary it expects us to provide. Each wrapper below enters the
 * existing int 0x80 ABI and translates the kernel's negative errno result into
 * Newlib's conventional `-1` plus thread-local `errno`.
 *
 * Compile this file with the installed Newlib headers, not as part of the tiny
 * pre-Newlib runtime. See README.md beside this file.
 */
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>

#include "os_abi.h"

extern void __signal_trampoline(void);

static int os_call0(int number)
{
    int result;
    __asm__ volatile("int $0x80" : "=a"(result) : "0"(number) : "memory");
    return result;
}

static int os_call1(int number, uintptr_t first)
{
    int result;
    __asm__ volatile("int $0x80"
                     : "=a"(result)
                     : "0"(number), "b"(first)
                     : "memory");
    return result;
}

static int os_call2(int number, uintptr_t first, uintptr_t second)
{
    int result;
    __asm__ volatile("int $0x80"
                     : "=a"(result)
                     : "0"(number), "b"(first), "c"(second)
                     : "memory");
    return result;
}

static int os_call3(int number, uintptr_t first, uintptr_t second,
                    uintptr_t third)
{
    int result;
    __asm__ volatile("int $0x80"
                     : "=a"(result)
                     : "0"(number), "b"(first), "c"(second), "d"(third)
                     : "memory");
    return result;
}

static int newlib_result(int kernel_result)
{
    if (kernel_result < 0)
    {
        errno = -kernel_result;
        return -1;
    }
    return kernel_result;
}

ssize_t _write(int descriptor, const void *buffer, size_t count)
{
    return (ssize_t)newlib_result(os_call3(OS_SYS_WRITE,
        (uintptr_t)descriptor, (uintptr_t)buffer, (uintptr_t)count));
}

ssize_t _read(int descriptor, void *buffer, size_t count)
{
    return (ssize_t)newlib_result(os_call3(OS_SYS_READ,
        (uintptr_t)descriptor, (uintptr_t)buffer, (uintptr_t)count));
}

int _open(const char *path, int flags, ...)
{
    /* Persistent writes do not exist yet; fail honestly instead of discarding. */
    if ((flags & O_ACCMODE) != O_RDONLY || (flags & O_CREAT) != 0)
    {
        errno = EROFS;
        return -1;
    }
    return newlib_result(os_call2(OS_SYS_OPEN, (uintptr_t)path,
                                  OS_OPEN_READ_ONLY));
}

int _close(int descriptor)
{
    return newlib_result(os_call1(OS_SYS_CLOSE, (uintptr_t)descriptor));
}

off_t _lseek(int descriptor, off_t offset, int origin)
{
    return (off_t)newlib_result(os_call3(OS_SYS_LSEEK, (uintptr_t)descriptor,
                                         (uintptr_t)offset, (uintptr_t)origin));
}

static void translate_status(const os_file_status_t *source,
                             struct stat *destination)
{
    /* A compound literal initializes every target-specific padding field too. */
    *destination = (struct stat){0};
    destination->st_ino = (ino_t)source->inode;
    destination->st_size = (off_t)source->size;
    destination->st_nlink = 1;
    destination->st_mode = (mode_t)source->mode;
    if (source->type == OS_FILE_REGULAR) destination->st_mode |= S_IFREG;
    else if (source->type == OS_FILE_DIRECTORY) destination->st_mode |= S_IFDIR;
    else if (source->type == OS_FILE_CHARACTER) destination->st_mode |= S_IFCHR;
}

int _fstat(int descriptor, struct stat *status)
{
    os_file_status_t compact;
    int result = newlib_result(os_call2(OS_SYS_FSTAT, (uintptr_t)descriptor,
                                        (uintptr_t)&compact));
    if (result == 0)
        translate_status(&compact, status);
    return result;
}

int _stat(const char *path, struct stat *status)
{
    os_file_status_t compact;
    int result = newlib_result(os_call2(OS_SYS_STAT, (uintptr_t)path,
                                        (uintptr_t)&compact));
    if (result == 0)
        translate_status(&compact, status);
    return result;
}

int _isatty(int descriptor)
{
    int result = os_call1(OS_SYS_ISATTY, (uintptr_t)descriptor);
    if (result < 0)
        return newlib_result(result);
    if (result == 0)
        errno = ENOTTY;
    return result;
}

void *_sbrk(ptrdiff_t increment)
{
    intptr_t result = (intptr_t)os_call1(OS_SYS_SBRK, (uintptr_t)increment);
    if (result == -1)
    {
        errno = ENOMEM;
        return (void*)-1;
    }
    return (void*)result;
}

int _getpid(void)
{
    return os_call0(OS_SYS_GETPID);
}

int _kill(int process_id, int signal_number)
{
    return newlib_result(os_call2(OS_SYS_KILL, (uintptr_t)process_id,
                                  (uintptr_t)signal_number));
}

typedef void (*os_signal_handler_t)(int);

os_signal_handler_t signal(int signal_number, os_signal_handler_t handler)
{
    int result = os_call3(OS_SYS_SIGNAL, (uintptr_t)signal_number,
                          (uintptr_t)handler,
                          (uintptr_t)__signal_trampoline);
    return result < 0 ? (os_signal_handler_t)SIG_ERR :
                        (os_signal_handler_t)(uintptr_t)result;
}

int raise(int signal_number)
{
    return _kill(_getpid(), signal_number);
}

/* Called from user/signal.s after a C handler returns. */
int __sigreturn(void *context)
{
    return os_call1(OS_SYS_SIGRETURN, (uintptr_t)context);
}

clock_t _times(struct tms *times)
{
    clock_t ticks = (clock_t)os_call0(OS_SYS_TICKS);
    if (times != NULL)
    {
        times->tms_utime = ticks;
        times->tms_stime = 0;
        times->tms_cutime = 0;
        times->tms_cstime = 0;
    }
    return ticks;
}

int _gettimeofday(struct timeval *time_value, void *timezone_value)
{
    (void)timezone_value;
    if (time_value == NULL)
    {
        errno = EFAULT;
        return -1;
    }
    unsigned int ticks = (unsigned int)os_call0(OS_SYS_TICKS);
    time_value->tv_sec = (time_t)(ticks / 50U);
    time_value->tv_usec = (suseconds_t)((ticks % 50U) * 20000U);
    return 0; /* Monotonic time since boot; no real-time clock exists yet. */
}

int _fork(void)
{
    return newlib_result(os_call0(OS_SYS_FORK));
}

int _execve(const char *path, char *const argv[], char *const envp[])
{
    return newlib_result(os_call3(OS_SYS_EXECVE, (uintptr_t)path,
                                  (uintptr_t)argv, (uintptr_t)envp));
}

int _waitpid(int process_id, int *status, int options)
{
    if (options != 0)
    {
        errno = EINVAL;
        return -1;
    }
    return newlib_result(os_call2(OS_SYS_WAITPID, (uintptr_t)process_id,
                                  (uintptr_t)status));
}

int _link(const char *old_path, const char *new_path)
{
    (void)old_path;
    (void)new_path;
    errno = EROFS;
    return -1;
}

int _unlink(const char *path)
{
    (void)path;
    errno = EROFS;
    return -1;
}

void _exit(int status)
{
    os_call1(OS_SYS_EXIT, (uintptr_t)status);
    for (;;)
        __asm__ volatile("pause");
}

/*
 * Newlib 4.6's generic reentrant wrappers reference the unprefixed hooks when
 * configured with --disable-newlib-supplied-syscalls. Keep forwarding symbols
 * as well as the traditional underscored spellings so this file also works
 * with older Newlib layouts and common libgloss expectations.
 */
ssize_t write(int fd, const void *buffer, size_t count)
{ return _write(fd, buffer, count); }
ssize_t read(int fd, void *buffer, size_t count)
{ return _read(fd, buffer, count); }
int open(const char *path, int flags, ...)
{ return _open(path, flags); }
int close(int fd)
{ return _close(fd); }
off_t lseek(int fd, off_t offset, int origin)
{ return _lseek(fd, offset, origin); }
int fstat(int fd, struct stat *status)
{ return _fstat(fd, status); }
int stat(const char *path, struct stat *status)
{ return _stat(path, status); }
int isatty(int fd)
{ return _isatty(fd); }
void *sbrk(ptrdiff_t increment)
{ return _sbrk(increment); }
int getpid(void)
{ return _getpid(); }
int kill(int pid, int signal_number)
{ return _kill(pid, signal_number); }
