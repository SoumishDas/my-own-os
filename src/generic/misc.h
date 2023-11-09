#ifndef MISC_H
#define MISC_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define assert(statement) ((statement) ? (void)0 : assert_failed(__FILE__, __LINE__, #statement))
#define ASSERT(statement) ((statement) ? (void)0 : assert_failed(__FILE__, __LINE__, #statement))

// TO BE CHANGED
#define debug_print(level, ...) kprint(__VA_ARGS__)

#define UNUSED(x) (void)(x)

#define IRQ_OFF { asm volatile ("cli"); }
#define IRQ_RES { asm volatile ("sti"); }
#define PAUSE   { asm volatile ("hlt"); }
#define IRQS_ON_AND_PAUSE { asm volatile ("sti\nhlt\ncli"); }

#define SYSCALL_VECTOR 0x80

#define STOP while (1) { PAUSE; }

extern void kprint(char *message);
extern void kprint_backspace();
extern void kprint_int(int n);

void assert_failed(char* , int, char*);
#endif