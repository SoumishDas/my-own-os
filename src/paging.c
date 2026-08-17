
/*
 * RESPONSIBILITIES OF THIS FILE
 * -----------------------------
 *  - Track which physical 4 KiB frames are free using one bit per frame.
 *  - Allocate page tables on demand and fill their page-table entries.
 *  - Construct the initial kernel address space and turn paging on.
 *  - Decode page faults.
 *  - Clone an address space when creating a task.
 *
 * PAGE SIZE is 0x1000 (4096).  Dividing an address by 0x1000 discards its
 * within-page offset.  Multiplying a frame number by 0x1000 reconstructs that
 * frame's physical base address.
 *
 * User-accessible flags are intentionally retained for the current user-mode
 * experiment; changing that policy is outside this repair pass.
 */

#include "paging.h"
#include "kheap.h"
#include "monitor.h"

/* Assembly helper copies exactly one 4 KiB physical frame. */
extern void copy_page_physical(u32int source, u32int destination);

/* Canonical directory containing mappings shared by every process. */
page_directory_t *kernel_directory=0;

/* Directory currently loaded in CR3. */
page_directory_t *current_directory=0;

/*
 * Physical-frame bitmap: bit N is 1 when frame N is occupied and 0 when free.
 * Packing 32 frame states into each u32int keeps the bookkeeping small.
 */
u32int *frames;
/* Total number of physical frames the kernel believes exist. */
u32int nframes;
/* Number of u32int words allocated for frames[] bitmap. */
static u32int frame_word_count;

// Defined in kheap.c
extern u32int placement_address;
extern heap_t *kheap;

/* Convert frame number N into frames[word] and the bit within that word. */
#define BITS_PER_FRAME_WORD 32
#define INDEX_FROM_BIT(a)   ((a) / BITS_PER_FRAME_WORD)
#define OFFSET_FROM_BIT(a)  ((a) % BITS_PER_FRAME_WORD)

// Static function to set a bit in the frames bitset
static void set_frame(u32int frame_addr)
{
    /* Convert byte address to frame number, then set its occupied bit. */
    u32int frame = frame_addr/0x1000;
    ASSERT(frame < nframes);
    u32int idx = INDEX_FROM_BIT(frame);
    u32int off = OFFSET_FROM_BIT(frame);
    frames[idx] |= (0x1 << off);
}

// Static function to clear a bit in the frames bitset
static void clear_frame(u32int frame_addr)
{
    /* Convert byte address to frame number, then clear its occupied bit. */
    u32int frame = frame_addr/0x1000;
    ASSERT(frame < nframes);
    u32int idx = INDEX_FROM_BIT(frame);
    u32int off = OFFSET_FROM_BIT(frame);
    frames[idx] &= ~(0x1 << off);
}

// Static function to test if a bit is set.
static u32int test_frame(u32int frame_addr)
{
    /* A nonzero result means this physical frame is currently reserved. */
    u32int frame = frame_addr/0x1000;
    ASSERT(frame < nframes);
    u32int idx = INDEX_FROM_BIT(frame);
    u32int off = OFFSET_FROM_BIT(frame);
    return (frames[idx] & (0x1 << off));
}

// Static function to find the first free frame.
static u32int first_frame()
{
    /* Search bitmap words from low memory toward high memory. */
    u32int i, j;
    for (i = 0; i < frame_word_count; i++)
    {
        if (frames[i] != 0xFFFFFFFF) // At least one of this word's 32 bits is free.
        {
            /* Locate the first zero bit inside this partially free word. */
            for (j = 0; j < 32; j++)
            {
                u32int toTest = 0x1 << j;
                if ( !(frames[i]&toTest) )
                {
                    /* Convert word index plus bit index back to a frame number. */
                    u32int frame = i*BITS_PER_FRAME_WORD+j;
                    if (frame < nframes)
                        return frame;
                }
            }
        }
    }
    /* Unsigned -1 is the sentinel meaning that every tracked frame is busy. */
    return (u32int)-1;
}

