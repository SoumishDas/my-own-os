
#include "kernel.h"




void user_input(char *input) {
    execute_command(input);
}

void ASSERT(_Bool b){
    if(b){
        return;
    }else{
        //kprint("Wrong Assertion");
    }
}



void irq_install() {
    /* Enable interruptions */
    asm volatile("sti");
    /* IRQ0: timer */
    init_timer(50);
    /* IRQ1: keyboard */
    init_keyboard();
}

void kernel_main() {
    isr_install();
    irq_install();

    
    initialise_paging();
    
    
    kprint("Type something, it will go through the kernel\n"
        "Type END to halt the CPU or PAGE to request a kmalloc()\n> ");
}