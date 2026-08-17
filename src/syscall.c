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
    if (!fs_root || !name_address || !capacity) return -1;
    struct dirent *entry = readdir_fs(fs_root, index);
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
    fs_node_t *node = finddir_fs(fs_root, (char*)name_address);
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

static syscall_service_t services[SYSCALL_COUNT] = {
    service_write, service_putchar, service_clear, service_getchar,
    service_readdir, service_readfile, service_getpid, service_ticks,
    service_memory, service_hex, service_dec
};

static void syscall_handler(registers_t *regs)
{
    if (regs->eax >= SYSCALL_COUNT) { regs->eax = (u32int)-1; return; }
    regs->eax = (u32int)services[regs->eax](regs->ebx, regs->ecx, regs->edx,
                                            regs->esi, regs->edi);
}

void initialise_syscalls(void) { register_interrupt_handler(0x80, syscall_handler); }

#define WRAP0(name, number) \
int syscall_##name(void) { int r; asm volatile("int $0x80" : "=a"(r) : "0"(number) : "memory"); return r; }
#define WRAP1(name, number, type1) \
int syscall_##name(type1 a) { int r; asm volatile("int $0x80" : "=a"(r) : "0"(number), "b"((u32int)a) : "memory"); return r; }
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

u32int syscall_ticks(void)
{ int r; asm volatile("int $0x80" : "=a"(r) : "0"(SYSCALL_TICKS) : "memory"); return (u32int)r; }
u32int syscall_memory_kib(void)
{ int r; asm volatile("int $0x80" : "=a"(r) : "0"(SYSCALL_MEMORY_KIB) : "memory"); return (u32int)r; }
