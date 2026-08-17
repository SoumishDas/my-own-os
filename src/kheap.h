
/*
 * BIG PICTURE
 * -----------
 * kmalloc() hides two different allocation strategies behind one name:
 *
 *  1. During early boot there is no heap yet.  kmalloc() therefore uses a
 *     one-way "placement allocator": return placement_address, then advance
 *     placement_address.  Early allocations are permanent and cannot be freed.
 *
 *  2. Once paging.c creates kheap, kmalloc() delegates to alloc().  The real
 *     heap can split free regions, merge them again in free(), grow by mapping
 *     pages, and shrink by unmapping pages at its upper end.
 *
 * Every region in the real heap has this physical layout in virtual memory:
 *
 *   low address                                                high address
 *   +----------------+-------------------------+----------------+
 *   | header_t       | bytes returned to user  | footer_t       |
 *   +----------------+-------------------------+----------------+
 *
 * The header says how large the complete region is and whether it is free.
 * The footer points backward to the header.  That backward link allows free()
 * to find the region immediately to the left without walking the whole heap.
 * A free region is called a "hole" throughout this code.
 */

#ifndef KHEAP_H
#define KHEAP_H

#include "common.h"
#include "ordered_array.h"

#define KHEAP_START         0xC0000000 /* First virtual heap address (3 GiB). */
#define KHEAP_INITIAL_SIZE  0x100000   /* Initially map 1 MiB for the heap. */

#define HEAP_INDEX_SIZE   0x20000    /* Maximum free-hole pointers in the index. */
#define HEAP_MAGIC        0x123890AB /* Metadata corruption-detection marker. */
#define HEAP_MIN_SIZE     0x70000    /* Never contract below this many bytes. */

/**
   Metadata stored immediately before every allocated block or free hole.
**/
typedef struct
{
    u32int magic;   // Must equal HEAP_MAGIC while this metadata is valid.
    u8int is_hole;  // 1 means available; 0 means owned by an allocator caller.
    u32int size;    // Total bytes: header + caller payload + footer.
} header_t;

/* Metadata stored in the final bytes of every allocated block or free hole. */
typedef struct
{
    u32int magic;     // Must equal HEAP_MAGIC, just like the header.
    header_t *header; // Back-pointer to the header at this region's beginning.
} footer_t;

/* Complete state of one heap.  This kernel currently creates one global heap. */
typedef struct
{
    ordered_array_t index; // Size-sorted pointers to free-hole headers only.
    u32int start_address;  // First byte available for block metadata.
    u32int end_address;    // One byte past the currently mapped heap.
    u32int max_address;    // Exclusive upper bound for heap expansion.
    u8int supervisor;      // Nonzero: new pages should be supervisor-only.
    u8int readonly;        // Nonzero: new pages should be read-only.
} heap_t;

/**
   Create a heap covering [start, end), with max as its growth limit.
   start and end must both be aligned to 4 KiB page boundaries.
**/
heap_t *create_heap(u32int start, u32int end, u32int max, u8int supervisor, u8int readonly);

/**
   Allocate size caller-usable bytes. If page_align is nonzero, the returned
   payload address (not its hidden header) must lie on a page boundary.
**/
void *alloc(u32int size, u8int page_align, heap_t *heap);

/**
   Release an alloc() result and merge adjacent free holes where possible.
**/
void free(void *p, heap_t *heap);

/**
   Allocate a chunk of memory, sz in size. If align == 1,
   the chunk must be page-aligned. If phys != 0, the physical
   location of the allocated chunk will be stored into phys.

   This is the common implementation behind every kmalloc wrapper. More user-friendly
   parameter representations are available in kmalloc, kmalloc_a,
   kmalloc_ap, kmalloc_p.
**/
u32int kmalloc_int(u32int sz, int align, u32int *phys);

/**
   Allocate a chunk of memory, sz in size. The chunk must be
   page aligned.
**/
u32int kmalloc_a(u32int sz);

/**
   Allocate a chunk of memory, sz in size. The physical address
   is returned in phys. Phys MUST be a valid pointer to u32int!
**/
u32int kmalloc_p(u32int sz, u32int *phys);

/**
   Allocate a chunk of memory, sz in size. The physical address 
   is returned in phys. It must be page-aligned.
**/
u32int kmalloc_ap(u32int sz, u32int *phys);

/**
   Ordinary allocation: no required alignment and no physical-address result.
**/
u32int kmalloc(u32int sz);

/**
   Free memory returned by kmalloc after the real heap was initialized.
**/
void kfree(void *p);

#endif // KHEAP_H
