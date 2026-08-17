/*
 * shell.c -- Standalone ELF32 command interpreter loaded as /bin/shell.
 *
 * The kernel contains no command parser. PID 1 forks PID 2, PID 2 execs this
 * image, and every operation below crosses int 0x80 through the tiny userspace
 * runtime. Child programs are launched with fork -> execve -> waitpid.
 */
#include "userlib.h"

#define SHELL_LINE_SIZE 128
#define SHELL_FILE_BUFFER_SIZE 256

/*
 * These are the first tiny libc-like output helpers.  String length belongs in
 * userspace; the kernel receives an explicit byte count and therefore also
 * supports non-string/binary buffers through write().
 */
static int shell_write_bytes(const void *buffer, uint32_t count)
{
    return write(1, buffer, count);
}

static int shell_write(const char *text)
{
    return shell_write_bytes(text, (uint32_t)strlen(text));
}

static int shell_putchar(char character)
{
    return shell_write_bytes(&character, 1);
}

static void print_prompt(void)
{
    char path[128];
    if (getcwd(path, sizeof(path)) < 0)
        shell_write("?> ");
    else
    {
        shell_write(path);
        shell_write("> ");
    }
}

static void print_line(const char *text)
{
    shell_write(text);
    shell_putchar('\n');
}

static const char *arguments_after(const char *line, const char *command)
{
    uint32_t i = (uint32_t)strlen(command);
    if (line[i] == ' ') return &line[i+1];
    return "";
}

static void command_help(void)
{
    print_line("Commands:");
    print_line("  help          show this command list");
    print_line("  clear         clear the VGA text screen");
    print_line("  echo TEXT     print TEXT");
    print_line("  ls            list the current directory");
    print_line("  cd PATH       change this task's working directory");
    print_line("  pwd           print the current working directory");
    print_line("  mkdir PATH    create a volatile directory (lost on reboot)");
    print_line("  cat FILE      print an initrd file using open/read/close");
    print_line("  pid           show the current task ID");
    print_line("  uptime        show timer ticks and approximate seconds");
    print_line("  mem           show RAM tracked by the frame allocator");
    print_line("  heaptest      grow this process's user heap across two pages");
    print_line("  exec PATH ... replace this shell with a static ELF32 program");
    print_line("  run PATH ...  fork, execute, wait, then return to this shell");
    print_line("  kill PID SIG  send a signal number to another process");
    print_line("  status        summarize this shell's current limitations");
    print_line("  about         identify the operating-system project");
}

/*
 * Exercise sbrk without requiring malloc yet.  5000 bytes necessarily crosses
 * a 4096-byte boundary, so success proves that the kernel mapped at least two
 * private user pages.  Writing the first and last bytes proves both mappings
 * are writable from ring 3.
 */
static void command_heaptest(void)
{
    unsigned char *memory = (unsigned char*)sbrk(5000);
    if (memory == (void*)-1)
    {
        print_line("heaptest: sbrk failed");
        return;
    }

    memory[0] = 0x5A;
    memory[4999] = 0xA5;
    if (memory[0] != 0x5A || memory[4999] != 0xA5)
    {
        print_line("heaptest: mapped memory did not retain writes");
        return;
    }

    /* monitor_write_hex(), used by the syscall, already prints the 0x prefix. */
    shell_write("heaptest: 5000 writable bytes at ");
    write_hex((uint32_t)memory);
    shell_write("; new break ");
    write_hex((uint32_t)sbrk(0));
    shell_putchar('\n');
}

static int split_words(char *text, char **argv, uint32_t maximum_arguments)
{
    uint32_t argc = 0;
    char *cursor = text;
    for (;;)
    {
        while (*cursor == ' ')
            cursor++;
        if (*cursor == '\0')
            break;
        if (argc == maximum_arguments)
            return -1;
        argv[argc++] = cursor;
        while (*cursor != '\0' && *cursor != ' ')
            cursor++;
        if (*cursor != '\0')
            *cursor++ = '\0';
    }
    argv[argc] = 0;
    return (int)argc;
}

static void command_exec(char *arguments)
{
    /* execve copies these old-stack strings before discarding the shell image. */
    char *argv[9];
    int argc = split_words(arguments, argv, 8);

    if (argc == 0)
    {
        print_line("usage: exec PATH [ARG ...]");
        return;
    }
    if (argc < 0)
    {
        print_line("exec: at most 8 arguments are currently supported");
        return;
    }

    /* Success never returns: IRET enters the new ELF program's _start. */
    if (execve(argv[0], argv, 0) < 0)
        print_line("exec: file missing, invalid, or unsupported ELF32");
}

static void command_run(char *arguments)
{
    char *argv[9];
    int argc = split_words(arguments, argv, 8);
    if (argc == 0)
    {
        print_line("usage: run PATH [ARG ...]");
        return;
    }
    if (argc < 0)
    {
        print_line("run: at most 8 arguments are currently supported");
        return;
    }

    int child_pid = fork();
    if (child_pid < 0)
    {
        print_line("run: fork failed");
        return;
    }
    if (child_pid == 0)
    {
        if (execve(argv[0], argv, 0) < 0)
        {
            print_line("run: child could not load executable");
            _exit(127);
        }
    }

    shell_write("run: started child PID ");
    write_dec((uint32_t)child_pid);
    shell_putchar('\n');

    int status = 0;
    int waited;
    while ((waited = waitpid(child_pid, &status)) == 0)
        asm volatile("pause");
    if (waited < 0)
    {
        print_line("run: waitpid failed");
        return;
    }
    shell_write("run: child exited with status ");
    write_dec((uint32_t)status);
    shell_putchar('\n');
}

