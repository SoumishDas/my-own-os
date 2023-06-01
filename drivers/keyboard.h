#ifndef KEYBOARD_H
#define KEYBOARD_H

#include "../cpu/ports.h"
#include "../cpu/isr.h"
#include "screen.h"
#include "../libc/string.h"
#include "../libc/function.h"
#include <stdint.h>

void init_keyboard();

#endif