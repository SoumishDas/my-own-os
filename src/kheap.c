// kheap.c -- Kernel heap functions, also provides
//            a placement malloc() for use before the heap is 
//            initialised.
//            Written for JamesM's kernel development tutorials.

/*
 * ALLOCATION LIFE CYCLE
 * ---------------------
 * Early boot: kheap == 0, so kmalloc_int() bumps placement_address.
 * Paging setup: paging.c maps frames and constructs the heap's virtual range.
 * Normal operation: kheap != 0, so kmalloc_int() calls the hole allocator.
 *
 * The normal allocator is a boundary-tag allocator.  "Boundary tag" means
 * that every block has a header at its beginning and footer at its end.  That
 * makes both neighboring blocks discoverable in constant time during free().
 * Only free block headers are stored in heap->index, ordered from smallest to
 * largest so find_smallest_hole() implements best fit.
 *
 * Assertions and heap_validate() deliberately fail early when metadata stops
 * describing one contiguous, internally consistent heap.
 */

#include "kheap.h"
#include "paging.h"

/*
 * end is emitted by link.ld immediately after the kernel image.  Before the
 * initrd is considered, it is the first byte early allocation may safely use.
 */
extern u32int end;
/* Next unused address for the early, one-way placement allocator. */
u32int placement_address = (u32int)&end;
/* Paging owns the page tables used to translate post-heap allocations. */
extern page_directory_t *kernel_directory;
/* Null until initialise_paging() installs the real kernel heap. */
heap_t *kheap=0;

u32int kmalloc_int(u32int sz, int align, u32int *phys)
{
    /* Once kheap exists, all allocations use reusable heap blocks. */
    if (kheap != 0)
    {
        void *addr = alloc(sz, (u8int)align, kheap);
        if (phys != 0)
        {
            /*
             * Translate the returned virtual address manually: frame gives
             * the page base and the low 12 address bits give the page offset.
             */
            page_t *page = get_page((u32int)addr, 0, kernel_directory);
            ASSERT(page != 0);
            ASSERT(page->present);
            *phys = page->frame*0x1000 + ((u32int)addr&0xFFF);
        }
        return (u32int)addr;
    }
    else
    {
        /*
         * Bootstrap allocation is just a bump pointer.  Alignment may waste
         * the remaining bytes in the current page; that space is never reused.
         *
         * Only the low 12-bit page offset decides whether padding is required;
         * an offset of zero means the cursor is already page-aligned.
         */
        if (align == 1 && (placement_address & 0xFFF) )
        {
            // Align the placement address;
            placement_address &= 0xFFFFF000;
            ASSERT(placement_address <= 0xFFFFEFFF);
            placement_address += 0x1000;
        }
        if (phys)
        {
            /* Paging is not active here, so early memory is identity-addressed. */
            *phys = placement_address;
        }
        /* Return the old cursor, then reserve sz bytes by advancing it. */
        u32int tmp = placement_address;
        ASSERT(sz <= 0xFFFFFFFF - placement_address);
        placement_address += sz;
        return tmp;
    }
}

void kfree(void *p)
{
    /* Thin public wrapper around the real heap's free operation. */
    ASSERT(kheap != 0);
    free(p, kheap);
}

u32int kmalloc_a(u32int sz)
{
    return kmalloc_int(sz, 1, 0);
}

u32int kmalloc_p(u32int sz, u32int *phys)
{
    return kmalloc_int(sz, 0, phys);
}

u32int kmalloc_ap(u32int sz, u32int *phys)
{
    return kmalloc_int(sz, 1, phys);
}

u32int kmalloc(u32int sz)
{
    return kmalloc_int(sz, 0, 0);
}

