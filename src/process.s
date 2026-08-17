; process.s -- Low-level helpers that cannot be expressed safely in C.
;
; read_eip supports the tutorial scheduler's unusual context-switch trick.
; copy_page_physical copies one physical page while paging is temporarily off.
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
