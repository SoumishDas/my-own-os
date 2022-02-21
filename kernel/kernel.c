#include "../cpu/isr.h"
#include "../drivers/screen.h"
#include "kernel.h"
#include "../libc/string.h"
#include "../libc/mem.h"

#include <stdint.h>

heap_t *kheap=0;

char * ptr;

void kernel_main() {
    isr_install();
    irq_install();

    
    initialise_paging();
    
    
    kprint("Type something, it will go through the kernel\n"
        "Type END to halt the CPU or PAGE to request a kmalloc()\n> ");
}

void user_input(char *input) {
    if (strcmp(input, "END") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    } else if (strcmp(input, "MEM") == 0){
        
        
        ptr = kmalloc(0x100000);
        
        
        kprint_hex(ptr);
    
    } else if (strcmp(input, "FREE") == 0){
        kfree(ptr);
    }
    kprint("You said: ");
    kprint(input);
    kprint("\n> ");
}

void ASSERT(_Bool b){
    if(b){
        return;
    }else{
        kprint("Wrong Assertion");
    }
}