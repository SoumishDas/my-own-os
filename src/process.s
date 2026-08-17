; process.s -- Low-level helpers that cannot be expressed safely in C.
;
; read_eip supports the tutorial scheduler's unusual context-switch trick.
; copy_page_physical copies one physical page while paging is temporarily off.
; zero_page_physical clears one newly allocated frame before userspace sees it.
; resume_user_frame starts a fork child from its private saved interrupt frame.
; Both functions use the 32-bit cdecl ABI: arguments are on the stack, EAX is
; the return register, and EBX must be preserved for the caller.

[BITS 32]

[GLOBAL read_eip]
read_eip:
    pop eax                     ; The call instruction placed its next EIP here.
    jmp eax                     ; Return. Can't use RET because return
                                ; address popped off the stack. 

[GLOBAL copy_page_physical]
copy_page_physical:
    push ebx              ; According to __cdecl, we must preserve the contents of EBX.
    pushf                 ; push EFLAGS, so we can pop it and reenable interrupts
                          ; later, if they were enabled anyway.
    cli                   ; Disable interrupts, so we aren't interrupted.
                          ; Load these in BEFORE we disable paging!
    ; Two pushes moved cdecl arguments from [esp+4]/[esp+8] to +12/+16.
    mov ebx, [esp+12]     ; Source PHYSICAL address.
    mov ecx, [esp+16]     ; Destination PHYSICAL address.
  
    mov edx, cr0          ; Get the control register...
    and edx, 0x7fffffff   ; and...
    mov cr0, edx          ; Disable address translation; addresses now mean physical.
  
    mov edx, 1024         ; 1024*4bytes = 4096 bytes
  
.loop:
    mov eax, [ebx]        ; Get the word at the source address
    mov [ecx], eax        ; Store it at the dest address
    add ebx, 4            ; Source address += sizeof(word)
    add ecx, 4            ; Dest address += sizeof(word)
    dec edx               ; One less word to do
    jnz .loop             
  
    mov edx, cr0          ; Get the control register again
    or  edx, 0x80000000   ; and...
    mov cr0, edx          ; Enable paging again (also refreshes translation state).
  
    popf                  ; Pop EFLAGS back.
    pop ebx               ; Get the original value of EBX back.
    ret

[GLOBAL zero_page_physical]
zero_page_physical:
    ; Save every cdecl callee-saved register that this routine modifies while
    ; paging is still active and the caller's virtual stack is reachable.
    push edi
    pushf
    cli

    ; Two pushes moved the physical-address argument from [esp+4] to [esp+12].
    mov edi, [esp+12]

    ; Kernel instructions are identity-mapped, so execution can continue while
    ; CR0.PG is clear.  Do not touch the virtual stack until paging is restored.
    mov eax, cr0
    and eax, 0x7fffffff
    mov cr0, eax

    xor eax, eax           ; Value written into every four-byte word.
    mov ecx, 1024          ; 1024 dwords = one 4096-byte physical frame.
    cld                    ; Make REP STOSD advance EDI toward higher addresses.
    rep stosd

    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax           ; Restoring paging also refreshes translation state.

    popf
    pop edi
    ret

[GLOBAL resume_user_frame]
resume_user_frame:
    ;
    ; The scheduler jumps here with ESP pointing at a registers_t copied onto
    ; the child's own kernel stack.  This is the same layout consumed by the
    ; tail of interrupt.s, so restoring it makes the child appear to return
    ; normally from int 0x80.  Its copied EAX is zero, as fork requires.
    ;
    pop ebx                    ; Saved user data-segment selector.
    mov ds, bx
    mov es, bx
    mov fs, bx
    mov gs, bx
    popa                       ; EDI..EAX in the order produced by PUSHA.
    add esp, 8                 ; Discard copied interrupt number and error code.
    sti
    iret                       ; Restore user EIP/CS/EFLAGS/ESP/SS.