static void expand(u32int new_size, heap_t *heap)
{
    ASSERT(heap != 0);
    /* new_size is measured relative to heap->start_address, not absolute. */
    // Sanity check.
    ASSERT(new_size > heap->end_address - heap->start_address);

    /*
     * Round upward because mappings can only be added in complete pages.  The
     * low 12 bits are nonzero exactly when new_size contains a partial page.
     */
    if ((new_size & 0xFFF) != 0)
    {
        ASSERT(new_size <= 0xFFFFF000);
        new_size &= 0xFFFFF000;
        new_size += 0x1000;
    }

    /* Refuse to map beyond the virtual range reserved for this heap. */
    ASSERT(new_size <= heap->max_address - heap->start_address);

    /* Existing heap length is already page-sized by the heap invariants. */
    u32int old_size = heap->end_address-heap->start_address;

    u32int i = old_size;
    while (i < new_size)
    {
        /* Materialize one page-table entry and attach a free physical frame. */
        alloc_frame( get_page(heap->start_address+i, 1, kernel_directory),
                     (heap->supervisor)?1:0, (heap->readonly)?0:1);
        i += 0x1000 /* page size */;
    }
    heap->end_address = heap->start_address+new_size;
}

static u32int contract(u32int new_size, heap_t *heap)
{
    ASSERT(heap != 0);
    /* Contraction is meaningful only when the requested length is smaller. */
    // Sanity check.
    ASSERT(new_size < heap->end_address-heap->start_address);

    /*
     * Keep a partial final page mapped by rounding the retained size upward.
     * The low 12 bits are the offset within a 4 KiB page; clearing those bits
     * gives the preceding page boundary before one page is added.
     */
    if ((new_size & 0xFFF) != 0)
    {
        new_size &= 0xFFFFF000;
        new_size += 0x1000;
    }

    /* Retain a minimum arena so small frees do not constantly map/unmap pages. */
    if (new_size < HEAP_MIN_SIZE)
        new_size = HEAP_MIN_SIZE;

    u32int old_size = heap->end_address-heap->start_address;
    u32int i = old_size - 0x1000;
    while (i >= new_size)
    {
        /* Drop the physical frame backing each page removed from the heap end. */
        free_frame(get_page(heap->start_address+i, 0, kernel_directory));
        invalidate_page(heap->start_address+i);
        i -= 0x1000;
    }

    heap->end_address = heap->start_address + new_size;
    return new_size;
}

/*
 * Return the number of bytes to leave before a block header so that the
 * caller-visible payload begins on a page boundary.  A nonzero prefix becomes
 * a real free hole, so it must be large enough to hold its own header/footer.
 * If the next page boundary would create a tiny unusable prefix, use the
 * following page boundary instead.
 */
static u32int page_alignment_offset(header_t *header)
{
    u32int payload_address = (u32int)header + sizeof(header_t);
    u32int offset = (0x1000 - (payload_address & 0xFFF)) & 0xFFF;
    u32int minimum_hole_size = sizeof(header_t) + sizeof(footer_t);

    if (offset != 0 && offset < minimum_hole_size)
        offset += 0x1000;

    return offset;
}

static s32int find_smallest_hole(u32int size, u8int page_align, heap_t *heap)
{
    /*
     * heap->index is sorted by hole size, so the first suitable entry is the
     * smallest suitable entry.  size already includes header and footer.
     */
    u32int iterator = 0;
    while (iterator < heap->index.size)
    {
        header_t *header = (header_t *)lookup_ordered_array(iterator, &heap->index);
        // If the user has requested the memory be page-aligned
        if (page_align > 0)
        {
            /*
             * The returned payload starts after header_t.  Work out how many
             * bytes at the front of this hole must be skipped so that payload,
             * rather than the hidden header, begins on a page boundary.  Only
             * its low 12 bits are its offset within that page.
             */
            u32int offset = page_alignment_offset(header);
            /* The requested block must still fit after consuming the padding. */
            if (offset <= 0xFFFFFFFF - size && header->size >= size + offset)
                break;
        }
        else if (header->size >= size)
            break;
        iterator++;
    }
    /* Exhaustion means the caller must expand the heap before retrying. */
    if (iterator == heap->index.size)
        return -1; // We got to the end and didn't find anything.
    else
        return iterator;
}

