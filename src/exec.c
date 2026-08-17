/*
 * exec.c -- Transactional static ELF32 program replacement.
 *
 * The crucial safety rule is that the running image is not destroyed until a
 * complete replacement has been validated, mapped, populated, and given a
 * valid initial stack.  On any ordinary validation/I/O failure we switch back
 * to the old CR3, destroy the temporary directory, and return -1.
 *
 * This loader intentionally supports a small, auditable ELF profile:
 *   - 32-bit, little-endian Intel 80386
 *   - ET_EXEC (fixed-address executable)
 *   - static PT_LOAD segments only
 *   - no interpreter, dynamic linker, relocations, TLS, or shared libraries
 */
#include "exec.h"
#include "elf.h"
#include "fs.h"
#include "kheap.h"
#include "paging.h"
#include "task.h"
#include "signal.h"

#define EXEC_PATH_MAX             256
#define EXEC_ARGUMENT_MAX          16
#define EXEC_ARGUMENT_BYTES_MAX  2048
#define ELF_PROGRAM_HEADER_MAX     32
#define PAGE_SIZE              0x1000
#define PAGE_MASK              0xFFFFF000
#define USER_PROGRAM_MIN       0x00400000

#define ALIGN_DOWN_PAGE(value) ((value) & PAGE_MASK)
#define ALIGN_UP_PAGE(value)   (((value) + PAGE_SIZE - 1) & PAGE_MASK)

extern page_directory_t *kernel_directory;
extern page_directory_t *current_directory;
extern volatile task_t *current_task;
extern void zero_page_physical(u32int physical_address);

/*
 * Everything copied from the old address space lives in one kernel allocation.
 * That allocation remains visible after CR3 changes because the kernel heap is
 * part of the canonical mappings shared by every process directory.
 */
typedef struct exec_request
{
    char path[EXEC_PATH_MAX];
    u32int argument_count;
    u32int argument_offset[EXEC_ARGUMENT_MAX];
    u32int argument_bytes;
    char argument_storage[EXEC_ARGUMENT_BYTES_MAX];
    elf32_header_t header;
    elf32_program_header_t programs[ELF_PROGRAM_HEADER_MAX];
} exec_request_t;

static int copy_bounded_string(char *destination, u32int capacity,
                               const char *source, u32int *length_out)
{
    if (destination == 0 || source == 0 || capacity == 0)
        return -1;
    for (u32int i = 0; i < capacity; i++)
    {
        destination[i] = source[i];
        if (source[i] == '\0')
        {
            if (length_out != 0)
                *length_out = i;
            return 0;
        }
    }
    return -1; /* No terminator appeared within the policy limit. */
}

static int capture_request(exec_request_t *request, const char *path,
                           char *const argv[], char *const envp[])
{
    memset(request, 0, sizeof(*request));
    u32int ignored_length;
    if (copy_bounded_string(request->path, sizeof(request->path), path,
                            &ignored_length) < 0 || request->path[0] == '\0')
        return -1;

    /* Environment strings will be added after argc/argv is proven reliable. */
    if (envp != 0 && envp[0] != 0)
        return -1;

    if (argv == 0)
        return 0;

    for (u32int index = 0; index < EXEC_ARGUMENT_MAX; index++)
    {
        const char *argument = argv[index];
        if (argument == 0)
        {
            request->argument_count = index;
            return 0;
        }

        u32int remaining = EXEC_ARGUMENT_BYTES_MAX - request->argument_bytes;
        u32int length;
        if (remaining == 0 ||
            copy_bounded_string(&request->argument_storage[request->argument_bytes],
                                remaining, argument, &length) < 0)
            return -1;

        request->argument_offset[index] = request->argument_bytes;
        request->argument_bytes += length + 1;
    }

    /* Exactly EXEC_ARGUMENT_MAX non-null entries lacked the required sentinel. */
    return -1;
}

static int read_exact(fs_node_t *file, u32int offset, u32int size, void *buffer)
{
    return read_fs(file, offset, size, (u8int*)buffer) == size ? 0 : -1;
}

static int ranges_overlap(u32int first_start, u32int first_end,
                          u32int second_start, u32int second_end)
{
    return first_start < second_end && second_start < first_end;
}

