// main.c -- Defines the C-code kernel entry point, calls initialisation routines.
//           Made for JamesM's tutorials <www.jamesmolloy.co.uk>

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
#include "shell.h"

extern u32int placement_address;
u32int initial_esp;

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

    initialise_syscalls();
    initialise_keyboard();

    switch_to_user_mode();

    shell_run();
    return 0; /* shell_run() is intentionally non-returning. */
}