static s8int header_t_less_than(void*a, void *b)
{
    /* This callback turns ordered_array_t into a smallest-hole-first index. */
    return (((header_t*)a)->size < ((header_t*)b)->size)?1:0;
}

/* Verify block boundaries, boundary tags, hole membership, and index ordering. */
static void heap_validate(heap_t *heap)
{
    ASSERT(heap != 0);
    ASSERT(heap->start_address < heap->end_address);
    ASSERT(heap->end_address <= heap->max_address);

    u32int address = heap->start_address;
    u32int hole_count = 0;
    while (address < heap->end_address)
    {
        header_t *header = (header_t*)address;
        ASSERT(header->magic == HEAP_MAGIC);
        ASSERT(header->size >= sizeof(header_t) + sizeof(footer_t));
        ASSERT(header->size <= heap->end_address - address);

        footer_t *footer = (footer_t*)(address + header->size - sizeof(footer_t));
        ASSERT(footer->magic == HEAP_MAGIC);
        ASSERT(footer->header == header);

        if (header->is_hole)
        {
            u32int matches = 0;
            u32int i;
            for (i = 0; i < heap->index.size; i++)
                if (lookup_ordered_array(i, &heap->index) == (void*)header)
                    matches++;
            ASSERT(matches == 1);
            hole_count++;
        }
        address += header->size;
    }
    ASSERT(address == heap->end_address);
    ASSERT(hole_count == heap->index.size);

    u32int i;
    for (i = 0; i < heap->index.size; i++)
    {
        header_t *hole = (header_t*)lookup_ordered_array(i, &heap->index);
        ASSERT((u32int)hole >= heap->start_address);
        ASSERT((u32int)hole < heap->end_address);
        ASSERT(hole->magic == HEAP_MAGIC);
        ASSERT(hole->is_hole == 1);
        if (i > 0)
        {
            header_t *previous = (header_t*)lookup_ordered_array(i-1, &heap->index);
            ASSERT(previous->size <= hole->size);
        }
    }
}

heap_t *create_heap(u32int start, u32int end_addr, u32int max, u8int supervisor, u8int readonly)
{
    /* heap_t itself is allocated by the bootstrap allocator at this stage. */
    heap_t *heap = (heap_t*)kmalloc(sizeof(heap_t));

    // All our assumptions are made on startAddress and endAddress being page-aligned.
    ASSERT(start%0x1000 == 0);
    ASSERT(end_addr%0x1000 == 0);
    ASSERT(max%0x1000 == 0 || max == 0xCFFFF000);
    ASSERT(start < end_addr);
    ASSERT(end_addr <= max);
    ASSERT(HEAP_INDEX_SIZE <= 0xFFFFFFFF / sizeof(type_t));
    ASSERT(end_addr - start > sizeof(type_t)*HEAP_INDEX_SIZE +
                              sizeof(header_t) + sizeof(footer_t));
    
    /*
     * Place the free-hole pointer index directly at the requested heap start.
     * This permanently reserves HEAP_INDEX_SIZE pointer slots at the front.
     */
    heap->index = place_ordered_array( (void*)start, HEAP_INDEX_SIZE, &header_t_less_than);
    
    /* Block storage begins after the index's backing pointer array. */
    start += sizeof(type_t)*HEAP_INDEX_SIZE;

    /*
     * Round the block arena start up only when its low 12-bit page offset is
     * nonzero.  Clearing that offset obtains the preceding page boundary.
     */
    if ((start & 0xFFF) != 0)
    {
        start &= 0xFFFFF000;
        start += 0x1000;
    }
    // Write the start, end and max addresses into the heap structure.
    heap->start_address = start;
    heap->end_address = end_addr;
    heap->max_address = max;
    heap->supervisor = supervisor;
    heap->readonly = readonly;

    /* Initially every byte in the block arena belongs to one large free hole. */
    header_t *hole = (header_t *)start;
    hole->size = end_addr-start;
    hole->magic = HEAP_MAGIC;
    hole->is_hole = 1;
    footer_t *hole_footer = (footer_t*)(end_addr - sizeof(footer_t));
    hole_footer->magic = HEAP_MAGIC;
    hole_footer->header = hole;
    insert_ordered_array((void*)hole, &heap->index);     

    heap_validate(heap);

    return heap;
}

