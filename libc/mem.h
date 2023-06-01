#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include "../kernel/memory_allocator/mem_vars.h"



void memcpy(uint8_t *source, uint8_t *dest, int nbytes);
void memset(uint8_t *dest, uint8_t val, uint32_t len);

/* At this stage there is no 'free' implemented. */
uint32_t kmalloc_a(uint32_t sz);  // page aligned.
uint32_t kmalloc_p(uint32_t sz, uint32_t *phys); // returns a physical address.
uint32_t kmalloc_ap(uint32_t sz, uint32_t *phys); // page aligned and returns a physical address.
uint32_t kmalloc(uint32_t sz); // vanilla (normal).
void kfree(void *ptr);

uint32_t kmalloc_ap_PT(uint32_t sz, uint32_t *phys); // page aligned and returns a physical address **Only for creating Pages and Page Directories**
#endif
