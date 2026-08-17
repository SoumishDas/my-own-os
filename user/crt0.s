; crt0.s -- First instruction in every standalone userspace ELF.
;
; execve constructs a conventional stack beginning with argc followed by the
; argv pointer array.  _start translates that raw process entry state into a
; normal cdecl call to main(argc, argv), then hands main's result to _exit.

[BITS 32]
[GLOBAL _start]
[EXTERN main]
[EXTERN _exit]

_start:
    xor ebp, ebp             ; Terminate frame-pointer walks at process entry.
    pop eax                  ; argc
    mov ebx, esp             ; argv points at the first pointer after argc.
    push ebx                 ; cdecl argument 2: argv
    push eax                 ; cdecl argument 1: argc
    call main
    add esp, 8
    push eax                 ; main's return value becomes _exit(status).
    call _exit

.unexpected_return:
    pause
    jmp .unexpected_return
