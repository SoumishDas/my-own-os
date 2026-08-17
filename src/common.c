/* common.c -- The kernel's miniature replacement for libc.
 * Freestanding code gets no operating-system C library, so this file supplies
 * basic memory/string operations plus x86 hardware port-I/O wrappers. */

#include "common.h"
#include "monitor.h"

/* Port I/O uses x86's separate 16-bit device-address space. */
void outb(u16int port, u8int value)
{
    asm volatile ("outb %1, %0" : : "dN" (port), "a" (value));
}

u8int inb(u16int port)
{
    /* volatile ensures this real hardware access cannot be optimized away. */
    u8int ret;
    asm volatile("inb %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

u16int inw(u16int port)
{
    /* This is the 16-bit counterpart to the byte-sized inb operation. */
    u16int ret;
    asm volatile ("inw %1, %0" : "=a" (ret) : "dN" (port));
    return ret;
}

// Copy len bytes from src to dest.
void *memcpy(void *dest, const void *src, u32int len)
{
    const u8int *sp = (const u8int *)src;
    u8int *dp = (u8int *)dest;
    for(; len != 0; len--) *dp++ = *sp++;
    return dest;
}

// Write len copies of val into dest.
void *memset(void *dest, u8int val, u32int len)
{
    u8int *temp = (u8int *)dest;
    for ( ; len != 0; len--) *temp++ = val;
    return dest;
}

// Compare two strings. Should return -1 if 
// str1 < str2, 0 if they are equal or 1 otherwise.
int strcmp(const char *str1, const char *str2)
{
      int i = 0;
      int failed = 0;
      while(str1[i] != '\0' && str2[i] != '\0')
      {
          if(str1[i] != str2[i])
          {
              failed = 1;
              break;
          }
          i++;
      }
      // why did the loop exit?
      if( (str1[i] == '\0' && str2[i] != '\0') || (str1[i] != '\0' && str2[i] == '\0') )
          failed = 1;
  
      return failed;
}

int strncmp(const char *str1, const char *str2, u32int count)
{
    u32int i;
    for (i = 0; i < count; i++)
    {
        if (str1[i] != str2[i]) return 1;
        if (str1[i] == '\0') return 0;
    }
    return 0;
}

// Copy the NULL-terminated string src into dest, and
// return dest.
char *strcpy(char *dest, const char *src)
{
    char *start = dest;
    while ((*dest++ = *src++) != '\0')
        ;
    return start;
}

// Concatenate the NULL-terminated string src onto
// the end of dest, and return dest.
char *strcat(char *dest, const char *src)
{
    char *start = dest;
    while (*dest != 0)
        dest++;

    while ((*dest++ = *src++) != '\0')
        ;
    return start;
}

int strlen(const char *src)
{
    int i = 0;
    while (*src++)
        i++;
    return i;
}

extern void panic(const char *message, const char *file, u32int line)
{
    // We encountered a massive problem and have to stop.
    asm volatile("cli"); // Disable interrupts.

    monitor_write("PANIC(");
    monitor_write(message);
    monitor_write(") at ");
    monitor_write(file);
    monitor_write(":");
    monitor_write_dec(line);
    monitor_write("\n");
    // Halt by going into an infinite loop.
    for(;;);
}

extern void panic_assert(const char *file, u32int line, const char *desc)
{
    // An assertion failed, and we have to panic.
    asm volatile("cli"); // Disable interrupts.

    monitor_write("ASSERTION-FAILED(");
    monitor_write(desc);
    monitor_write(") at ");
    monitor_write(file);
    monitor_write(":");
    monitor_write_dec(line);
    monitor_write("\n");
    // Halt by going into an infinite loop.
    for(;;);
}
