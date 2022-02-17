#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"

#include <stdint.h>


void kernel_main() {
    isr_install();
    irq_install();

    
    InitMemory();
    kprint_hex();
    
    
    kprint("Type something, it will go through the kernel\n"
        "Type END to halt the CPU or PAGE to request a kmalloc()\n> ");
}

void user_input(char *input) {
    if (strcmp(input, "END") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    } else if (strcmp(input, "MEM") == 0){
        GetFreeBlock(6);
    } else if (strcmp(input, "BLOCKS") == 0){
        PrintBlocks();
    }
    kprint("You said: ");
    kprint(input);
    kprint("\n> ");
}