void *alloc(u32int size, u8int page_align, heap_t *heap)
{
    ASSERT(heap != 0);
    ASSERT(size > 0);
    ASSERT(size <= 0xFFFFFFFF - sizeof(header_t) - sizeof(footer_t));
    heap_validate(heap);
    /* Caller asks for payload bytes; the heap must reserve metadata too. */
    u32int new_size = size + sizeof(header_t) + sizeof(footer_t);
    // Find the smallest hole that will fit.
    s32int iterator = find_smallest_hole(new_size, page_align, heap);

    if (iterator == -1) // If we didn't find a suitable hole
    {
        /* No current hole fits, so map enough pages to make the arena larger. */
        // Save some previous data.
        u32int old_length = heap->end_address - heap->start_address;
        u32int old_end_address = heap->end_address;

        // We need to allocate some more space.
        ASSERT(old_length <= 0xFFFFFFFF - new_size);
        expand(old_length+new_size, heap);
        u32int new_length = heap->end_address-heap->start_address;

        /*
         * The index is ordered by size, not address.  Scan every indexed hole
         * to find the one with the numerically greatest address.
         */
        u32int scan_index = 0;
        // Vars to hold the index of, and value of, the endmost header found so far.
        u32int idx = -1; u32int value = 0x0;
        while (scan_index < heap->index.size)
        {
            u32int tmp = (u32int)lookup_ordered_array(scan_index, &heap->index);
            if (tmp > value)
            {
                value = tmp;
                idx = scan_index;
            }
            scan_index++;
        }

        /*
         * Only a hole ending exactly at old_end_address is adjacent to the new
         * pages. Otherwise the newly mapped tail must be a separate hole.
         */
        header_t *end_hole = 0;
        if (idx != (u32int)-1)
            end_hole = (header_t*)lookup_ordered_array(idx, &heap->index);

        if (end_hole == 0 ||
            (u32int)end_hole + end_hole->size != old_end_address)
        {
            header_t *header = (header_t *)old_end_address;
            header->magic = HEAP_MAGIC;
            header->size = new_length - old_length;
            header->is_hole = 1;
            footer_t *footer = (footer_t *) (old_end_address + header->size - sizeof(footer_t));
            footer->magic = HEAP_MAGIC;
            footer->header = header;
            insert_ordered_array((void*)header, &heap->index);
        }
        else
        {
            /*
             * Remove the adjacent tail hole before changing its size, then
             * reinsert it so the size-sorted index remains ordered.
             */
            remove_ordered_array(idx, &heap->index);
            end_hole->size += new_length - old_length;
            // Rewrite the footer.
            footer_t *footer = (footer_t *) ( (u32int)end_hole + end_hole->size - sizeof(footer_t) );
            footer->header = end_hole;
            footer->magic = HEAP_MAGIC;
            insert_ordered_array((void*)end_hole, &heap->index);
        }
        /* Retry the exact request now that additional space has been mapped. */
        return alloc(size, page_align, heap);
    }

    header_t *orig_hole_header = (header_t *)lookup_ordered_array(iterator, &heap->index);
    u32int orig_hole_pos = (u32int)orig_hole_header;
    u32int orig_hole_size = orig_hole_header->size;
    u32int minimum_hole_size = sizeof(header_t) + sizeof(footer_t);

    /* The selected entry will either be allocated or replaced by split holes. */
    remove_ordered_array(iterator, &heap->index);

    /*
     * For aligned allocation, leave a leading free hole so the allocated
     * block's payload begins exactly on a page boundary.  new_location is the
     * new hidden-header address, one sizeof(header_t) before that boundary.
     * page_alignment_offset() guarantees that a nonzero leading hole is large
     * enough to contain valid boundary tags.
     */
    u32int leading_hole_size = page_align ? page_alignment_offset(orig_hole_header) : 0;
    if (leading_hole_size != 0)
    {
        header_t *hole_header = (header_t *)orig_hole_pos;
        hole_header->size     = leading_hole_size;
        hole_header->magic    = HEAP_MAGIC;
        hole_header->is_hole  = 1;
        footer_t *hole_footer = (footer_t *) (orig_hole_pos + leading_hole_size - sizeof(footer_t));
        hole_footer->magic    = HEAP_MAGIC;
        hole_footer->header   = hole_header;
        insert_ordered_array((void*)hole_header, &heap->index);
        orig_hole_pos         += leading_hole_size;
        orig_hole_size        -= leading_hole_size;
    }

    /*
     * A trailing free hole also needs both boundary tags.  If the remainder is
     * too small for that metadata, give those bytes to the allocation instead
     * of creating a malformed tiny hole.  find_smallest_hole() guarantees that
     * orig_hole_size is at least new_size, so this subtraction cannot underflow.
     */
    if (orig_hole_size - new_size < minimum_hole_size)
    {
        size += orig_hole_size - new_size;
        new_size = orig_hole_size;
    }

    /* Write boundary tags for the caller-owned portion. */
    header_t *block_header  = (header_t *)orig_hole_pos;
    block_header->magic     = HEAP_MAGIC;
    block_header->is_hole   = 0;
    block_header->size      = new_size;
    // ...And the footer
    footer_t *block_footer  = (footer_t *) (orig_hole_pos + sizeof(header_t) + size);
    block_footer->magic     = HEAP_MAGIC;
    block_footer->header    = block_header;

    /* Any remainder reaching minimum_hole_size becomes a valid hole on the right. */
    if (orig_hole_size - new_size >= minimum_hole_size)
    {
        header_t *hole_header = (header_t *) (orig_hole_pos + sizeof(header_t) + size + sizeof(footer_t));
        hole_header->magic    = HEAP_MAGIC;
        hole_header->is_hole  = 1;
        hole_header->size     = orig_hole_size - new_size;
        footer_t *hole_footer = (footer_t *) ( (u32int)hole_header + orig_hole_size - new_size - sizeof(footer_t) );
        ASSERT((u32int)hole_footer < heap->end_address);
        hole_footer->magic = HEAP_MAGIC;
        hole_footer->header = hole_header;
        /* Make the remainder discoverable by future allocations. */
        insert_ordered_array((void*)hole_header, &heap->index);
    }
    
    /* Hide header_t from the caller by returning the byte immediately after it. */
    heap_validate(heap);
    return (void *) ( (u32int)block_header+sizeof(header_t) );
}

