
/*
 * x86 32-BIT PAGING STRUCTURE USED HERE
 * -------------------------------------
 * A 32-bit virtual address is divided into three useful pieces:
 *
 *    bits 31..22        bits 21..12          bits 11..0
 *   +-------------+--------------------+------------------+
 *   | directory # | page-table entry # | offset in 4K page|
 *   +-------------+--------------------+------------------+
 *       10 bits            10 bits             12 bits
 *
 * Each page directory has 1024 entries.  Each selected page table has 1024
 * page_t entries.  A page_t then selects a 4 KiB physical frame.  Therefore
 * one table describes 4 MiB and one complete directory can describe 4 GiB.
 *
 * The C bit-field layout below is intended to match an x86 page-table entry.
 * This kernel and compiler target 32-bit little-endian x86, where each page_t
 * occupies exactly four bytes.
 */

#ifndef PAGING_H
#define PAGING_H

#include "common.h"
#include "isr.h"

typedef struct page
{
    u32int present    : 1;   // 0: access faults; 1: frame translation is valid.
    u32int rw         : 1;   // 0: read-only; 1: writes are permitted.
    u32int user       : 1;   // 0: supervisor only; 1: user mode may access it.
    u32int accessed   : 1;   // CPU sets this after any access through the entry.
    u32int dirty      : 1;   // CPU sets this after writing through the entry.
    u32int unused     : 7;   // Reserved/unused flags not modeled by this kernel.
    u32int frame      : 20;  // Physical frame number; physical base is frame<<12.
} page_t;

/* One page table is 1024 entries * 4 bytes = exactly one 4 KiB page. */
typedef struct page_table
{
    page_t pages[1024];
} page_table_t;

typedef struct page_directory
{
    /**
       Virtual pointers used by kernel C code to access page-table objects.
    **/
    page_table_t *tables[1024];
    /**
       Hardware-facing directory entries.  Each value contains a page table's
       physical address plus low-bit flags such as present/RW/user.
    **/
    u32int tablesPhysical[1024];

    /**
       Physical address of tablesPhysical[0], which is what CR3 requires.
       Keeping this separately matters once the directory object is allocated
       at a virtual address different from its physical address.
    **/
    u32int physicalAddr;
} page_directory_t;

/**
   Create the frame bitmap and kernel directory, establish initial mappings,
   register the page-fault handler, enable paging, and create the kernel heap.
**/
void initialise_paging(u32int physical_memory_end);

/**
   Load the directory's physical address into CR3 and set CR0.PG.
**/
void switch_page_directory(page_directory_t *new);

void alloc_frame(page_t *page, int is_kernel, int is_writeable);
/* Release the physical frame named by page and mark the page not present. */
void free_frame(page_t *page);
/* Remove one virtual page translation from the processor's TLB cache. */
void invalidate_page(u32int address);
/* Total contiguous RAM tracked by the frame bitmap, expressed in KiB. */
u32int paging_total_memory_kib(void);

/**
   Return the page-table entry describing address. If make is nonzero and the
   required page table does not exist, allocate and install that table first.
**/
page_t *get_page(u32int address, int make, page_directory_t *dir);

/**
   Decode a CPU page-fault error and stop the kernel with diagnostics.
**/
void page_fault(registers_t *regs);

/**
   Copy a page directory for a new task. Kernel tables remain shared, while
   task-specific tables receive new frames whose contents are copied.
**/
page_directory_t *clone_directory(page_directory_t *src);

/**
   Destroy a non-canonical directory after it is no longer loaded in CR3.
   Tables shared with kernel_directory are retained; every private mapped frame,
   private page table, and the directory object itself are returned to their
   allocators.  This is the primitive exec() needs to discard the old image.
**/
void destroy_directory(page_directory_t *directory);

#endif