static void command_kill(char *arguments)
{
    char *argv[3];
    int argc = split_words(arguments, argv, 2);
    if (argc != 2)
    {
        print_line("usage: kill PID SIGNAL");
        return;
    }

    int process_id = atoi(argv[0]);
    int signal_number = atoi(argv[1]);
    if (process_id <= 0 || signal_number < 0 ||
        kill(process_id, signal_number) < 0)
    {
        print_line("kill: invalid PID/signal or process does not exist");
        return;
    }
    print_line(signal_number == 0 ? "kill: process exists" : "kill: signal queued");
}

static void command_ls(void)
{
    char name[128];
    uint32_t index = 0;
    int result;
    while ((result = readdir_name(index, name, sizeof(name))) > 0)
    {
        shell_write(name);
        shell_putchar('\n');
        index++;
    }
    if (result < 0) print_line("ls: filesystem unavailable");
}

static void command_cat(const char *name)
{
    if (*name == '\0') { print_line("usage: cat FILE"); return; }
    int fd = open(name, OPEN_READ_ONLY);
    if (fd < 0) { print_line("cat: file not found or not a regular file"); return; }

    char buffer[SHELL_FILE_BUFFER_SIZE];
    int bytes;
    char final_character = '\0';
    while ((bytes = read(fd, buffer, sizeof(buffer) - 1)) > 0)
    {
        final_character = buffer[bytes - 1];
        shell_write_bytes(buffer, (uint32_t)bytes);
    }
    close(fd);
    if (bytes < 0) { print_line("cat: read failed"); return; }
    if (final_character != '\n') shell_putchar('\n');
}

static void execute_command(char *line)
{
    if (*line == '\0') return;
    if (!strcmp(line, "help")) command_help();
    else if (!strcmp(line, "clear")) clear_screen();
    else if (!strcmp(line, "ls")) command_ls();
    else if (!strcmp(line, "pwd")) {
        char path[128];
        if (getcwd(path, sizeof(path)) < 0) print_line("pwd: unavailable");
        else print_line(path);
    }
    else if (!strcmp(line, "cd") || !strcmp(line, "cd ")) print_line("usage: cd PATH");
    else if (!strncmp(line, "cd ", 3)) {
        if (chdir(arguments_after(line, "cd")) < 0)
            print_line("cd: directory not found");
    }
    else if (!strcmp(line, "mkdir") || !strcmp(line, "mkdir "))
        print_line("usage: mkdir PATH");
    else if (!strncmp(line, "mkdir ", 6)) {
        if (mkdir(arguments_after(line, "mkdir")) < 0)
            print_line("mkdir: parent missing, name exists, or path is invalid");
    }
    else if (!strcmp(line, "pid")) { shell_write("PID: "); write_dec((uint32_t)getpid()); shell_putchar('\n'); }
    else if (!strcmp(line, "uptime")) {
        uint32_t ticks = system_ticks();
        shell_write("Ticks: "); write_dec(ticks);
        shell_write(" (~"); write_dec(ticks/50); shell_write(" seconds)\n");
    }
    else if (!strcmp(line, "mem")) {
        shell_write("Tracked contiguous RAM: ");
        write_dec(system_memory_kib()); shell_write(" KiB\n");
    }
    else if (!strcmp(line, "heaptest")) command_heaptest();
    else if (!strcmp(line, "exec") || !strcmp(line, "exec "))
        print_line("usage: exec PATH [ARG ...]");
    else if (!strncmp(line, "exec ", 5))
        command_exec(&line[5]);
    else if (!strcmp(line, "run") || !strcmp(line, "run "))
        print_line("usage: run PATH [ARG ...]");
    else if (!strncmp(line, "run ", 4))
        command_run(&line[4]);
    else if (!strcmp(line, "kill") || !strcmp(line, "kill "))
        print_line("usage: kill PID SIGNAL");
    else if (!strncmp(line, "kill ", 5))
        command_kill(&line[5]);
    else if (!strcmp(line, "status")) {
        print_line("ring 3: yes; syscalls: yes; initrd: read-only");
        print_line("external executables: yes; this shell is /bin/shell ELF32");
        print_line("scheduler: PID 0 kernel idle, PID 1 user mother, PID 2 shell");
        print_line("user heap: page-backed sbrk; malloc not added yet");
        print_line("signals: catch/ignore/default, kill, SIGCHLD, sigreturn");
    }
    else if (!strcmp(line, "about")) print_line("Soumish's experimental 32-bit x86 OS");
    else if (!strcmp(line, "echo") || !strcmp(line, "echo ")) shell_putchar('\n');
    else if (!strncmp(line, "echo ", 5)) print_line(arguments_after(line, "echo"));
    else if (!strncmp(line, "cat ", 4)) command_cat(arguments_after(line, "cat"));
    else { shell_write("unknown command: "); print_line(line); }
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    char line[SHELL_LINE_SIZE];
    uint32_t length = 0;

    print_line("Welcome to user mode. Type 'help'.");
    print_prompt();

    for (;;)
    {
        int value = getchar_nonblocking();
        if (value < 0) { asm volatile("pause"); continue; }
        char character = (char)value;

        if (character == '\n')
        {
            shell_putchar('\n');
            line[length] = '\0';
            execute_command(line);
            length = 0;
            print_prompt();
        }
        else if (character == '\b')
        {
            if (length > 0)
            {
                length--;
                shell_putchar('\b'); shell_putchar(' '); shell_putchar('\b');
            }
        }
        else if (character >= ' ' && length + 1 < SHELL_LINE_SIZE)
        {
            line[length++] = character;
            shell_putchar(character);
        }
    }
    return 0;
}
