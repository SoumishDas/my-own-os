#include "terminal.h"
 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This needs to be compiled with a ix86-elf compiler"
#endif


void init_descriptors(){
    init_GDT();
    // Access a memory location in the data segment
    isr_install();
    //asm volatile("int $0x01"); // Trigger software interrupt 0x01
    asm volatile("sti");
    init_keyboard();
    init_timer(1000);
}


int kernel_main(struct multiboot *mboot_ptr)
{
    
    
    clear_screen();
    init_descriptors();
    initialise_paging();
    
    unsigned long cr0;

    // Read the value of the CR0 control register
    asm volatile("mov %%cr0, %0" : "=r"(cr0));

    // Check the PG (Paging) bit (bit 31) in CR0
    if (cr0 & (1UL << 31)) {
        kprint("Paging is enabled.\n");
    } else {
        kprint("Paging is disabled.\n");
    }
    
    
    while (1) {
        // Handle interrupts, events, and scheduling here
        // Implement your kernel's functionality
    }
    return 0;
}