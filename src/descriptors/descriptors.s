global gdt_flush

gdt_flush:
    ; Get the pointer to the GDT, passed as a parameter
    mov eax, [esp + 4]

    ; Load the new GDT pointer
    lgdt [eax]

    ; Far jump to the code segment
    jmp 0x08:.flush   ; 0x08 is the offset to our code segment
.flush:
    ; Load data segment selectors
    mov ax, 0x10      ; 0x10 is the offset in the GDT to our data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    ret

global tss_flush

tss_flush:
    mov eax,0x0 ; 
    mov ax, 0x2B      ; Load the index of our TSS structure - The index is
                      ; 0x28, as it is the 5th selector and each is 8 bytes
                      ; long, but we set the bottom two bits (making 0x2B)
                      ; so that it has an RPL of 3, not zero.
    ltr ax            ; Load 0x2B into the task state register.
    ret