void free(void *p, heap_t *heap)
{
    /* Matching standard free(), freeing NULL is deliberately harmless. */
    if (p == 0)
        return;

    ASSERT(heap != 0);
    heap_validate(heap);
    ASSERT((u32int)p >= heap->start_address + sizeof(header_t));
    ASSERT((u32int)p < heap->end_address);

    /* Recover the boundary tags which surround the caller's payload. */
    header_t *header = (header_t*) ( (u32int)p - sizeof(header_t) );
    ASSERT(header->magic == HEAP_MAGIC);
    ASSERT(header->is_hole == 0);
    ASSERT(header->size >= sizeof(header_t) + sizeof(footer_t));
    ASSERT(header->size <= heap->end_address - (u32int)header);
    footer_t *footer = (footer_t*) ( (u32int)header + header->size - sizeof(footer_t) );

    /* Fail loudly if p is invalid or an earlier overwrite damaged metadata. */
    ASSERT(header->magic == HEAP_MAGIC);
    ASSERT(footer->magic == HEAP_MAGIC);

    /* Ownership ends here; the region can now satisfy later allocations. */
    header->is_hole = 1;

    /* Nonzero means the final free hole still needs publishing in the index. */
    char do_add = 1;

    /*
     * Coalesce left: the bytes immediately before our header should be the
     * left neighbor's footer.  Its back-pointer reveals the neighbor header.
     * The boundary check is evaluated first so the initial heap block never
     * attempts to read a footer before heap->start_address.
     */
    // If the thing immediately to the left of us is a footer...
    footer_t *test_footer = (footer_t*) ( (u32int)header - sizeof(footer_t) );
    if ((u32int)header > heap->start_address &&
        test_footer->magic == HEAP_MAGIC &&
        test_footer->header->is_hole == 1)
    {
        /* Remove the left hole under its old size before enlarging it. */
        u32int iterator = 0;
        while ( (iterator < heap->index.size) &&
                (lookup_ordered_array(iterator, &heap->index) != (void*)test_footer->header) )
            iterator++;
        ASSERT(iterator < heap->index.size);
        remove_ordered_array(iterator, &heap->index);

        u32int cache_size = header->size; // Cache our current size.
        header = test_footer->header;     // Rewrite our header with the new one.
        footer->header = header;          // Rewrite our footer to point to the new header.
        header->size += cache_size;       // Change the size.
    }

    /*
     * Coalesce right: the bytes immediately after our footer should be the
     * right neighbor's header.  Remove that neighbor from the free-hole index
     * because it becomes part of our larger combined hole.
     * The boundary check is evaluated first so the final heap block never
     * attempts to read a header at heap->end_address.
     */
    // If the thing immediately to the right of us is a header...
    header_t *test_header = (header_t*) ( (u32int)footer + sizeof(footer_t) );
    if ((u32int)test_header < heap->end_address &&
        test_header->magic == HEAP_MAGIC &&
        test_header->is_hole)
    {
        header->size += test_header->size; // Increase our size.
        test_footer = (footer_t*) ( (u32int)test_header + // Rewrite it's footer to point to our header.
                                    test_header->size - sizeof(footer_t) );
        footer = test_footer;
        footer->header = header;
        // Find and remove this header from the index.
        u32int iterator = 0;
        while ( (iterator < heap->index.size) &&
                (lookup_ordered_array(iterator, &heap->index) != (void*)test_header) )
            iterator++;

        // Make sure we actually found the item.
        ASSERT(iterator < heap->index.size);
        // Remove it.
        remove_ordered_array(iterator, &heap->index);
    }

    /*
     * Only a hole touching heap->end_address can release trailing pages.  The
     * surviving portion, if any, needs a rewritten footer at its new end.
     * If contraction removes the complete hole, no now-nonexistent hole may be
     * inserted afterward.
     */
    if ( (u32int)footer+sizeof(footer_t) == heap->end_address)
    {
        u32int old_length = heap->end_address-heap->start_address;
        u32int new_length = contract( (u32int)header - heap->start_address, heap);
        // Check how big we will be after resizing.
        if (header->size > old_length-new_length)
        {
            // We will still exist, so resize us.
            header->size -= old_length-new_length;
            footer = (footer_t*) ( (u32int)header + header->size - sizeof(footer_t) );
            footer->magic = HEAP_MAGIC;
            footer->header = header;
        }
        else
        {
            /* The contracted pages consume the complete, not-yet-indexed hole. */
            do_add = 0;
        }
    }

    /*
     * Publish the final free hole only after all size changes, preserving the
     * index's smallest-to-largest ordering.
     */
    if (do_add == 1)
        insert_ordered_array((void*)header, &heap->index);

    heap_validate(heap);

}
