#include "terminal.h"

// Function prototypes for command handlers
void cmd_end(char *args[]);
void cmd_mem(char *args[]);
void cmd_free(char *args[]);
void cmd_help(char *args[]);
void cmd_clear(char *args[]);
void cmd_time(char *args[]);

// Structure to represent a command
typedef struct {
    const char *name;                 // Command name
    void (*handler)(char *args[]);    // Pointer to the command handler function
    const char *description;          // Command description
} Command;

// Define an array of commands
Command commands[] = {
    {"END", cmd_end, "Stop the CPU"},
    {"MEM", cmd_mem, "Display memory information"},
    {"FREE", cmd_free, "Free memory"},
    {"HELP", cmd_help, "Show available commands"},
    {"CLEAR", cmd_clear, "Clear the screen"},
    {"TIME", cmd_time, "Display the current date and time"},
    // Add more commands here
};

// Number of commands
#define NUM_COMMANDS (sizeof(commands) / sizeof(Command))

void execute_command(char *input) {
    char *wordlist[5];
    int wordcount = strsplitwords(input, wordlist);

    if (wordcount == 0) {
        // Handle empty input
        return;
    }

    // Search for the command in the command table
    for (int i = 0; i < NUM_COMMANDS; i++) {
        if (strcmp(wordlist[0], commands[i].name) == 0) {
            // Found the command, call its handler function
            commands[i].handler(wordlist);
            return;
        }
    }

    // Command not found
    kprint("Command not found. Type 'HELP' for a list of available commands.\n>");

    // Clean up allocated memory, if any
    for (int i = 0; i < wordcount; i++) {
        kfree(wordlist[i]);
    }
}

// Implement command handler functions here

void cmd_end(char *args[]) {
    kprint("Stopping the CPU. Bye!\n");
    asm volatile("hlt");
}

void cmd_mem(char *args[]) {
    // Implement memory command
}

void cmd_free(char *args[]) {
    // Implement free command
}

void cmd_help(char *args[]) {
    kprint("Available commands:\n");
    for (int i = 0; i < NUM_COMMANDS; i++) {
        kprint(commands[i].name);
        kprint(" - ");
        kprint(commands[i].description);
        kprint("\n");
    }
}

void cmd_clear(char *args[]) {
    // Implement clear command
    clear_screen();
}

void cmd_time(char *args[]) {
    // Implement time command
}
