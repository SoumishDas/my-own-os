/* Static ELF32 userspace layout consumed by the kernel's first exec loader. */
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* PF_R | PF_X */
    data PT_LOAD FLAGS(6); /* PF_R | PF_W */
}

SECTIONS
{
    . = 0x08048000;

    .text :
    {
        *(.text*)
        *(.rodata*)
    } :text

    . = ALIGN(4096);
    .data :
    {
        *(.data*)
    } :data

    .bss :
    {
        *(.bss*)
        *(COMMON)
    } :data

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
