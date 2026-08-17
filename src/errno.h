/* errno.h -- Numeric errors returned negated across the kernel syscall ABI. */
#ifndef KERNEL_ERRNO_H
#define KERNEL_ERRNO_H

#define ERRNO_EPERM    1
#define ERRNO_ENOENT   2
#define ERRNO_ESRCH    3
#define ERRNO_EINTR    4
#define ERRNO_EIO      5
#define ERRNO_ENOEXEC  8
#define ERRNO_EBADF    9
#define ERRNO_ECHILD  10
#define ERRNO_EAGAIN  11
#define ERRNO_ENOMEM  12
#define ERRNO_EACCES  13
#define ERRNO_EFAULT  14
#define ERRNO_EEXIST  17
#define ERRNO_ENOTDIR 20
#define ERRNO_EISDIR  21
#define ERRNO_EINVAL  22
#define ERRNO_EMFILE  24
#define ERRNO_ESPIPE  29
#define ERRNO_EROFS   30
#define ERRNO_ENOSYS  38

#define SYSCALL_ERROR(number) (-(number))

#endif
