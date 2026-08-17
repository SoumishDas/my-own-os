/*
 * link.ld -- Lay the freestanding kernel out at physical/virtual 1 MiB.
 *
 * A normal hosted linker script expects an operating system and dynamic
 * loader.  This one instead gives GRUB a flat loadable image, keeps executable,
 * read-only/data, and zero-filled sections in order, and page-aligns their
 * ends.  The exported code/data/bss/end names are consumed by assembly and the
 * early allocator.  This kernel currently identity-maps these addresses.
 */

ENTRY(start)
SECTIONS
{

    /* 1 MiB avoids the BIOS/legacy-device region in low physical memory. */
    .text 0x100000 :
    {
        code = .; _code = .; __code = .;
        *(.text)       /* Machine instructions from every input object. */
        . = ALIGN(4096);
    }

    .data :
    {
        data = .; _data = .; __data = .;
        *(.data)       /* Initialized writable globals. */
        *(.rodata)     /* String literals and other constants. */
        . = ALIGN(4096);
    }

    .bss :
    {
        bss = .; _bss = .; __bss = .;
        *(.bss)        /* Zero-initialized globals; occupies RAM, not file bytes. */
        . = ALIGN(4096);
    }

    end = .; _end = .; __end = .;
}
