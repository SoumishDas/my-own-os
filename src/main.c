/*
 * main.c -- Kernel boot coordinator.
 *
 * GRUB enters through boot.s, which passes the Multiboot information and the
 * original stack pointer here.  This function initializes subsystems in their
 * dependency order, creates the initial userspace mother process, and then
 * leaves PID 0 as the kernel idle context.  The mother becomes PID 1, forks
 * PID 2, and PID 2 replaces its bootstrap image with /bin/shell from initrd.
 */

#include "monitor.h"
#include "descriptor_tables.h"
#include "timer.h"
#include "paging.h"
#include "multiboot.h"
#include "fs.h"
#include "initrd.h"
#include "task.h"
#include "syscall.h"
#include "keyboard.h"

extern u32int placement_address;
u32int initial_esp;

/*
 * Bootstrap entry for the one userspace process that does not come from fork.
 *
 * This function initially executes at ring 0 on PID 1's newly mapped user
 * stack.  It immediately enters ring 3 and uses the same public fork syscall
 * that the shell will later use for every external program.
 *
 * After fork, both branches use IRET to enter ring 3:
 *   - the parent is the requested do-nothing mother process (PID 1);
 *   - the child execs the standalone /bin/shell ELF image (PID 2).
 */
static void mother_process_entry(void)
{
    switch_to_user_mode();
    int shell_pid = syscall_fork();

    if (shell_pid == 0)
    {
        char *shell_argv[] = { "/bin/shell", 0 };

        /*
         * execve copies the path and argument strings out of this old image,
         * validates the ELF file, builds a fresh userspace address space and
         * finally enters its _start entry point.  A successful exec therefore
         * never returns here: PID 2 keeps its identity but becomes the shell.
         */
        if (syscall_execve(shell_argv[0], shell_argv, 0) < 0)
        {
            syscall_write("PID 2 could not load /bin/shell from initrd.\n");
            syscall_exit(127);
        }
    }

    /* A failed initial fork leaves PID 1 alive but intentionally inert. */

    /*
     * PID 1 intentionally owns no policy yet.  PAUSE is legal in ring 3 and
     * merely reduces pressure in this spin loop; timer IRQs still preempt it.
     * Once wait/exit states exist, init will block here and reap children.
     */
    for (;;)
        asm volatile("pause");
}

int main(struct multiboot *mboot_ptr, u32int initial_stack)
{
    initial_esp = initial_stack;
    // Initialise all the ISRs and segmentation
    init_descriptor_tables();
    // Initialise the screen (by clearing it)
    monitor_clear();

    // Initialise the PIT to 100Hz
    asm volatile("sti");
    init_timer(50);

    // Find the location of our initial ramdisk.
    ASSERT(mboot_ptr->mods_count > 0);
    u32int initrd_location = *((u32int*)mboot_ptr->mods_addr);
    u32int initrd_end = *(u32int*)(mboot_ptr->mods_addr+4);
    // Don't trample our module with placement accesses, please!
    placement_address = initrd_end;

    /* GRUB reports mem_upper in KiB above the first MiB. */
    ASSERT(mboot_ptr->flags & MULTIBOOT_FLAG_MEM);
    ASSERT(mboot_ptr->mem_upper <= (0xFFFFFFFF - 0x100000) / 1024);
    u32int physical_memory_end = 0x100000 + mboot_ptr->mem_upper * 1024;
    u32int contiguous_memory_kib = mboot_ptr->mem_upper + 1024;

    /* Make the bootloader-provided value visible instead of trusting it silently. */
    monitor_write("Multiboot contiguous memory: ");
    monitor_write_dec(contiguous_memory_kib);
    monitor_write(" KiB (~");
    monitor_write_dec(physical_memory_end / (1024 * 1024));
    monitor_write(" MiB)\n");

    // Start paging using the amount of RAM reported by GRUB.
    initialise_paging(physical_memory_end);

    // Start multitasking.
    initialise_tasking();

    // Initialise the initial ramdisk, and set it as the filesystem root.
    fs_root = initialise_initrd(initrd_location, initrd_end);
    /* PID 0 starts at / so PID 1 and its forked descendants inherit it. */
    task_set_working_directory(fs_root);

    initialise_syscalls();
    initialise_keyboard();

    /*
     * Add PID 1 with a fresh address space.  Its bootstrap will fork PID 2 for
     * the shell before either branch enters ring 3.
     */
    int mother_pid = create_initial_user_process(mother_process_entry);
    ASSERT(mother_pid == 1);

    monitor_write("Started user mother process with PID ");
    monitor_write_dec((u32int)mother_pid);
    monitor_write("; it will fork shell PID 2.\n");

    /*
     * HLT sleeps kernel PID 0 until the next interrupt instead of wasting CPU.
     * IRQ0 wakes it and the timer handler may schedule PID 1 or PID 2.
     */
    for (;;)
        asm volatile("hlt");
}
