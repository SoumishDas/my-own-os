/*
 * task.c -- Early process creation and round-robin scheduling.
 *
 * PROCESS MODEL USED DURING THIS STAGE
 * ------------------------------------
 *   PID 0: kernel boot/idle context; it never enters user mode.
 *   PID 1: the one initial userspace "mother" process, created with a fresh
 *          page directory by create_initial_user_process().
 *   PID 2: forked from PID 1 and used for the linked-in shell.
 *
 * Later programs will also be produced by fork(), followed by exec() in the
 * child.  fork() eagerly copies private user pages today; copy-on-write is a
 * future optimization.  exec() is not implemented yet.
 */

#include "task.h"
#include "paging.h"
#include "kheap.h"
#include "descriptor_tables.h"
#include "signal.h"

// The currently running task.
volatile task_t *current_task;

// The start of the task linked list.
volatile task_t *ready_queue;

// Some externs are needed to access members in paging.c...
extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;
extern u32int initial_esp;
extern u32int read_eip();
/* Clear a frame by physical address without requiring it in the current CR3. */
extern void zero_page_physical(u32int physical_address);
extern void resume_user_frame(void);

// The next available process ID.
u32int next_pid = 1;

/* Round an address upward without changing an already aligned address. */
#define PAGE_SIZE 0x1000
#define PAGE_ALIGN_UP(value) (((value) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

static void initialise_standard_descriptors(task_t *task)
{
    memset(task->descriptors, 0, sizeof(task->descriptors));
    task->descriptors[0].kind = FD_KIND_KEYBOARD; /* stdin */
    task->descriptors[1].kind = FD_KIND_CONSOLE;  /* stdout */
    task->descriptors[2].kind = FD_KIND_CONSOLE;  /* stderr */
}

void initialise_tasking()
{
    // Rather important stuff happening, no interrupts please!
    asm volatile("cli");

    // Relocate the stack so we know where it is.
    move_stack((void*)0xE0000000, 0x2000);

    /*
     * PID 0 represents the kernel's current execution context.  It remains in
     * the ready queue as the idle task, but is deliberately not counted as the
     * first userspace process.
     */
    current_task = ready_queue = (task_t*)kmalloc(sizeof(task_t));
    memset((void*)current_task, 0, sizeof(task_t));
    current_task->id = 0;
    current_task->parent_id = -1;
    current_task->state = TASK_RUNNABLE;
    current_task->esp = current_task->ebp = 0;
    current_task->eip = 0;
    current_task->page_directory = current_directory;
    current_task->next = 0;
    current_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);
    current_task->is_user_process = 0;
    initialise_standard_descriptors((task_t*)current_task);
    current_task->working_directory = 0; /* Set after the initrd is mounted. */

    // Reenable interrupts.
    asm volatile("sti");
}

void move_stack(void *new_stack_start, u32int size)
{
  u32int i;
  // Allocate some space for the new stack.
  for( i = (u32int)new_stack_start;
       i >= ((u32int)new_stack_start-size);
       i -= 0x1000)
  {
    // General-purpose stack is in user-mode.
    alloc_frame( get_page(i, 1, current_directory), 0 /* User mode */, 1 /* Is writable */ );
  }
  
  // Flush the TLB by reading and writing the page directory address again.
  u32int pd_addr;
  asm volatile("mov %%cr3, %0" : "=r" (pd_addr));
  asm volatile("mov %0, %%cr3" : : "r" (pd_addr));

  // Old ESP and EBP, read from registers.
  u32int old_stack_pointer; asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
  u32int old_base_pointer;  asm volatile("mov %%ebp, %0" : "=r" (old_base_pointer));

  // Offset to add to old stack addresses to get a new stack address.
  u32int offset            = (u32int)new_stack_start - initial_esp;

  // New ESP and EBP.
  u32int new_stack_pointer = old_stack_pointer + offset;
  u32int new_base_pointer  = old_base_pointer  + offset;

  // Copy the stack.
  memcpy((void*)new_stack_pointer, (void*)old_stack_pointer, initial_esp-old_stack_pointer);

  // Backtrace through the original stack, copying new values into
  // the new stack.  
  for(i = (u32int)new_stack_start; i > (u32int)new_stack_start-size; i -= 4)
  {
    u32int tmp = * (u32int*)i;
    // If the value of tmp is inside the range of the old stack, assume it is a base pointer
    // and remap it. This will unfortunately remap ANY value in this range, whether they are
    // base pointers or not.
    if (( old_stack_pointer < tmp) && (tmp < initial_esp))
    {
      tmp = tmp + offset;
      u32int *tmp2 = (u32int*)i;
      *tmp2 = tmp;
    }
  }

  // Change stacks.
  asm volatile("mov %0, %%esp" : : "r" (new_stack_pointer));
  asm volatile("mov %0, %%ebp" : : "r" (new_base_pointer));
}

