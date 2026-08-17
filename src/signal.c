/*
 * signal.c -- Minimal asynchronous, process-directed signal delivery.
 *
 * DELIVERY WALK-THROUGH
 * ---------------------
 *  1. kill(pid, n) sets bit n in the target task's pending_signals word.
 *  2. A syscall or timer interrupt eventually enters the kernel for that task.
 *  3. Before IRET, task_signal_deliver() saves the complete interrupted CPU
 *     state on the user's stack and changes EIP to the registered handler.
 *  4. The handler executes in ring 3 as handler(n). Its artificial return
 *     address is a tiny userspace trampoline supplied by libc.
 *  5. The trampoline invokes sigreturn with the saved-context address.
 *  6. sigreturn copies the trusted saved frame back to the kernel interrupt
 *     frame. IRET then resumes precisely where the signal interrupted execution.
 *
 * One handler may be active at a time. Additional signals remain pending until
 * sigreturn; this avoids nested frames while the ABI is still deliberately
 * small. Pending signals are coalesced, as traditional non-realtime signals are.
 */
#include "signal.h"
#include "task.h"

#define SIGNAL_FRAME_MAGIC 0x53494746U /* ASCII "SIGF" */
#define USER_ADDRESS_MIN   0x00400000U

typedef struct userspace_signal_frame
{
    /* RET in the handler pops this and enters libc's trampoline. */
    u32int return_address;

    /* The handler sees this at [ESP+4], exactly like a cdecl int argument. */
    u32int signal_number;

    /* sigreturn uses this cookie to reject accidental/bogus stack pointers. */
    u32int magic;

    /* Complete state that the interrupt stubs would otherwise restore. */
    registers_t saved_registers;
} userspace_signal_frame_t;

extern volatile task_t *current_task;
extern volatile task_t *ready_queue;

static int signal_number_is_valid(int signal_number)
{
    return signal_number > 0 && signal_number < SIGNAL_COUNT;
}

static task_t *find_task(int process_id)
{
    for (task_t *task = (task_t*)ready_queue; task != 0; task = task->next)
        if (task->id == process_id)
            return task;
    return 0;
}

static int signal_default_is_ignore(int signal_number)
{
    /* Child exit is observable through waitpid, so SIGCHLD need not terminate. */
    return signal_number == SIGNAL_CHLD;
}

int task_signal_set_handler(int signal_number, u32int handler,
                            u32int userspace_trampoline)
{
    task_t *task = (task_t*)current_task;
    if (task == 0 || !task->is_user_process ||
        !signal_number_is_valid(signal_number) || signal_number == SIGNAL_KILL)
        return -1;

    if (handler != SIGNAL_DEFAULT && handler != SIGNAL_IGNORE)
    {
        /* Both addresses will become ring-3 instruction pointers. */
        if (handler < USER_ADDRESS_MIN || handler >= USER_STACK_TOP ||
            userspace_trampoline < USER_ADDRESS_MIN ||
            userspace_trampoline >= USER_STACK_TOP)
            return -1;

        page_t *handler_page = get_page(handler, 0, task->page_directory);
        page_t *trampoline_page =
            get_page(userspace_trampoline, 0, task->page_directory);
        if (handler_page == 0 || !handler_page->present || !handler_page->user ||
            trampoline_page == 0 || !trampoline_page->present ||
            !trampoline_page->user)
            return -1;
        task->signal_trampoline = userspace_trampoline;
    }

    u32int previous = task->signal_handlers[signal_number];
    task->signal_handlers[signal_number] = handler;
    return (int)previous;
}

int task_signal_send(int process_id, int signal_number)
{
    task_t *target = find_task(process_id);
    if (target == 0 || target->id == 0 || !target->is_user_process ||
        target->state == TASK_ZOMBIE)
        return -1;

    /* POSIX kill(pid, 0) checks that the process exists without sending. */
    if (signal_number == 0)
        return 0;
    if (!signal_number_is_valid(signal_number))
        return -1;

    /* Ignored signals do not needlessly occupy a pending bit. */
    u32int handler = target->signal_handlers[signal_number];
    if (handler != SIGNAL_IGNORE &&
        !(handler == SIGNAL_DEFAULT && signal_default_is_ignore(signal_number)))
        target->pending_signals |= (1U << signal_number);

    /* A signal interrupts a blocking stdin read so delivery is not postponed. */
    if (target->state == TASK_WAITING_INPUT)
        target->state = TASK_RUNNABLE;
    return 0;
}

