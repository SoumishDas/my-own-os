#include "mem_vars.h"

heap_t *kheap;

// A bitset of frames - used or free.
uint32_t *frames;
uint32_t nframes;

// Pointers to Current page directory and the kernel page directory
page_directory_t *kernel_directory;
page_directory_t *current_directory;


uint32_t placement_address = 0x10000;