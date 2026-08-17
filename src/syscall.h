/*
 * syscall.h -- Ring-3 API for entering the kernel through interrupt 0x80.
 *
 * ABI: EAX contains the syscall number; EBX, ECX, EDX, ESI and EDI carry up
 * to five arguments; EAX carries the signed result.
 */
#ifndef SYSCALL_H
#define SYSCALL_H

#include "common.h"

enum syscall_number {
    SYSCALL_WRITE = 0, SYSCALL_PUTCHAR, SYSCALL_CLEAR, SYSCALL_GETCHAR,
    SYSCALL_READDIR, SYSCALL_READFILE, SYSCALL_GETPID, SYSCALL_TICKS,
    SYSCALL_MEMORY_KIB, SYSCALL_WRITE_HEX, SYSCALL_WRITE_DEC, SYSCALL_COUNT
};

void initialise_syscalls(void);
int syscall_write(const char *text);
int syscall_putchar(char character);
int syscall_clear(void);
int syscall_getchar(void);
int syscall_readdir(u32int index, char *name, u32int capacity);
int syscall_readfile(const char *name, char *buffer, u32int capacity);
int syscall_getpid(void);
u32int syscall_ticks(void);
u32int syscall_memory_kib(void);
int syscall_write_hex(u32int value);
int syscall_write_dec(u32int value);

#endif
