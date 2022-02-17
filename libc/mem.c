#include "mem.h"

void memcpy(uint8_t *source, uint8_t *dest, int nbytes) {
    int i;
    for (i = 0; i < nbytes; i++) {
        *(dest + i) = *(source + i);
    }
}

void memset(uint8_t *dest, uint8_t val, uint32_t len) {
    uint8_t *temp = (uint8_t *)dest;
    for ( ; len != 0; len--) *temp++ = val;
}

/* This should be computed at link time, but a hardcoded
 * value is fine for now. Remember that our kernel starts
 * at 0x1000 as defined on the Makefile */
uint32_t placement_address = 0xc4fff;

/* Implementation is just a pointer to some free memory which
 * keeps growing */

uint32_t kmalloc_a(size_t sz){
    if ((placement_address & 0xFFFFF000)) {
        placement_address &= 0xFFFFF000;
        placement_address += 0x1000;
    }
    uint32_t ret = placement_address;
    placement_address += sz; /* Remember to increment the pointer */
    return ret;

}
uint32_t kmalloc_p(size_t sz, uint32_t *phys){
    /* Save also the physical address */
    if (phys) *phys = placement_address;

    uint32_t ret = placement_address;
    placement_address += sz; /* Remember to increment the pointer */
    return ret;
}
uint32_t kmalloc_ap(size_t sz, uint32_t *phys){
    /* Pages are aligned to 4K, or 0x1000 */
    if ((placement_address & 0xFFFFF000)) {
        placement_address &= 0xFFFFF000;
        placement_address += 0x1000;
    }
    /* Save also the physical address */
    if (phys) *phys = placement_address;

    uint32_t ret = placement_address;
    placement_address += sz; /* Remember to increment the pointer */
    return ret;
}
uint32_t kmalloc(size_t sz){
    uint32_t ret = placement_address;
    placement_address += sz; /* Remember to increment the pointer */
    return ret;
}