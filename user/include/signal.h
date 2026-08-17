/*
 * signal.h -- Small libc-facing signal interface.
 *
 * The numeric values intentionally match common POSIX values. This is not a
 * complete POSIX sigaction API yet, but signal(), raise(), and kill() provide
 * enough surface area to exercise real asynchronous userspace handlers.
 */
#ifndef USER_SIGNAL_H
#define USER_SIGNAL_H

typedef void (*sighandler_t)(int);

#define SIGHUP   1
#define SIGINT   2
#define SIGKILL  9
#define SIGUSR1 10
#define SIGUSR2 12
#define SIGTERM 15
#define SIGCHLD 17

#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

sighandler_t signal(int signal_number, sighandler_t handler);
int kill(int process_id, int signal_number);
int raise(int signal_number);

#endif
