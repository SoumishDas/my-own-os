#include "userlib.h"

uint32_t string_length(const char *text)
{
    uint32_t length = 0;
    while (text[length] != '\0')
        length++;
    return length;
}

int strlen(const char *text)
{
    return (int)string_length(text);
}

int strcmp(const char *left, const char *right)
{
    uint32_t index = 0;
    while (left[index] != '\0' && right[index] != '\0')
    {
        if (left[index] != right[index])
            return 1;
        index++;
    }
    return left[index] == right[index] ? 0 : 1;
}

int strncmp(const char *left, const char *right, uint32_t count)
{
    for (uint32_t index = 0; index < count; index++)
    {
        if (left[index] != right[index])
            return 1;
        if (left[index] == '\0')
            return 0;
    }
    return 0;
}

/* Decimal conversion needed by shell commands until strtol joins libc. */
int atoi(const char *text)
{
    int sign = 1;
    int value = 0;
    if (*text == '-')
    {
        sign = -1;
        text++;
    }
    while (*text >= '0' && *text <= '9')
    {
        value = value * 10 + (*text - '0');
        text++;
    }
    return value * sign;
}

void print(const char *text)
{
    write(1, text, string_length(text));
}

void print_unsigned(uint32_t value)
{
    char digits[10];
    uint32_t count = 0;
    do
    {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value != 0);
    while (count != 0)
        write(1, &digits[--count], 1);
}

void print_pid_line(const char *program_name)
{
    print(program_name);
    print(" is running as PID ");
    print_unsigned((uint32_t)getpid());
    print(".\n");
}
