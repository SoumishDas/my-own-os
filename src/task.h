// 
// task.h - Defines the structures and prototypes needed to multitask.
//          Written for JamesM's kernel development tutorials.
//

#ifndef TASK_H
#define TASK_H

#include "common.h"
#include "paging.h"
#include "fs.h"

#define KERNEL_STACK_SIZE 8192       // Ring-0 stack used by interrupts/syscalls.
#define TASK_STACK_SIZE   8192       // Initial user stack mapped for a new process.
#define MAX_FILE_DESCRIPTORS 16

/*
 * TEMPORARY USER VIRTUAL-MEMORY LAYOUT
 * ------------------------------------
 * The linked-in mother and shell are an intermediate step before ELF exec.
 * Their instructions still live in the low identity-mapped kernel image, but
 * their writable process memory lives in private page tables:
 *
 *   0x40000000  first possible byte of the userspace heap
 *       ...     heap grows upward when sbrk() advances heap_break
 *   0xBFF00000  exclusive heap limit and guard gap
 *       ...     deliberately unused space
 *   0xBFFFE000  bottom of the initial 8 KiB user stack
 *   0xC0000000  top of user address space / start of kernel heap
 *
 * Once exec loads ELF files, heap_start will be computed from the end of the
 * loaded program rather than always using USER_HEAP_START.
 */
#define USER_HEAP_START  0x40000000
#define USER_HEAP_LIMIT  0xBFF00000
#define USER_STACK_TOP   0xC0000000

/* Descriptor kinds let standard streams exist without fake filesystem nodes. */
typedef enum file_descriptor_kind
{
    FD_KIND_FREE = 0,
    FD_KIND_KEYBOARD,
    FD_KIND_CONSOLE,
    FD_KIND_NODE
} file_descriptor_kind_t;

typedef struct file_descriptor
{
    file_descriptor_kind_t kind;
    fs_node_t *node;  /* Only FD_KIND_NODE uses this pointer. */
    u32int offset;    /* Next byte read or written for seekable nodes. */
    u32int flags;     /* Reserved for O_RDONLY/O_WRONLY/O_RDWR expansion. */
} file_descriptor_t;

typedef enum task_state
{
    TASK_RUNNABLE = 0,
    TASK_WAITING_INPUT,
    TASK_ZOMBIE
} task_state_t;

// This structure defines a 'task' - a process.
typedef struct task
{
    int id;                // Process ID.
    int parent_id;         // PID allowed to collect this process with waitpid.
    task_state_t state;    // Zombies own resources but are never scheduled.
    int exit_status;       // Preserved after exit until the parent collects it.
    u32int esp, ebp;       // Stack and base pointers.
    u32int eip;            // Instruction pointer.
    page_directory_t *page_directory; // Page directory.
    u32int kernel_stack;   // Kernel stack location.

    /*
     * A user heap has no kernel-side hole headers or allocation records.
     * libc's malloc/free will eventually manage individual blocks.  The
     * kernel only remembers the legal range, the exact byte-level break, and
     * how far physical pages have already been mapped.
     */
    u32int heap_start;      // Lowest break value; fixed for this program image.
    u32int heap_break;      // First byte beyond storage currently granted by sbrk.
    u32int heap_mapped_end; // Page-aligned end of already mapped heap pages.
    u32int heap_limit;      // Exclusive ceiling preventing stack/heap collision.
    u8int is_user_process;  // Zero for PID 0; nonzero for ring-3 processes.

    /*
     * Traditional (non-realtime) signals are represented by one pending bit
     * and one handler address per signal number. Multiple identical signals
     * coalesce. signal_active prevents nested handler frames until sigreturn.
     */
    u32int signal_handlers[32];
    u32int pending_signals;
    u32int signal_trampoline;
    u8int signal_active;
    u8int active_signal;

    file_descriptor_t descriptors[MAX_FILE_DESCRIPTORS];
    fs_node_t *working_directory; /* Base node for relative path resolution. */
    struct task *next;     // The next task in a linked list.
} task_t;

// Initialises the tasking system.
void initialise_tasking();

// Called by the timer hook, this changes the running process.
void switch_task();

/*
 * Create PID 1 with a fresh address space and private user stack.  This is the
 * one exceptional process that does not originate from fork(); every later
 * userspace process begins as a clone of an existing userspace process.
 */
int create_initial_user_process(void (*entry)(void));

/*
 * Move the current process break by increment bytes and return the old break.
 * A return value of (u32int)-1 means the request was outside the heap range.
 * Positive growth maps user-writable pages only when a page boundary is
 * crossed.  Shrinking changes the logical break but retains mapped pages for
 * now, allowing later growth to reuse them cheaply.
 */
u32int task_sbrk(s32int increment);

/* Descriptor helpers used by syscall.c on behalf of the current task. */
file_descriptor_t *task_get_descriptor(int fd);
int task_open_descriptor(fs_node_t *node, u32int flags);
int task_close_descriptor(int fd);
int task_seek_descriptor(int fd, s32int offset, int origin);
fs_node_t *task_get_working_directory(void);
void task_set_working_directory(fs_node_t *directory);

/* Put the caller to sleep for stdin and wake sleepers after keyboard IRQ1. */
void task_wait_for_keyboard(void);
void task_wake_keyboard_waiters(void);
int task_has_pending_signal(void);

/* Clone a ring-3 process from the syscall return frame. */
int task_fork_from_frame(registers_t *parent_frame);

/* Mark the current process dead and switch away; this function never returns. */
void task_exit(int status) __attribute__((noreturn));

/*
 * Return pid and collect a zombie child, 0 while it is still running, or -1
 * when pid is not a child of the caller.  status may be null.
 */
int task_waitpid(int pid, int *status);

// Causes the current process' stack to be forcibly moved to a new location.
void move_stack(void *new_stack_start, u32int size);

// Returns the pid of the current process.
int getpid();

// Drops from ring 0 into ring 3 using the current task's kernel stack.
void switch_to_user_mode();

#endif
