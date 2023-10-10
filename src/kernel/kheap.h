#ifndef KHEAP_H
#define KHEAP_H



#include "paging.h"

/**
  Sets up the environment, page directories etc and
  enables paging.
**/
void initialise_paging();

/**
  Create a new heap.
**/
heap_t *create_heap(uint32_t start, uint32_t end_addr, uint32_t max, uint8_t supervisor, uint8_t readonly);
/**
  Allocates a contiguous region of memory 'size' in size. If page_align==1, it creates that block starting
  on a page boundary.
**/
void *alloc(uint32_t size, uint8_t page_align, heap_t *heap);
/**
  Releases a block allocated with 'alloc'.
**/
void free(void *p, heap_t *heap);
void kfree(void *ptr);



#endif