// Function to allocate a frame.
void alloc_frame(page_t *page, int is_kernel, int is_writeable)
{
    ASSERT(page != 0);
    /* Do not replace a mapping which already owns a physical frame. */
    if (page->present)
    {
        return;
    }
    else
    {
        u32int idx = first_frame();
        if (idx == (u32int)-1)
            PANIC("Out of physical memory");
        ASSERT(!test_frame(idx*0x1000));
        set_frame(idx*0x1000);
        /* Fill the hardware-visible flags and store the frame number. */
        page->present = 1;
        page->rw = (is_writeable==1)?1:0;
        page->user = (is_kernel==1)?0:1;
        page->frame = idx;
    }
}

// Function to deallocate a frame.
void free_frame(page_t *page)
{
    ASSERT(page != 0);
    u32int frame;
    if (!page->present)
    {
        return;
    }
    else
    {
        /* Return the frame to the bitmap and invalidate this translation. */
        frame = page->frame;
        clear_frame(frame * 0x1000);
        page->present = 0;
        page->rw = 1;
        page->user = 0;
        page->accessed = 0;
        page->dirty = 0;
        page->frame = 0x0;
    }
}

void invalidate_page(u32int address)
{
    asm volatile("invlpg (%0)" : : "r"(address) : "memory");
}

u32int paging_total_memory_kib(void)
{
    return nframes * 4; /* Every frame is 4 KiB. */
}

void initialise_paging(u32int physical_memory_end)
{
    /*
     * Build the frame bitmap from the RAM limit reported by Multiboot.
     */
    physical_memory_end &= 0xFFFFF000;
    ASSERT(physical_memory_end > placement_address);
    nframes = physical_memory_end / 0x1000;
    /* Add 31 before division to round the bitmap word count upward. */
    frame_word_count = INDEX_FROM_BIT(nframes + 31);
    frames = (u32int*)kmalloc(frame_word_count * sizeof(u32int));
    memset(frames, 0, frame_word_count * sizeof(u32int));

    /* Mark padding bits in the final bitmap word unavailable. */
    u32int padding_frame;
    for (padding_frame = nframes;
         padding_frame < frame_word_count * BITS_PER_FRAME_WORD;
         padding_frame++)
    {
        frames[INDEX_FROM_BIT(padding_frame)] |=
            (1U << OFFSET_FROM_BIT(padding_frame));
    }
    
    /*
     * The hardware directory needs alignment because physical low bits carry
     * flags.  physicalAddr points specifically to its tablesPhysical member.
     */
    kernel_directory = (page_directory_t*)kmalloc_a(sizeof(page_directory_t));
    memset(kernel_directory, 0, sizeof(page_directory_t));
    kernel_directory->physicalAddr = (u32int)kernel_directory->tablesPhysical;

    /*
     * Pre-create page TABLES covering the initial heap range, but do not assign
     * physical frames yet.  get_page(make=1) may itself consume placement
     * memory for a newly required table.
     */
    // Here we call get_page but not alloc_frame. This causes page_table_t's 
    // to be created where necessary. We can't allocate frames yet because they
    // they need to be identity mapped first below, and yet we can't increase
    // placement_address between identity mapping and enabling the heap!
    /*
     */

    u32int i = 0;
    for (i = KHEAP_START; i < KHEAP_START+KHEAP_INITIAL_SIZE; i += 0x1000)
        get_page(i, 1, kernel_directory);

    /*
     * Identity-map bootstrap memory so the CPU can continue executing and all
     * placement allocations stay reachable immediately after CR0.PG is set.
     *
     * The loop condition follows placement_address dynamically because creating
     * a page table can itself advance that bootstrap-allocation boundary.
     */
    // We need to identity map (phys addr = virt addr) from
    // 0x0 to the end of used memory, so we can access this
    // transparently, as if paging wasn't enabled.
    // NOTE that we use a while loop here deliberately.
    // inside the loop body we actually change placement_address
    // by calling kmalloc(). A while loop causes this to be
    // computed on-the-fly rather than once at the start.
    // Allocate a lil' bit extra so the kernel heap can be
    // initialised properly.
    i = 0;
    ASSERT(placement_address <= 0xFFFFEFFF);
    while (i < placement_address + 0x1000)
    {
        /*
         * Allocate frames in ascending order.  Because the bitmap begins empty,
         * frame N is selected for virtual page N, producing identity mapping.
         * 
         * user=1 is intentionally retained for the current user-mode setup.
         */
        page_t *page = get_page(i, 1, kernel_directory);
        ASSERT(page != 0);
        ASSERT(i / 0x1000 < nframes);
        set_frame(i);
        page->present = 1;
        page->rw = 0;
        page->user = 1; /* Intentionally retained until user-access policy changes. */
        page->frame = i / 0x1000;
        i += 0x1000;
    }

    /*
     * Attach physical frames to the heap page entries created above.
     * User accessibility is intentionally retained for the current setup; heap
     * pages are writable because allocator metadata and payloads live there.
     */
    for (i = KHEAP_START; i < KHEAP_START+KHEAP_INITIAL_SIZE; i += 0x1000)
        alloc_frame( get_page(i, 1, kernel_directory), 0, 1);

    /* Handler must exist before the first translation failure can occur. */
    register_interrupt_handler(14, page_fault);

    /* Load CR3 and set CR0.PG; all subsequent memory accesses are translated. */
    switch_page_directory(kernel_directory);

    /* Overlay boundary-tag allocator metadata onto the mapped heap range. */
    kheap = create_heap(KHEAP_START, KHEAP_START+KHEAP_INITIAL_SIZE, 0xCFFFF000, 0, 0);

    /* Create the first task-style directory, sharing all kernel page tables. */
    current_directory = clone_directory(kernel_directory);
    switch_page_directory(current_directory);
}