static int validate_elf(fs_node_t *file, exec_request_t *request,
                        u32int *highest_address_out)
{
    if (file == 0 || (file->flags & 0x7) != FS_FILE || file->read == 0 ||
        file->length < sizeof(elf32_header_t))
        return -1;
    if (read_exact(file, 0, sizeof(request->header), &request->header) < 0)
        return -1;

    elf32_header_t *header = &request->header;
    if (header->identification[0] != 0x7F ||
        header->identification[1] != 'E' ||
        header->identification[2] != 'L' ||
        header->identification[3] != 'F' ||
        header->identification[4] != ELF_CLASS_32 ||
        header->identification[5] != ELF_DATA_LSB ||
        header->identification[6] != ELF_VERSION_CURRENT ||
        header->type != ELF_TYPE_EXECUTABLE ||
        header->machine != ELF_MACHINE_386 ||
        header->version != ELF_VERSION_CURRENT ||
        header->header_size != sizeof(elf32_header_t) ||
        header->program_header_entry_size != sizeof(elf32_program_header_t) ||
        header->program_header_count == 0 ||
        header->program_header_count > ELF_PROGRAM_HEADER_MAX)
        return -1;

    u32int table_size = header->program_header_count *
                        sizeof(elf32_program_header_t);
    if (header->program_header_offset > file->length ||
        table_size > file->length - header->program_header_offset ||
        read_exact(file, header->program_header_offset, table_size,
                   request->programs) < 0)
        return -1;

    u32int highest_address = 0;
    int entry_is_executable = 0;
    u32int loadable_count = 0;

    for (u32int i = 0; i < header->program_header_count; i++)
    {
        elf32_program_header_t *program = &request->programs[i];
        if (program->type != ELF_PROGRAM_LOAD || program->memory_size == 0)
            continue;
        loadable_count++;

        if (program->file_size > program->memory_size ||
            program->file_offset > file->length ||
            program->file_size > file->length - program->file_offset ||
            program->virtual_address < USER_PROGRAM_MIN ||
            program->memory_size > 0xFFFFFFFFU - program->virtual_address)
            return -1;

        u32int end = program->virtual_address + program->memory_size;
        if (end > USER_HEAP_LIMIT || end <= program->virtual_address)
            return -1;

        if (program->alignment > 1)
        {
            if ((program->alignment & (program->alignment - 1)) != 0 ||
                (program->virtual_address & (program->alignment - 1)) !=
                (program->file_offset & (program->alignment - 1)))
                return -1;
        }

        /*
         * Reject even page-level overlap for the first loader.  It keeps page
         * permission calculation unambiguous and our linker script emits fully
         * page-separated text and data segments anyway.
         */
        u32int page_start = ALIGN_DOWN_PAGE(program->virtual_address);
        u32int page_end = ALIGN_UP_PAGE(end);
        for (u32int earlier = 0; earlier < i; earlier++)
        {
            elf32_program_header_t *other = &request->programs[earlier];
            if (other->type != ELF_PROGRAM_LOAD || other->memory_size == 0)
                continue;
            u32int other_end = other->virtual_address + other->memory_size;
            if (ranges_overlap(page_start, page_end,
                               ALIGN_DOWN_PAGE(other->virtual_address),
                               ALIGN_UP_PAGE(other_end)))
                return -1;
        }

        if (end > highest_address)
            highest_address = end;
        if ((program->flags & ELF_FLAG_EXECUTE) &&
            header->entry >= program->virtual_address && header->entry < end)
            entry_is_executable = 1;
    }

    if (loadable_count == 0 || !entry_is_executable)
        return -1;
    *highest_address_out = highest_address;
    return 0;
}

static int map_and_load_segments(fs_node_t *file, exec_request_t *request,
                                 page_directory_t *directory)
{
    for (u32int i = 0; i < request->header.program_header_count; i++)
    {
        elf32_program_header_t *program = &request->programs[i];
        if (program->type != ELF_PROGRAM_LOAD || program->memory_size == 0)
            continue;

        u32int segment_end = program->virtual_address + program->memory_size;
        u32int page_start = ALIGN_DOWN_PAGE(program->virtual_address);
        u32int page_end = ALIGN_UP_PAGE(segment_end);

        for (u32int address = page_start; address < page_end; address += PAGE_SIZE)
        {
            page_t *page = get_page(address, 1, directory);
            if (page == 0 || page->present)
                return -1;
            /* Writable temporarily so the kernel can populate the segment. */
            alloc_frame(page, 0, 1);
            zero_page_physical(page->frame * PAGE_SIZE);
            invalidate_page(address);
        }

        if (program->file_size != 0 &&
            read_exact(file, program->file_offset, program->file_size,
                       (void*)program->virtual_address) < 0)
            return -1;

        /* BSS and alignment padding remain zero from frame initialization. */
        int writable = (program->flags & ELF_FLAG_WRITE) != 0;
        for (u32int address = page_start; address < page_end; address += PAGE_SIZE)
        {
            page_t *page = get_page(address, 0, directory);
            ASSERT(page != 0 && page->present);
            page->rw = writable ? 1 : 0;
            invalidate_page(address);
        }
    }
    return 0;
}

