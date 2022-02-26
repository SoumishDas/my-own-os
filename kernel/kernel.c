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
    execute_command(input);
}

void ASSERT(_Bool b){
    if(b){
        return;
    }else{
        kprint("Wrong Assertion");
    }
}