void switch_task()
{
    // If we haven't initialised tasking yet, just return.
    if (!current_task)
        return;
    /* Avoid saving/restoring the same context when only one task is runnable. */
    if (current_task == ready_queue && current_task->next == 0)
        return;

    // Read esp, ebp now for saving later on.
    u32int esp, ebp, eip;
    asm volatile("mov %%esp, %0" : "=r"(esp));
    asm volatile("mov %%ebp, %0" : "=r"(ebp));

    // Read the instruction pointer. We do some cunning logic here:
    // One of two things could have happened when this function exits - 
    //   (a) We called the function and it returned the EIP as requested.
    //   (b) We have just switched tasks, and because the saved EIP is essentially
    //       the instruction after read_eip(), it will seem as if read_eip has just
    //       returned.
    // In the second case we need to return immediately. To detect it we put a dummy
    // value in EAX further down at the end of this function. As C returns values in EAX,
    // it will look like the return value is this dummy value! (0x12345).
    eip = read_eip();

    // Have we just switched tasks?
    if (eip == 0x12345)
        return;

    // No, we didn't switch tasks. Let's save some register values and switch.
    current_task->eip = eip;
    current_task->esp = esp;
    current_task->ebp = ebp;
    
    /*
     * Select the next runnable entry.  Exited processes remain as zombies so a
     * parent can read their status, but the scheduler must never enter them.
     * PID 0 is permanently runnable, guaranteeing that this search terminates.
     */
    task_t *candidate = (task_t*)current_task;
    do
    {
        candidate = candidate->next;
        if (candidate == 0)
            candidate = (task_t*)ready_queue;
    } while (candidate->state != TASK_RUNNABLE);
    current_task = candidate;

    eip = current_task->eip;
    esp = current_task->esp;
    ebp = current_task->ebp;

    // Make sure the memory manager knows we've changed page directory.
    current_directory = current_task->page_directory;

    // Change our kernel stack over.
    set_kernel_stack(current_task->kernel_stack+KERNEL_STACK_SIZE);
    // Here we:
    // * Stop interrupts so we don't get interrupted.
    // * Temporarily put the new EIP location in ECX.
    // * Load the stack and base pointers from the new task struct.
    // * Change page directory to the physical address (physicalAddr) of the new directory.
    // * Put a dummy value (0x12345) in EAX so that above we can recognise that we've just
    //   switched task.
    // * Restart interrupts. The STI instruction has a delay - it doesn't take effect until after
    //   the next instruction.
    // * Jump to the location in ECX (remember we put the new EIP in there).
    asm volatile("         \
      cli;                 \
      mov %0, %%ecx;       \
      mov %1, %%esp;       \
      mov %2, %%ebp;       \
      mov %3, %%cr3;       \
      mov $0x12345, %%eax; \
      sti;                 \
      jmp *%%ecx           "
                 :
                 : "r"(eip), "r"(esp), "r"(ebp),
                   "r"(current_directory->physicalAddr)
                 : "eax", "ecx", "memory");
}

/* Append a fully initialized task while interrupts are disabled. */
static void append_ready_task(task_t *new_task)
{
    task_t *tail = (task_t*)ready_queue;
    while (tail->next != 0)
        tail = tail->next;
    tail->next = new_task;
}