static int build_initial_stack(exec_request_t *request,
                               page_directory_t *directory,
                               u32int *stack_pointer_out)
{
    u32int stack_bottom = USER_STACK_TOP - TASK_STACK_SIZE;
    for (u32int address = stack_bottom; address < USER_STACK_TOP;
         address += PAGE_SIZE)
    {
        page_t *page = get_page(address, 1, directory);
        if (page == 0 || page->present)
            return -1;
        alloc_frame(page, 0, 1);
        zero_page_physical(page->frame * PAGE_SIZE);
        invalidate_page(address);
    }

    u32int stack_pointer = USER_STACK_TOP;
    u32int argument_addresses[EXEC_ARGUMENT_MAX];

    /* Copy strings downward so every pointer in argv refers to the new stack. */
    for (u32int reverse = request->argument_count; reverse > 0; reverse--)
    {
        u32int index = reverse - 1;
        const char *argument =
            &request->argument_storage[request->argument_offset[index]];
        u32int length = (u32int)strlen(argument) + 1;
        if (length > stack_pointer - stack_bottom)
            return -1;
        stack_pointer -= length;
        memcpy((void*)stack_pointer, argument, length);
        argument_addresses[index] = stack_pointer;
    }

    /*
     * Reserve padding before constructing argc/argv/envp so final ESP is
     * 16-byte aligned.  Initial layout follows the conventional System V form:
     * argc, argv pointers, NULL, envp pointers (none yet), NULL.
     */
    stack_pointer &= ~0xFU;
    u32int pointer_bytes = (request->argument_count + 3) * sizeof(u32int);
    if (pointer_bytes > stack_pointer - stack_bottom)
        return -1;
    u32int final_stack_pointer = (stack_pointer - pointer_bytes) & ~0xFU;
    stack_pointer = final_stack_pointer + pointer_bytes;

#define PUSH_STACK(value) do { \
    stack_pointer -= sizeof(u32int); \
    *(u32int*)stack_pointer = (u32int)(value); \
} while (0)

    PUSH_STACK(0); /* Empty envp terminator. */
    PUSH_STACK(0); /* argv[argc]. */
    for (u32int reverse = request->argument_count; reverse > 0; reverse--)
        PUSH_STACK(argument_addresses[reverse - 1]);
    PUSH_STACK(request->argument_count);
#undef PUSH_STACK

    ASSERT(stack_pointer == final_stack_pointer);
    *stack_pointer_out = stack_pointer;
    return 0;
}

int process_execve(const char *path, char *const argv[], char *const envp[],
                   registers_t *return_frame)
{
    task_t *process = (task_t*)current_task;
    if (process == 0 || !process->is_user_process || return_frame == 0)
        return -1;

    exec_request_t *request = (exec_request_t*)kmalloc(sizeof(*request));
    if (request == 0)
        return -1;
    if (capture_request(request, path, argv, envp) < 0)
    {
        kfree(request);
        return -1;
    }

    fs_node_t *file = resolve_path_fs(fs_root, process->working_directory,
                                      request->path);
    u32int highest_loaded_address;
    if (validate_elf(file, request, &highest_loaded_address) < 0)
    {
        kfree(request);
        return -1;
    }

    page_directory_t *old_directory = process->page_directory;
    page_directory_t *new_directory = clone_directory(kernel_directory);

    /*
     * Interrupt entry disabled interrupts before reaching this function.  The
     * kernel code, heap, and current TSS kernel stack are canonical mappings,
     * so they remain usable after loading the temporary CR3 below.
     */
    switch_page_directory(new_directory);

    u32int new_stack_pointer;
    if (map_and_load_segments(file, request, new_directory) < 0 ||
        build_initial_stack(request, new_directory, &new_stack_pointer) < 0)
    {
        switch_page_directory(old_directory);
        destroy_directory(new_directory);
        kfree(request);
        return -1;
    }

    u32int new_heap_start = ALIGN_UP_PAGE(highest_loaded_address);
    if (new_heap_start >= USER_HEAP_LIMIT)
    {
        switch_page_directory(old_directory);
        destroy_directory(new_directory);
        kfree(request);
        return -1;
    }

    /* Commit: from this point the process owns only the replacement image. */
    process->page_directory = new_directory;
    process->heap_start = new_heap_start;
    process->heap_break = new_heap_start;
    process->heap_mapped_end = new_heap_start;
    process->heap_limit = USER_HEAP_LIMIT;

    /* Custom handler addresses belonged to the discarded ELF image. */
    task_signal_exec_reset();

    return_frame->eip = request->header.entry;
    return_frame->useresp = new_stack_pointer;
    return_frame->eax = 0;

    destroy_directory(old_directory);
    kfree(request);
    return 0;
}