static int take_next_pending_signal(task_t *task)
{
    for (int signal_number = 1; signal_number < SIGNAL_COUNT; signal_number++)
    {
        u32int bit = 1U << signal_number;
        if ((task->pending_signals & bit) == 0)
            continue;
        task->pending_signals &= ~bit;
        return signal_number;
    }
    return 0;
}

void task_signal_deliver(registers_t *return_frame)
{
    task_t *task = (task_t*)current_task;
    if (task == 0 || return_frame == 0 || !task->is_user_process ||
        task->state != TASK_RUNNABLE || task->signal_active ||
        (return_frame->cs & 3) != 3)
        return;

    int signal_number = take_next_pending_signal(task);
    if (signal_number == 0)
        return;

    u32int handler = task->signal_handlers[signal_number];
    if (handler == SIGNAL_IGNORE ||
        (handler == SIGNAL_DEFAULT && signal_default_is_ignore(signal_number)))
        return;

    if (handler == SIGNAL_DEFAULT || signal_number == SIGNAL_KILL)
    {
        /* Shells conventionally report signal death as 128 + signal number. */
        task_exit(128 + signal_number);
    }

    /*
     * The first implementation deliberately confines signal frames to the
     * fixed two-page process stack. If it lacks room, continuing would corrupt
     * memory, so terminate as a failed delivery instead.
     */
    u32int stack_bottom = USER_STACK_TOP - TASK_STACK_SIZE;
    if (return_frame->useresp < stack_bottom + sizeof(userspace_signal_frame_t) ||
        return_frame->useresp > USER_STACK_TOP || task->signal_trampoline == 0)
        task_exit(128 + signal_number);

    u32int frame_address =
        (return_frame->useresp - sizeof(userspace_signal_frame_t)) & ~0xFU;
    if (frame_address < stack_bottom)
        task_exit(128 + signal_number);
    userspace_signal_frame_t *signal_frame =
        (userspace_signal_frame_t*)frame_address;
    signal_frame->return_address = task->signal_trampoline;
    signal_frame->signal_number = (u32int)signal_number;
    signal_frame->magic = SIGNAL_FRAME_MAGIC;
    memcpy(&signal_frame->saved_registers, return_frame, sizeof(*return_frame));

    task->signal_active = 1;
    task->active_signal = (u8int)signal_number;
    return_frame->eip = handler;
    return_frame->useresp = frame_address;
}

int task_signal_return(u32int context_address, registers_t *return_frame)
{
    task_t *task = (task_t*)current_task;
    u32int stack_bottom = USER_STACK_TOP - TASK_STACK_SIZE;

    /* context_address points at signal_number, immediately after return_address. */
    if (task == 0 || return_frame == 0 || !task->signal_active ||
        context_address < stack_bottom + sizeof(u32int) ||
        context_address > USER_STACK_TOP -
            (sizeof(userspace_signal_frame_t) - sizeof(u32int)))
        return -1;

    userspace_signal_frame_t *signal_frame =
        (userspace_signal_frame_t*)(context_address - sizeof(u32int));
    if (signal_frame->magic != SIGNAL_FRAME_MAGIC ||
        signal_frame->signal_number != task->active_signal)
        return -1;

    /* The saved frame was written by the kernel, not accepted from kill(). */
    memcpy(return_frame, &signal_frame->saved_registers, sizeof(*return_frame));
    task->signal_active = 0;
    task->active_signal = 0;
    return 0;
}

void task_signal_inherit(struct task *child_untyped,
                         const struct task *parent_untyped)
{
    task_t *child = (task_t*)child_untyped;
    const task_t *parent = (const task_t*)parent_untyped;
    memcpy(child->signal_handlers, parent->signal_handlers,
           sizeof(child->signal_handlers));
    child->signal_trampoline = parent->signal_trampoline;
    /* A new child inherits dispositions, never its parent's pending delivery. */
    child->pending_signals = 0;
    child->signal_active = 0;
    child->active_signal = 0;
}

void task_signal_exec_reset(void)
{
    task_t *task = (task_t*)current_task;
    ASSERT(task != 0);
    for (int signal_number = 1; signal_number < SIGNAL_COUNT; signal_number++)
    {
        /* POSIX keeps explicitly ignored dispositions across exec. */
        if (task->signal_handlers[signal_number] != SIGNAL_IGNORE)
            task->signal_handlers[signal_number] = SIGNAL_DEFAULT;
    }
    task->signal_trampoline = 0;
    /* Pending notifications, especially uncatchable SIGKILL, survive exec. */
    task->signal_active = 0;
    task->active_signal = 0;
}
