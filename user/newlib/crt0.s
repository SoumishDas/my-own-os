; Newlib-aware ELF process entry.
;
; Unlike the bootstrap CRT, this calls exit(main(...)) rather than _exit().
; Newlib's exit() runs atexit callbacks and flushes buffered FILE streams before
; eventually invoking our _exit syscall hook.

[BITS 32]
[GLOBAL _start]
[GLOBAL _init]
[GLOBAL _fini]
[EXTERN main]
[EXTERN exit]
[EXTERN atexit]
[EXTERN __libc_init_array]
[EXTERN __libc_fini_array]

_start:
    xor ebp, ebp
    push dword __libc_fini_array ; exit() will run global destructors last.
    call atexit
    add esp, 4
    call __libc_init_array       ; Run .preinit_array and .init_array entries.
    pop eax                    ; argc
    mov ebx, esp               ; argv
    push ebx
    push eax
    call main
    add esp, 8
    push eax
    call exit

.unreachable:
    pause
    jmp .unreachable

; GCC normally supplies these through crti.o/crtn.o. They are empty until this
; OS adopts the complete compiler CRT set; array constructors still run.
_init:
    ret
_fini:
    ret
