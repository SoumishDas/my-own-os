/*
 * sigdemo.c -- End-to-end exercise for caught and default signal actions.
 *
 * PID A installs a SIGUSR1 handler, forks PID B, and B signals A. The handler
 * runs asynchronously in A and returns through sigreturn. A then forks PID C,
 * which raises default-action SIGTERM; waitpid must report 128 + SIGTERM.
 */
#include "userlib.h"

static volatile int caught_signal;
static volatile int child_notifications;

static void on_user_signal(int signal_number)
{
    caught_signal = signal_number;
    print("sigdemo: userspace handler caught SIGUSR1 in PID ");
    print_unsigned((uint32_t)getpid());
    print("\n");
}

static void on_child_exit(int signal_number)
{
    if (signal_number == SIGCHLD)
        child_notifications++;
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (signal(SIGUSR1, on_user_signal) == SIG_ERR)
    {
        print("sigdemo: could not install handler\n");
        return 1;
    }
    if (signal(SIGCHLD, on_child_exit) == SIG_ERR)
    {
        print("sigdemo: could not install SIGCHLD handler\n");
        return 1;
    }

    int parent_pid = getpid();
    int sender_pid = fork();
    if (sender_pid < 0)
        return 2;
    if (sender_pid == 0)
    {
        int result = kill(parent_pid, SIGUSR1);
        _exit(result == 0 ? 7 : 3);
    }

    int status = 0;
    while (waitpid(sender_pid, &status) == 0)
        asm volatile("pause");
    if (status != 7 || caught_signal != SIGUSR1 || child_notifications < 1)
    {
        print("sigdemo: caught-signal test failed\n");
        return 4;
    }

    int terminated_pid = fork();
    if (terminated_pid < 0)
        return 5;
    if (terminated_pid == 0)
    {
        raise(SIGTERM);
        print("sigdemo: ERROR: default SIGTERM returned\n");
        _exit(6);
    }

    status = 0;
    while (waitpid(terminated_pid, &status) == 0)
        asm volatile("pause");
    if (status != 128 + SIGTERM)
    {
        print("sigdemo: default-action test failed; status ");
        print_unsigned((uint32_t)status);
        print("\n");
        return 8;
    }

    if (child_notifications < 2)
    {
        print("sigdemo: SIGCHLD handler test failed\n");
        return 9;
    }

    print("sigdemo: SIGTERM default action produced status 143\n");
    print("sigdemo: caught SIGCHLD for both child exits\n");
    print("sigdemo: all signal tests passed\n");
    return 0;
}
