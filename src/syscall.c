/*
 * syscall.c -- int 0x80 dispatcher and user-side assembly wrappers.
 *
 * Interrupt entry switches from ring 3 to the TSS kernel stack. interrupt.s
 * saves registers, this dispatcher invokes one fixed-signature service, and
 * IRET eventually restores user execution with the result in EAX.
 *
 * Pointer arguments are currently trusted because this experimental address
 * space still exposes kernel mappings to ring 3. Real isolation will require
 * user-range validation plus copy_from_user/copy_to_user helpers.
 */
#include "syscall.h"
#include "isr.h"
#include "monitor.h"
#include "keyboard.h"
#include "fs.h"
#include "task.h"
#include "timer.h"
#include "paging.h"
#include "exec.h"

typedef int (*syscall_service_t)(u32int, u32int, u32int, u32int, u32int);

static int service_write(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)b;(void)c;(void)d;(void)e; if (!a) return -1; monitor_write((const char*)a); return 0; }
static int service_putchar(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)b;(void)c;(void)d;(void)e; monitor_put((char)a); return 0; }
static int service_clear(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; monitor_clear(); return 0; }
static int service_getchar(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; return keyboard_getchar(); }

static int service_readdir(u32int index,u32int name_address,u32int capacity,u32int d,u32int e)
{
    (void)d;(void)e;
    fs_node_t *directory = task_get_working_directory();
    if (!directory || !name_address || !capacity) return -1;
    struct dirent *entry = readdir_fs(directory, index);
    if (!entry) return 0;
    char *destination = (char*)name_address;
    u32int i = 0;
    while (i + 1 < capacity && entry->name[i]) { destination[i] = entry->name[i]; i++; }
    destination[i] = '\0';
    return 1;
}

static int service_readfile(u32int name_address,u32int buffer_address,u32int capacity,u32int d,u32int e)
{
    (void)d;(void)e;
    if (!fs_root || !name_address || !buffer_address || !capacity) return -1;
    fs_node_t *node = resolve_path_fs(fs_root, task_get_working_directory(),
                                      (const char*)name_address);
    if (!node || (node->flags & 0x7) != FS_FILE) return -1;
    u32int amount = node->length < capacity ? node->length : capacity;
    return (int)read_fs(node, 0, amount, (u8int*)buffer_address);
}

static int service_getpid(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; return getpid(); }
static int service_ticks(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; return (int)timer_ticks(); }
static int service_memory(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; return (int)paging_total_memory_kib(); }
static int service_hex(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)b;(void)c;(void)d;(void)e; monitor_write_hex(a); return 0; }
static int service_dec(u32int a,u32int b,u32int c,u32int d,u32int e)
{ (void)b;(void)c;(void)d;(void)e; monitor_write_dec(a); return 0; }

static int service_open(u32int path_address,u32int flags,u32int c,u32int d,u32int e)
{
    (void)c;(void)d;(void)e;
    if (path_address == 0 || fs_root == 0 || flags != OPEN_READ_ONLY)
        return -1;
    fs_node_t *node = resolve_path_fs(fs_root, task_get_working_directory(),
                                      (const char*)path_address);
    /* Directories will later be opened too, but read() currently means bytes. */
    if (node == 0 || (node->flags & 0x7) != FS_FILE || node->read == 0)
        return -1;
    return task_open_descriptor(node, flags);
}

static int service_fd_read(u32int fd,u32int buffer_address,u32int count,u32int d,u32int e)
{
    (void)d;(void)e;
    if (buffer_address == 0 || count == 0)
        return count == 0 ? 0 : -1;

    file_descriptor_t *descriptor = task_get_descriptor((int)fd);
    if (descriptor == 0)
        return -1;

    if (descriptor->kind == FD_KIND_KEYBOARD)
    {
        /* stdin is nonblocking for now: no queued key means try again later. */
        int character = keyboard_getchar();
        if (character < 0)
            return 0;
        ((u8int*)buffer_address)[0] = (u8int)character;
        return 1;
    }
    if (descriptor->kind != FD_KIND_NODE || descriptor->node == 0)
        return -1;

    u32int bytes = read_fs(descriptor->node, descriptor->offset, count,
                           (u8int*)buffer_address);
    descriptor->offset += bytes;
    return (int)bytes;
}

static int service_close(u32int fd,u32int b,u32int c,u32int d,u32int e)
{
    (void)b;(void)c;(void)d;(void)e;
    return task_close_descriptor((int)fd);
}

static int service_fd_write(u32int fd,u32int buffer_address,u32int count,u32int d,u32int e)
{
    (void)d;(void)e;
    /* POSIX-style zero-length I/O succeeds without dereferencing the buffer. */
    if (count == 0)
        return 0;
    if (buffer_address == 0)
        return -1;

    file_descriptor_t *descriptor = task_get_descriptor((int)fd);
    if (descriptor == 0)
        return -1;

    if (descriptor->kind == FD_KIND_CONSOLE)
    {
        const u8int *buffer = (const u8int*)buffer_address;
        for (u32int i = 0; i < count; i++)
            monitor_put((char)buffer[i]);
        return (int)count;
    }

    /* stdin is not writable; read-only initrd descriptors also reach -1 here. */
    if (descriptor->kind != FD_KIND_NODE || descriptor->node == 0 ||
        descriptor->node->write == 0 || descriptor->flags == OPEN_READ_ONLY)
        return -1;

    u32int bytes = write_fs(descriptor->node, descriptor->offset, count,
                            (u8int*)buffer_address);
    descriptor->offset += bytes;
    return (int)bytes;
}

