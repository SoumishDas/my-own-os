#ifndef PAGING_H
#define PAGING_H

#include "../../drivers/screen.h"

#include "../../cpu/register.h"

#include "mem_vars.h"





// Macros used in the bitset algorithms.
#define INDEX_FROM_BIT(a) (a/(8*4))
#define OFFSET_FROM_BIT(a) (a%(8*4))


/**
  Sets up the environment, page directories etc and
  enables paging.
**/
void initialise_paging();

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

#endif