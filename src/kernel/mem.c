#include "mem.h"


void memcpy(uint8_t *dest, uint8_t *source, int nbytes) {
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
//uint32_t placement_address =  0x10000;
//uint32_t placement_address = 0x20000000;

/* Implementation is just a pointer to some free memory which
 * keeps growing */