void switch_page_directory(page_directory_t *dir)
{
    /* CR3 takes the PHYSICAL address of the hardware directory entries. */
    current_directory = dir;
    asm volatile("mov %0, %%cr3":: "r"(dir->physicalAddr));
    u32int cr0;
    asm volatile("mov %%cr0, %0": "=r"(cr0));
    cr0 |= 0x80000000; // Bit 31 (PG) enables paging; setting it again is harmless.
    asm volatile("mov %0, %%cr0":: "r"(cr0));
}

page_t *get_page(u32int address, int make, page_directory_t *dir)
{
    ASSERT(dir != 0);
    /* Remove the 12-bit within-page offset to obtain a virtual page number. */
    address /= 0x1000;
    /* Every table owns 1024 consecutive virtual pages (4 MiB). */
    u32int table_idx = address / 1024;

    if (dir->tables[table_idx]) // If this table is already assigned
    {
        /* Low ten bits of page number choose the entry within that table. */
        return &dir->tables[table_idx]->pages[address%1024];
    }
    else if(make)
    {
        /*
         * Allocate one aligned page table and request its physical address.
         * Hardware consumes the physical address; C code uses the virtual one.
         */
        u32int tmp;
        dir->tables[table_idx] = (page_table_t*)kmalloc_ap(sizeof(page_table_t), &tmp);
        memset(dir->tables[table_idx], 0, sizeof(page_table_t));
        /*
         * Low three bits: present | writable | user-accessible.
         * User access (0x7 rather than 0x3) is intentionally retained for now.
         */
        dir->tablesPhysical[table_idx] = tmp | 0x7; // PRESENT, RW, US.
        return &dir->tables[table_idx]->pages[address%1024];
    }
    else
    {
        return 0;
    }
}


