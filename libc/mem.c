#include "mem.h"

extern heap_t *kheap;

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
uint32_t placement_address = 0x10000;
//uint32_t placement_address = 0x20000000;

/* Implementation is just a pointer to some free memory which
 * keeps growing */

extern page_directory_t *kernel_directory;
extern heap_t *kheap;

uint32_t kmalloc_int(uint32_t sz, int align, uint32_t *phys)
{
    if (kheap != 0)
    {
        void *addr = alloc(sz, (uint8_t)align, kheap);
        if (phys != 0)
        {
            page_t *page = get_page((uint32_t)addr, 0, kernel_directory);
            *phys = page->frame*0x1000 + (uint32_t)addr&0xFFF;
        }
        return (uint32_t)addr;
    }
    else
    {
        if (align == 1 && (placement_address & 0xFFFFF000) )
        {
            // Align the placement address;
            placement_address &= 0xFFFFF000;
            placement_address += 0x1000;
        }
        if (phys)
        {
            *phys = placement_address;
        }
        uint32_t tmp = placement_address;
        placement_address += sz;
        return tmp;
    }
}

void kfree(void *p)
{
    free(p, kheap);
}

uint32_t kmalloc_a(uint32_t sz)
{
    return kmalloc_int(sz, 1, 0);
}

uint32_t kmalloc_p(uint32_t sz, uint32_t *phys)
{
    return kmalloc_int(sz, 0, phys);
}

uint32_t kmalloc_ap(uint32_t sz, uint32_t *phys)
{
    return kmalloc_int(sz, 1, phys);
}

uint32_t kmalloc(uint32_t sz)
{
    return kmalloc_int(sz, 0, 0);
}

uint32_t kmalloc_ap_PT(uint32_t sz, uint32_t *phys)
{
    if ((placement_address & 0xFFFFF000) )
        {
            // Align the placement address;
            placement_address &= 0xFFFFF000;
            placement_address += 0x1000;
        }
    

    
    if (phys)
    {
        *phys = placement_address;
    }

    uint32_t tmp = placement_address;
    placement_address += sz;
    
    return tmp;
}