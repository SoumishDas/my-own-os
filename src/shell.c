/*
 * shell.c -- Small interactive command interpreter running at privilege ring 3.
 *
 * This is user-mode code in the CPU privilege sense, but it is still linked
 * into the kernel ELF image. It communicates through int 0x80 and deliberately
 * avoids direct port I/O or direct filesystem calls. A future exec loader will
 * replace this linked-in arrangement with independently built user binaries.
 */
#include "shell.h"
#include "syscall.h"
#include "common.h"

#define SHELL_LINE_SIZE 128
#define SHELL_FILE_BUFFER_SIZE 1024

static void print_line(const char *text)
{
    syscall_write(text);
    syscall_putchar('\n');
}

static const char *arguments_after(const char *line, const char *command)
{
    u32int i = (u32int)strlen(command);
    if (line[i] == ' ') return &line[i+1];
    return "";
}

static void command_help(void)
{
    print_line("Commands:");
    print_line("  help          show this command list");
    print_line("  clear         clear the VGA text screen");
    print_line("  echo TEXT     print TEXT");
    print_line("  ls            list the initrd root directory");
    print_line("  cat FILE      print up to 1023 bytes from an initrd file");
    print_line("  pid           show the current task ID");
    print_line("  uptime        show timer ticks and approximate seconds");
    print_line("  mem           show RAM tracked by the frame allocator");
    print_line("  status        summarize this shell's current limitations");
    print_line("  about         identify the operating-system project");
}

static void command_ls(void)
{
    char name[128];
    u32int index = 0;
    int result;
    while ((result = syscall_readdir(index, name, sizeof(name))) > 0)
    {
        syscall_write(name);
        syscall_putchar('\n');
        index++;
    }
    if (result < 0) print_line("ls: filesystem unavailable");
}

static void command_cat(const char *name)
{
    if (*name == '\0') { print_line("usage: cat FILE"); return; }
    char buffer[SHELL_FILE_BUFFER_SIZE];
    int bytes = syscall_readfile(name, buffer, SHELL_FILE_BUFFER_SIZE-1);
    if (bytes < 0) { print_line("cat: file not found or not a regular file"); return; }
    buffer[bytes] = '\0';
    syscall_write(buffer);
    if (bytes == 0 || buffer[bytes-1] != '\n') syscall_putchar('\n');
}

static void execute_command(char *line)
{
    if (*line == '\0') return;
    if (!strcmp(line, "help")) command_help();
    else if (!strcmp(line, "clear")) syscall_clear();
    else if (!strcmp(line, "ls")) command_ls();
    else if (!strcmp(line, "pid")) { syscall_write("PID: "); syscall_write_dec((u32int)syscall_getpid()); syscall_putchar('\n'); }
    else if (!strcmp(line, "uptime")) {
        u32int ticks = syscall_ticks();
        syscall_write("Ticks: "); syscall_write_dec(ticks);
        syscall_write(" (~"); syscall_write_dec(ticks/50); syscall_write(" seconds)\n");
    }
    else if (!strcmp(line, "mem")) {
        syscall_write("Tracked contiguous RAM: ");
        syscall_write_dec(syscall_memory_kib()); syscall_write(" KiB\n");
    }
    else if (!strcmp(line, "status")) {
        print_line("ring 3: yes; syscalls: yes; initrd: read-only");
        print_line("external executables: not yet; shell is linked into kernel");
        print_line("scheduler: timer-driven round robin, currently one task");
    }
    else if (!strcmp(line, "about")) print_line("Soumish's experimental 32-bit x86 OS");
    else if (!strcmp(line, "echo") || !strcmp(line, "echo ")) syscall_putchar('\n');
    else if (!strncmp(line, "echo ", 5)) print_line(arguments_after(line, "echo"));
    else if (!strncmp(line, "cat ", 4)) command_cat(arguments_after(line, "cat"));
    else { syscall_write("unknown command: "); print_line(line); }
}

void shell_run(void)
{
    char line[SHELL_LINE_SIZE];
    u32int length = 0;

    print_line("Welcome to user mode. Type 'help'.");
    syscall_write("os> ");

    for (;;)
    {
        int value = syscall_getchar();
        if (value < 0) { asm volatile("pause"); continue; }
        char character = (char)value;

        if (character == '\n')
        {
            syscall_putchar('\n');
            line[length] = '\0';
            execute_command(line);
            length = 0;
            syscall_write("os> ");
        }
        else if (character == '\b')
        {
            if (length > 0)
            {
                length--;
                syscall_putchar('\b'); syscall_putchar(' '); syscall_putchar('\b');
            }
        }
        else if (character >= ' ' && length + 1 < SHELL_LINE_SIZE)
        {
            line[length++] = character;
            syscall_putchar(character);
        }
    }
}
