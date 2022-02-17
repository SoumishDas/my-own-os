#ifndef MEM_H
#define MEM_H

#include <stdint.h>
#include <stddef.h>

void memcpy(uint8_t *source, uint8_t *dest, int nbytes);
void memset(uint8_t *dest, uint8_t val, uint32_t len);

/* At this stage there is no 'free' implemented. */
uint32_t kmalloc_a(size_t sz);  // page aligned.
uint32_t kmalloc_p(size_t sz, uint32_t *phys); // returns a physical address.
uint32_t kmalloc_ap(size_t sz, uint32_t *phys); // page aligned and returns a physical address.
uint32_t kmalloc(size_t sz); // vanilla (normal).

#endif