int create_initial_user_process(void (*entry)(void))
{
    ASSERT(entry != 0);
    ASSERT(ready_queue != 0);

    /* The timer IRQ traverses the same ready list and must not see half a task. */
    asm volatile("cli");

    task_t *process = (task_t*)kmalloc(sizeof(task_t));
    memset(process, 0, sizeof(*process));

    /*
     * Clone the canonical kernel directory, not PID 0's current directory.
     * PID 0 has a private relocated stack mapping which must not leak into the
     * first userspace address space.  clone_directory(kernel_directory) shares
     * canonical kernel tables and gives PID 1 room for private user mappings.
     */
    process->page_directory = clone_directory(kernel_directory);

    /*
     * Map the fixed initial user stack into PID 1's private directory.  These
     * pages are empty at first; the bootstrap entry starts with ESP at the top
     * and ordinary C calls grow the stack toward lower addresses.
     */
    u32int stack_bottom = USER_STACK_TOP - TASK_STACK_SIZE;
    for (u32int address = stack_bottom;
         address < USER_STACK_TOP;
         address += PAGE_SIZE)
    {
        page_t *page = get_page(address, 1, process->page_directory);
        alloc_frame(page, 0, 1);
        zero_page_physical(page->frame * PAGE_SIZE);
    }

    process->id = next_pid++;
    process->parent_id = 0;
    process->state = TASK_RUNNABLE;
    process->esp = USER_STACK_TOP;
    process->ebp = USER_STACK_TOP;
    process->eip = (u32int)entry;
    process->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);
    process->is_user_process = 1;

    /*
     * There is no ELF image yet, so use a temporary fixed heap base.  exec()
     * will later replace this with ALIGN_UP(highest_loaded_segment_end).
     */
    process->heap_start = USER_HEAP_START;
    process->heap_break = USER_HEAP_START;
    process->heap_mapped_end = USER_HEAP_START;
    process->heap_limit = USER_HEAP_LIMIT;

    memcpy(process->descriptors, ((task_t*)current_task)->descriptors,
           sizeof(process->descriptors));
    process->working_directory = current_task->working_directory;
    process->next = 0;
    append_ready_task(process);

    asm volatile("sti");
    return process->id;
}

u32int task_sbrk(s32int increment)
{
    task_t *process = (task_t*)current_task;
    if (process == 0 || !process->is_user_process)
        return (u32int)-1;

    u32int old_break = process->heap_break;
    u32int new_break;

    /*
     * Compute signed movement without relying on signed overflow.  The two
     * branches explicitly reject wrapping around address zero or 4 GiB.
     */
    if (increment >= 0)
    {
        u32int growth = (u32int)increment;
        if (growth > 0xFFFFFFFFU - old_break)
            return (u32int)-1;
        new_break = old_break + growth;
    }
    else
    {
        /* -(INT_MIN) overflows signed int, so form the magnitude unsigned. */
        u32int shrink = 0U - (u32int)increment;
        if (shrink > old_break)
            return (u32int)-1;
        new_break = old_break - shrink;
    }

    if (new_break < process->heap_start || new_break > process->heap_limit)
        return (u32int)-1;

    /*
     * heap_break is byte-granular, whereas x86 mappings are page-granular.
     * Only map frames that become necessary after crossing a page boundary.
     */
    u32int required_mapped_end = PAGE_ALIGN_UP(new_break);
    for (u32int address = process->heap_mapped_end;
         address < required_mapped_end;
         address += PAGE_SIZE)
    {
        page_t *page = get_page(address, 1, process->page_directory);
        ASSERT(page != 0);
        alloc_frame(page, 0, 1); /* User-accessible and writable. */
        zero_page_physical(page->frame * PAGE_SIZE);
        invalidate_page(address);
    }

    if (required_mapped_end > process->heap_mapped_end)
        process->heap_mapped_end = required_mapped_end;
    process->heap_break = new_break;
    return old_break;
}

file_descriptor_t *task_get_descriptor(int fd)
{
    if (current_task == 0 || fd < 0 || fd >= MAX_FILE_DESCRIPTORS)
        return 0;
    file_descriptor_t *descriptor = &((task_t*)current_task)->descriptors[fd];
    return descriptor->kind == FD_KIND_FREE ? 0 : descriptor;
}

