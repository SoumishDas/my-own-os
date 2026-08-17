/*
 * signal.h -- Kernel side of the first userspace signal ABI.
 *
 * A signal is a small asynchronous notification identified by a number.  The
 * sender only marks that number pending on the target task.  Just before the
 * kernel returns to ring 3, task_signal_deliver() consumes one pending signal
 * and either applies its default action or redirects the saved CPU frame to a
 * userspace handler.  This separation is important: kill() may run while some
 * entirely different process owns the CPU, so it must not call that process's
 * handler directly.
 */
#ifndef SIGNAL_H
#define SIGNAL_H

#include "common.h"
#include "isr.h"

struct task;

#define SIGNAL_COUNT 32

/* Familiar POSIX signal numbers keep future libc adaptation unsurprising. */
#define SIGNAL_HUP   1
#define SIGNAL_INT   2
#define SIGNAL_KILL  9
#define SIGNAL_USR1 10
#define SIGNAL_USR2 12
#define SIGNAL_TERM 15
#define SIGNAL_CHLD 17

/* Special handler values are never executable addresses. */
#define SIGNAL_DEFAULT ((u32int)0)
#define SIGNAL_IGNORE  ((u32int)1)

/* Register or query a disposition. Returns the previous value or -1. */
int task_signal_set_handler(int signal_number, u32int handler,
                            u32int userspace_trampoline);

/* Queue a signal for a process. signal_number zero only checks existence. */
int task_signal_send(int process_id, int signal_number);

/* Deliver at most one pending signal by changing a ring-3 return frame. */
void task_signal_deliver(registers_t *return_frame);

/* Restore the context saved beneath a userspace handler invocation. */
int task_signal_return(u32int context_address, registers_t *return_frame);

/* Apply fork/exec disposition rules to process signal state. */
void task_signal_inherit(struct task *child, const struct task *parent);
void task_signal_exec_reset(void);

#endif
