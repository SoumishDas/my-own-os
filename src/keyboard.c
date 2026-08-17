/*
 * keyboard.c -- IRQ1 PS/2 keyboard driver with a single-producer/single-consumer
 * ring buffer.
 *
 * The keyboard controller raises IRQ1 and places a set-1 scancode at I/O port
 * 0x60.  Key-press codes are below 0x80; key-release codes set bit 7.  This
 * driver handles the common US keyboard keys needed by a shell, plus Shift,
 * Backspace and Enter.  Extended keys and international layouts are future
 * work rather than being silently misinterpreted.
 */
#include "keyboard.h"
#include "isr.h"

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_BUFFER_SIZE 128

static volatile char input_buffer[KEYBOARD_BUFFER_SIZE];
static volatile u32int read_index;
static volatile u32int write_index;
static u8int shift_pressed;

/* Set-1 make-code maps for an ordinary US keyboard. Zero means unsupported. */
static const char normal_map[128] = {
    0,  0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b','\t',
    'q','w','e','r','t','y','u','i','o','p','[',']','\n',0,  'a','s',
    'd','f','g','h','j','k','l',';','\'', '`',0, '\\','z','x','c','v',
    'b','n','m',',','.','/',0,  '*',0,  ' ',0
};

static const char shifted_map[128] = {
    0,  0, '!','@','#','$','%','^','&','*','(',')','_','+', '\b','\t',
    'Q','W','E','R','T','Y','U','I','O','P','{','}','\n',0,  'A','S',
    'D','F','G','H','J','K','L',':','"', '~',0,  '|','Z','X','C','V',
    'B','N','M','<','>','?',0,  '*',0,  ' ',0
};

static void keyboard_interrupt(registers_t *regs)
{
    (void)regs;
    u8int scancode = inb(KEYBOARD_DATA_PORT);

    /* Left and right Shift have distinct make codes and release codes. */
    if (scancode == 0x2A || scancode == 0x36)
    {
        shift_pressed = 1;
        return;
    }
    if (scancode == 0xAA || scancode == 0xB6)
    {
        shift_pressed = 0;
        return;
    }

    /* Ignore releases and keys not represented by the compact ASCII maps. */
    if ((scancode & 0x80) != 0 || scancode >= 128)
        return;

    char character = shift_pressed ? shifted_map[scancode] : normal_map[scancode];
    if (character == 0)
        return;

    /* Leave one slot empty so read==write always has the unique meaning empty. */
    u32int next = (write_index + 1) % KEYBOARD_BUFFER_SIZE;
    if (next == read_index)
        return; /* Full buffer: drop the newest key rather than overwrite input. */

    input_buffer[write_index] = character;
    write_index = next;
}

void initialise_keyboard(void)
{
    read_index = 0;
    write_index = 0;
    shift_pressed = 0;
    register_interrupt_handler(IRQ1, keyboard_interrupt);
}

int keyboard_getchar(void)
{
    if (read_index == write_index)
        return -1;

    char character = input_buffer[read_index];
    read_index = (read_index + 1) % KEYBOARD_BUFFER_SIZE;
    return (u8int)character;
}
