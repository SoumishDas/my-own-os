; signal.s -- Return path shared by all userspace signal handlers.
;
; The kernel makes this address look like the handler's caller. A normal RET
; from handler(signum) arrives here with ESP pointing at the signum field of the
; kernel-built signal frame. SYS_SIGRETURN validates that pointer and replaces
; the current int 0x80 frame with the pre-signal CPU state. On success IRET
; resumes the interrupted code, so this routine never returns conventionally.

[BITS 32]
[GLOBAL __signal_trampoline]
[EXTERN __sigreturn]
[EXTERN _exit]

__signal_trampoline:
    mov eax, esp                 ; Address of signum + magic + saved registers.
    push eax
    call __sigreturn             ; Success resumes old state and never returns.

    ; A valid kernel-created frame never fails. If corruption is detected,
    ; terminate instead of returning through an address that does not exist.
    push dword 255
    call _exit
.unreachable:
    pause
    jmp .unreachable
