/*
 * keyboard.h -- Minimal PS/2 keyboard input used by the interactive shell.
 *
 * The interrupt handler translates set-1 scancodes into ASCII and appends them
 * to a small ring buffer.  User mode retrieves characters through a syscall;
 * it never performs privileged port I/O directly.
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "common.h"

void initialise_keyboard(void);

/* Return the next buffered ASCII byte, or -1 when no key is waiting. */
int keyboard_getchar(void);

#endif
