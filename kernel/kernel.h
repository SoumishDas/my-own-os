#ifndef KERNEL_H
#define KERNEL_H

#include"../cpu/timer.h"
#include "../drivers/keyboard.h"
#include "../drivers/screen.h"
#include <stdint.h>
#include "../libc/string.h"




void irq_install();
void user_input(char *input);
void ASSERT(_Bool b);

#endif