void page_fault(registers_t *regs)
{
    // A page fault has occurred.
    // The faulting address is stored in the CR2 register.
    u32int faulting_address;
    asm volatile("mov %%cr2, %0" : "=r" (faulting_address));
    
    /*
     * x86 pushes a bit field describing the failed access.  Note that the local
     * variable named present is inverted: true means "page was not present".
     */
    int present   = !(regs->err_code & 0x1); // Page not present
    int rw = regs->err_code & 0x2;           // Write operation?
    int us = regs->err_code & 0x4;           // Processor was in user-mode?
    int reserved = regs->err_code & 0x8;     // Overwritten CPU-reserved bits of page entry?
    int id = regs->err_code & 0x10;          // Caused by an instruction fetch?

    /* Print every applicable cause before halting through PANIC. */
    monitor_write("Page fault! ( ");
    if (present) {monitor_write("present ");}
    if (rw) {monitor_write("read-only ");}
    if (us) {monitor_write("user-mode ");}
    if (reserved) {monitor_write("reserved ");}
    if (id) {monitor_write("instruction-fetch ");}
    monitor_write(") at 0x");
    monitor_write_hex(faulting_address);
    monitor_write(" - EIP: ");
    monitor_write_hex(regs->eip);
    monitor_write("\n");
    PANIC("Page fault");
}

static page_table_t *clone_table(page_table_t *src, u32int *physAddr)
{
    /* Allocate one page-aligned destination table and report its physical base. */
    page_table_t *table = (page_table_t*)kmalloc_ap(sizeof(page_table_t), physAddr);
    /*
     * Ensure that all destination entries begin not-present and frame-free.
     */
    memset(table, 0, sizeof(page_table_t));

    // For every entry in the table...
    int i;
    for (i = 0; i < 1024; i++)
    {
        /*
         * Skip entries which do not describe a present mapping.
         */
        if (!src->pages[i].present)
            continue;
        /* Give the clone independent physical storage. */
        alloc_frame(&table->pages[i], 0, 0);
        /* Preserve access/status flags after alloc_frame supplied a frame. */
        if (src->pages[i].present) table->pages[i].present = 1;
        if (src->pages[i].rw)      table->pages[i].rw = 1;
        if (src->pages[i].user)    table->pages[i].user = 1;
        if (src->pages[i].accessed)table->pages[i].accessed = 1;
        if (src->pages[i].dirty)   table->pages[i].dirty = 1;
        /* Copy page contents without relying on matching virtual mappings. */
        copy_page_physical(src->pages[i].frame*0x1000, table->pages[i].frame*0x1000);
    }
    return table;
}

page_directory_t *clone_directory(page_directory_t *src)
{
    ASSERT(src != 0);
    u32int phys;
    // Make a new page directory and obtain its physical address.
    page_directory_t *dir = (page_directory_t*)kmalloc_ap(sizeof(page_directory_t), &phys);
    // Ensure that it is blank.
    memset(dir, 0, sizeof(page_directory_t));

    /*
     * kmalloc_ap reports the physical address corresponding to dir itself.
     * Add the in-structure byte offset to locate tablesPhysical physically.
     */
    u32int offset = (u32int)dir->tablesPhysical - (u32int)dir;

    // Then the physical address of dir->tablesPhysical is:
    dir->physicalAddr = phys + offset;

    /*
     * Kernel tables are deliberately shared, so kernel mappings remain common
     * to all tasks.  Non-kernel tables are deep-copied page by page.
     */
    int i;
    for (i = 0; i < 1024; i++)
    {
        if (!src->tables[i])
            continue;

        if (kernel_directory->tables[i] == src->tables[i])
        {
            /* Both software pointer and hardware PDE refer to the shared table. */
            dir->tables[i] = src->tables[i];
            dir->tablesPhysical[i] = src->tablesPhysical[i];
        }
        else
        {
            /* Private table: allocate new frames and duplicate their contents. */
            u32int phys;
            dir->tables[i] = clone_table(src->tables[i], &phys);
            dir->tablesPhysical[i] = phys | 0x07;
        }
    }
    return dir;
}
