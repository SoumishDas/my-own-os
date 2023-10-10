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
