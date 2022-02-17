global _start;
global loadPageDirectory
global enablePaging
[bits 32]

_start:
    [extern kernel_main] ; Define calling point. Must have same name as kernel.c 'main' function
    call kernel_main ; Calls the C function. The linker will know where it is placed in memory
    jmp $



loadPageDirectory:
    push ebp
    mov ebp, esp
    mov eax,dword [esp]
    mov cr3,eax
    mov esp, ebp
    pop ebp
    ret

enablePaging:
    push ebp
    mov ebp, esp
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax 
    mov esp, ebp
    pop ebp
    ret

