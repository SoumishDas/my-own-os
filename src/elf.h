/*
 * elf.h -- Minimal ELF32 definitions used by the first userspace loader.
 *
 * ELF contains both section headers and program headers.  Section headers are
 * useful to linkers and debuggers; an operating-system loader only needs the
 * program headers, which describe the byte ranges that must appear in memory.
 * This kernel initially accepts static little-endian i386 ET_EXEC files only.
 */
#ifndef ELF_H
#define ELF_H

#include "common.h"

#define ELF_IDENT_SIZE 16

#define ELF_CLASS_32       1
#define ELF_DATA_LSB       1
#define ELF_VERSION_CURRENT 1
#define ELF_TYPE_EXECUTABLE 2
#define ELF_MACHINE_386     3

#define ELF_PROGRAM_LOAD 1

#define ELF_FLAG_EXECUTE 0x1
#define ELF_FLAG_WRITE   0x2
#define ELF_FLAG_READ    0x4

typedef struct __attribute__((packed)) elf32_header
{
    u8int identification[ELF_IDENT_SIZE];
    u16int type;
    u16int machine;
    u32int version;
    u32int entry;
    u32int program_header_offset;
    u32int section_header_offset;
    u32int flags;
    u16int header_size;
    u16int program_header_entry_size;
    u16int program_header_count;
    u16int section_header_entry_size;
    u16int section_header_count;
    u16int section_name_index;
} elf32_header_t;

typedef struct __attribute__((packed)) elf32_program_header
{
    u32int type;
    u32int file_offset;
    u32int virtual_address;
    u32int physical_address;
    u32int file_size;
    u32int memory_size;
    u32int flags;
    u32int alignment;
} elf32_program_header_t;

#endif
