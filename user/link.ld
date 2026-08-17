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
    .preinit_array :
    {
        PROVIDE_HIDDEN(__preinit_array_start = .);
        KEEP(*(.preinit_array*))
        PROVIDE_HIDDEN(__preinit_array_end = .);
    } :data

    .init_array :
    {
        PROVIDE_HIDDEN(__init_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*)))
        KEEP(*(.init_array*))
        PROVIDE_HIDDEN(__init_array_end = .);
    } :data

    .fini_array :
    {
        PROVIDE_HIDDEN(__fini_array_start = .);
        KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*)))
        KEEP(*(.fini_array*))
        PROVIDE_HIDDEN(__fini_array_end = .);
    } :data

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