int task_open_descriptor(fs_node_t *node, u32int flags)
{
    if (current_task == 0 || node == 0)
        return -1;
    for (int fd = 3; fd < MAX_FILE_DESCRIPTORS; fd++)
    {
        file_descriptor_t *descriptor = &((task_t*)current_task)->descriptors[fd];
        if (descriptor->kind != FD_KIND_FREE)
            continue;
        descriptor->kind = FD_KIND_NODE;
        descriptor->node = node;
        descriptor->offset = 0;
        descriptor->flags = flags;
        open_fs(node, 1, 0);
        return fd;
    }
    return -1; /* This task already has all sixteen slots occupied. */
}

int task_close_descriptor(int fd)
{
    /* Standard streams remain installed for the lifetime of these early tasks. */
    if (fd < 3 || fd >= MAX_FILE_DESCRIPTORS || current_task == 0)
        return -1;
    file_descriptor_t *descriptor = &((task_t*)current_task)->descriptors[fd];
    if (descriptor->kind == FD_KIND_FREE)
        return -1;
    if (descriptor->kind == FD_KIND_NODE)
        close_fs(descriptor->node);
    memset(descriptor, 0, sizeof(*descriptor));
    return 0;
}

int task_seek_descriptor(int fd, s32int offset, int origin)
{
    file_descriptor_t *descriptor = task_get_descriptor(fd);
    if (descriptor == 0 || descriptor->kind != FD_KIND_NODE ||
        descriptor->node == 0)
        return -1;

    u32int base;
    if (origin == 0)      base = 0;                        /* SEEK_SET */
    else if (origin == 1) base = descriptor->offset;       /* SEEK_CUR */
    else if (origin == 2) base = descriptor->node->length; /* SEEK_END */
    else return -1;

    u32int result;
    if (offset >= 0)
    {
        if (base > 0x7FFFFFFFU || (u32int)offset > 0x7FFFFFFFU - base)
            return -1;
        result = base + (u32int)offset;
    }
    else
    {
        u32int magnitude = 0U - (u32int)offset;
        if (magnitude > base)
            return -1;
        result = base - magnitude;
    }
    descriptor->offset = result;
    return (int)result;
}

void task_wait_for_keyboard(void)
{
    task_t *task = (task_t*)current_task;
    ASSERT(task != 0 && task->is_user_process && task->state == TASK_RUNNABLE);
    task->state = TASK_WAITING_INPUT;
    switch_task();
    ASSERT(task->state == TASK_RUNNABLE);
}

void task_wake_keyboard_waiters(void)
{
    for (task_t *task = (task_t*)ready_queue; task != 0; task = task->next)
        if (task->state == TASK_WAITING_INPUT)
            task->state = TASK_RUNNABLE;
}

int task_has_pending_signal(void)
{
    task_t *task = (task_t*)current_task;
    return task != 0 && task->pending_signals != 0;
}

fs_node_t *task_get_working_directory(void)
{
    return current_task == 0 ? 0 : current_task->working_directory;
}

void task_set_working_directory(fs_node_t *directory)
{
    ASSERT(current_task != 0);
    ASSERT(directory != 0 && (directory->flags & 0x7) == FS_DIRECTORY);
    current_task->working_directory = directory;
}


