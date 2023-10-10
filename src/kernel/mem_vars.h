
#ifndef MEM_VARS_H
#define MEM_VARS_H

#include "ordered_array.h"

#define KHEAP_START         0xC0000000
#define KHEAP_INITIAL_SIZE  0x100000
#define HEAP_INDEX_SIZE   0x20000
#define HEAP_MAGIC        0x123890AB
#define HEAP_MIN_SIZE     0x70000


/**
  Size information for a hole/block
**/
typedef struct
{
   uint32_t magic;   // Magic number, used for error checking and identification.
   int8_t is_hole;   // 1 if this is a hole. 0 if this is a block.
   uint32_t size;    // size of the block, including the end footer.
} header_t;

typedef struct
{
   uint32_t magic;     // Magic number, same as in header_t.
   header_t *header; // Pointer to the block header.
} footer_t;


typedef struct
{
   ordered_array_t index;
   uint32_t start_address; // The start of our allocated space.
   uint32_t end_address;   // The end of our allocated space. May be expanded up to max_address.
   uint32_t max_address;   // The maximum address the heap can be expanded to.
   int8_t supervisor;     // Should extra pages requested by us be mapped as supervisor-only?
   int8_t readonly;       // Should extra pages requested by us be mapped as read-only?
} heap_t;

typedef struct page
{
   uint32_t present    : 1;   // Page present in memory
   uint32_t rw         : 1;   // Read-only if clear, readwrite if set
   uint32_t user       : 1;   // Supervisor level only if clear
   uint32_t accessed   : 1;   // Has the page been accessed since last refresh?
   uint32_t dirty      : 1;   // Has the page been written to since last refresh?
   uint32_t unused     : 7;   // Amalgamation of unused and reserved bits
   uint32_t frame      : 20;  // Frame address (shifted right 12 bits)
} page_t;

typedef struct page_table
{
   page_t pages[1024];
} page_table_t;

typedef struct page_directory
{
   /**
      Array of pointers to pagetables.
   **/
   page_table_t *tables[1024];
   /**
      Array of pointers to the pagetables above, but gives their *physical*
      location, for loading into the CR3 register.
   **/
   uint32_t tablesPhysical[1024];
   /**
      The physical address of tablesPhysical. This comes into play
      when we get our kernel heap allocated and the directory
      may be in a different location in virtual memory.
   **/
   uint32_t physicalAddr;
} page_directory_t;

// A Heap for the Kernel
extern heap_t *kheap;

// A bitset of frames - used or free.
extern uint32_t *frames;
extern uint32_t nframes;

// Pointers to Current page directory and the kernel page directory
extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;


extern uint32_t placement_address;

#endif