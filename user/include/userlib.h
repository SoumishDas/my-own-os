/* Tiny printing helpers shared by demonstration programs before Newlib. */
#ifndef USERLIB_H
#define USERLIB_H

#include "syscall.h"
#include "signal.h"

uint32_t string_length(const char *text);
int strlen(const char *text);
int strcmp(const char *left, const char *right);
int strncmp(const char *left, const char *right, uint32_t count);
int atoi(const char *text);
void print(const char *text);
void print_unsigned(uint32_t value);
void print_pid_line(const char *program_name);

#endif
