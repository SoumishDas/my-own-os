#ifndef PAGING_H
#define PAGING_H

#include "mem_vars.h"

// Macros used in the bitset algorithms.
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))



/**
  Causes the specified page directory to be loaded into the
  CR3 register.
**/
void switch_page_directory(page_directory_t *new);

/**
  Retrieves a pointer to the page required.
  If make == 1, if the page-table in which this page should
  reside isn't created, create it!
**/
page_t *get_page(uint32_t address, int make, page_directory_t *dir);

/**
  Handler for page faults.
**/
void page_fault(registers_t regs);

void alloc_frame(page_t *page, int is_kernel, int is_writeable);
void alloc_particular_frame(page_t *page, int is_kernel, int is_writeable,uint32_t idx);
void free_frame(page_t *page);

// Implemented in kheap.c
/* At this stage there is no 'free' implemented. */
uint32_t kmalloc_a(uint32_t sz);  // page aligned.
uint32_t kmalloc_p(uint32_t sz, uint32_t *phys); // returns a physical address.
uint32_t kmalloc_ap(uint32_t sz, uint32_t *phys); // page aligned and returns a physical address.
uint32_t kmalloc(uint32_t sz); // vanilla (normal).


uint32_t kmalloc_ap_PT(uint32_t sz, uint32_t *phys); // page aligned and returns a physical address **Only for creating Pages and Page Directories**




#endif