static int service_chdir(u32int path_address,u32int b,u32int c,u32int d,u32int e)
{
    (void)b;(void)c;(void)d;(void)e;
    if (path_address == 0 || fs_root == 0)
        return -1;
    fs_node_t *directory = resolve_path_fs(fs_root,
                                            task_get_working_directory(),
                                            (const char*)path_address);
    if (directory == 0 || (directory->flags & 0x7) != FS_DIRECTORY)
        return -1;
    task_set_working_directory(directory);
    return 0;
}

static int service_getcwd(u32int buffer_address,u32int capacity,u32int c,u32int d,u32int e)
{
    (void)c;(void)d;(void)e;
    if (buffer_address == 0)
        return -1;
    return get_path_fs(fs_root, task_get_working_directory(),
                       (char*)buffer_address, capacity);
}

static int service_mkdir(u32int path_address,u32int b,u32int c,u32int d,u32int e)
{
    (void)b;(void)c;(void)d;(void)e;
    if (path_address == 0 || fs_root == 0)
        return -1;
    return mkdir_path_fs(fs_root, task_get_working_directory(),
                         (const char*)path_address);
}

static int service_sbrk(u32int increment,u32int b,u32int c,u32int d,u32int e)
{
    (void)b;(void)c;(void)d;(void)e;
    /* The register carries the same 32 bits for positive and negative values. */
    return (int)task_sbrk((s32int)increment);
}

static syscall_service_t services[SYSCALL_COUNT] = {
    service_write, service_putchar, service_clear, service_getchar,
    service_readdir, service_readfile, service_getpid, service_ticks,
    service_memory, service_hex, service_dec,
    service_open, service_fd_read, service_close, service_fd_write,
    service_chdir, service_getcwd, service_mkdir, service_sbrk,
    0 /* execve needs the complete saved interrupt frame; handled below. */
};

static void syscall_handler(registers_t *regs)
{
    if (regs->eax >= SYSCALL_COUNT) { regs->eax = (u32int)-1; return; }

    /*
     * Ordinary calls only need five scalar registers.  execve is different:
     * success must change the EIP and user ESP that IRET will restore, so it
     * receives the complete interrupt frame instead of using services[].
     */
    if (regs->eax == SYSCALL_EXECVE)
    {
        int result = process_execve((const char*)regs->ebx,
                                    (char *const*)regs->ecx,
                                    (char *const*)regs->edx, regs);
        if (result < 0)
            regs->eax = (u32int)-1;
        return;
    }

    ASSERT(services[regs->eax] != 0);
    regs->eax = (u32int)services[regs->eax](regs->ebx, regs->ecx, regs->edx,
                                            regs->esi, regs->edi);
}

void initialise_syscalls(void) { register_interrupt_handler(0x80, syscall_handler); }

#define WRAP0(name, number) \
int syscall_##name(void) { int r; asm volatile("int $0x80" : "=a"(r) : "0"(number) : "memory"); return r; }
#define WRAP1(name, number, type1) \
int syscall_##name(type1 a) { int r; asm volatile("int $0x80" : "=a"(r) : "0"(number), "b"((u32int)a) : "memory"); return r; }
#define WRAP2(name, number, type1, type2) \
int syscall_##name(type1 a,type2 b) { int r; asm volatile("int $0x80" : "=a"(r) : "0"(number), "b"((u32int)a), "c"((u32int)b) : "memory"); return r; }
#define WRAP3(name, number, type1, type2, type3) \
int syscall_##name(type1 a,type2 b,type3 c) { int r; asm volatile("int $0x80" : "=a"(r) : "0"(number), "b"((u32int)a), "c"((u32int)b), "d"((u32int)c) : "memory"); return r; }

WRAP1(write, SYSCALL_WRITE, const char*)
WRAP1(putchar, SYSCALL_PUTCHAR, char)
WRAP0(clear, SYSCALL_CLEAR)
WRAP0(getchar, SYSCALL_GETCHAR)
WRAP3(readdir, SYSCALL_READDIR, u32int, char*, u32int)
WRAP3(readfile, SYSCALL_READFILE, const char*, char*, u32int)
WRAP0(getpid, SYSCALL_GETPID)
WRAP1(write_hex, SYSCALL_WRITE_HEX, u32int)
WRAP1(write_dec, SYSCALL_WRITE_DEC, u32int)
WRAP2(open, SYSCALL_OPEN, const char*, u32int)
WRAP3(fd_read, SYSCALL_FD_READ, int, void*, u32int)
WRAP1(close, SYSCALL_CLOSE, int)
WRAP3(fd_write, SYSCALL_FD_WRITE, int, const void*, u32int)
WRAP1(chdir, SYSCALL_CHDIR, const char*)
WRAP2(getcwd, SYSCALL_GETCWD, char*, u32int)
WRAP1(mkdir, SYSCALL_MKDIR, const char*)

void *syscall_sbrk(s32int increment)
{
    u32int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(SYSCALL_SBRK), "b"((u32int)increment)
                 : "memory");
    return (void*)result;
}

int syscall_execve(const char *path, char *const argv[], char *const envp[])
{
    int result;
    asm volatile("int $0x80"
                 : "=a"(result)
                 : "0"(SYSCALL_EXECVE), "b"((u32int)path),
                   "c"((u32int)argv), "d"((u32int)envp)
                 : "memory");
    return result;
}

u32int syscall_ticks(void)
{ int r; asm volatile("int $0x80" : "=a"(r) : "0"(SYSCALL_TICKS) : "memory"); return (u32int)r; }
u32int syscall_memory_kib(void)
{ int r; asm volatile("int $0x80" : "=a"(r) : "0"(SYSCALL_MEMORY_KIB) : "memory"); return (u32int)r; }