int task_fork_from_frame(registers_t *parent_frame)
{
    ASSERT(parent_frame != 0);
    task_t *parent_task = (task_t*)current_task;
    if (parent_task == 0 || !parent_task->is_user_process ||
        parent_task->state != TASK_RUNNABLE)
        return -1;

    /* Eagerly duplicate private user pages; kernel tables remain shared. */
    page_directory_t *directory = clone_directory(current_directory);

    task_t *new_task = (task_t*)kmalloc(sizeof(task_t));
    memset(new_task, 0, sizeof(*new_task));
    new_task->id = next_pid++;
    new_task->parent_id = parent_task->id;
    new_task->state = TASK_RUNNABLE;
    new_task->page_directory = directory;
    new_task->kernel_stack = kmalloc_a(KERNEL_STACK_SIZE);
    new_task->heap_start = parent_task->heap_start;
    new_task->heap_break = parent_task->heap_break;
    new_task->heap_mapped_end = parent_task->heap_mapped_end;
    new_task->heap_limit = parent_task->heap_limit;
    new_task->is_user_process = parent_task->is_user_process;
    task_signal_inherit(new_task, parent_task);
    memcpy(new_task->descriptors, parent_task->descriptors,
           sizeof(new_task->descriptors));
    new_task->working_directory = parent_task->working_directory;
    new_task->next = 0;

    /* File descriptions are inherited.  Notify node drivers of extra users. */
    for (int fd = 0; fd < MAX_FILE_DESCRIPTORS; fd++)
    {
        if (new_task->descriptors[fd].kind == FD_KIND_NODE &&
            new_task->descriptors[fd].node != 0)
            open_fs(new_task->descriptors[fd].node, 1, 0);
    }

    /*
     * Give the child its own kernel-resident copy of the syscall return frame.
     * The scheduler loads this ESP and jumps to resume_user_frame(), whose IRET
     * returns to the cloned ring-3 EIP/ESP.  Only EAX differs: child fork is 0.
     */
    u32int frame_address = new_task->kernel_stack + KERNEL_STACK_SIZE -
                           sizeof(registers_t);
    registers_t *child_frame = (registers_t*)frame_address;
    memcpy(child_frame, parent_frame, sizeof(*child_frame));
    child_frame->eax = 0;
    new_task->esp = frame_address;
    new_task->ebp = frame_address;
    new_task->eip = (u32int)resume_user_frame;

    append_ready_task(new_task);
    return new_task->id;
}

void task_exit(int status)
{
    task_t *process = (task_t*)current_task;
    ASSERT(process != 0 && process->id != 0 && process->is_user_process);

    /* Release descriptor-level references now; memory waits for parent reaping. */
    for (int fd = 0; fd < MAX_FILE_DESCRIPTORS; fd++)
    {
        file_descriptor_t *descriptor = &process->descriptors[fd];
        if (descriptor->kind == FD_KIND_NODE && descriptor->node != 0)
            close_fs(descriptor->node);
        memset(descriptor, 0, sizeof(*descriptor));
    }

    process->exit_status = status;
    process->state = TASK_ZOMBIE;

    /* Inform a living parent. SIGCHLD defaults to ignored but may be caught. */
    if (process->parent_id > 0)
        task_signal_send(process->parent_id, SIGNAL_CHLD);

    /* switch_task() skips this zombie and execution can never return here. */
    switch_task();
    PANIC("zombie process resumed after exit");
    for (;;)
        ;
}

int task_waitpid(int pid, int *status)
{
    task_t *parent = (task_t*)current_task;
    if (parent == 0 || pid <= 0)
        return -1;

    task_t *previous = 0;
    task_t *child = (task_t*)ready_queue;
    while (child != 0)
    {
        if (child->id == pid)
            break;
        previous = child;
        child = child->next;
    }
    if (child == 0 || child->parent_id != parent->id)
        return -1;
    if (child->state != TASK_ZOMBIE)
        return 0;

    if (status != 0)
        *status = child->exit_status;

    ASSERT(previous != 0); /* PID 0, the list head, cannot be waited upon. */
    previous->next = child->next;
    destroy_directory(child->page_directory);
    kfree((void*)child->kernel_stack);
    kfree(child);
    return pid;
}

int getpid()
{
    return current_task->id;
}

void switch_to_user_mode()
{
    // Set up our kernel stack.
    set_kernel_stack(current_task->kernel_stack+KERNEL_STACK_SIZE);
    
    // Set up a stack structure for switching to user mode.
    asm volatile("  \
      cli; \
      mov $0x23, %ax; \
      mov %ax, %ds; \
      mov %ax, %es; \
      mov %ax, %fs; \
      mov %ax, %gs; \
                    \
       \
      mov %esp, %eax; \
      pushl $0x23; \
      pushl %esp; \
      pushf; \
      pop %eax; \
      or $0x200, %eax; \
      push %eax; \
      pushl $0x1B; \
      push $1f; \
      iret; \
    1: \
      "); 
      
}
