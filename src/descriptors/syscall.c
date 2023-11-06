#include "syscall.h"


static void syscall_handler(registers_t *regs);

DEFN_SYSCALL1(kprint, 0, const char*);
DEFN_SYSCALL1(kprint_hex, 1, const char*);
DEFN_SYSCALL1(kprint_int, 2, const char*);
DEFN_SYSCALL0(exit, 3);
static void *syscalls[4] =
{
    &kprint,
    &kprint_hex,
    &kprint_int,
    &_exit_usermode,
};


    
uint32_t num_syscalls = 4;

void initialise_syscalls()
{
    // Register our syscall handler.
    register_interrupt_handler (0x80, &syscall_handler);
}

void syscall_handler(registers_t *regs)
{
    // Firstly, check if the requested syscall number is valid.
    // The syscall number is found in EAX.
    if (regs->eax >= num_syscalls)
        return;

    // Get the required syscall location.
    void *location = syscalls[regs->eax];

    // We don't know how many parameters the function wants, so we just
    // push them all onto the stack in the correct order. The function will
    // use all the parameters it wants, and we can pop them all back off afterwards.
    int ret;
    asm volatile (" \
      push %1; \
      push %2; \
      push %3; \
      push %4; \
      push %5; \
      call *%6; \
      pop %%ebx; \
      pop %%ebx; \
      pop %%ebx; \
      pop %%ebx; \
      pop %%ebx; \
    " : "=a" (ret) : "r" (regs->edi), "r" (regs->esi), "r" (regs->edx), "r" (regs->ecx), "r" (regs->ebx), "r" (location));
    regs->eax = ret;
}
