#ifndef TASK_H
#define TASK_H

#include "terminal.h"

// This structure defines a 'task' - a process.
typedef struct task
{
   int id;                // Process ID.
   uint32_t esp, ebp;       // Stack and base pointers.
   uint32_t eip;            // Instruction pointer.
   page_directory_t *page_directory; // Page directory.
   struct task *next;     // The next task in a linked list.
} task_t;

// Initialises the tasking system.
void initialise_tasking();

// Called by the timer hook, this changes the running process.
void switch_task();

// Forks the current process, spawning a new one with a different
// memory space.
int fork();

// Causes the current process' stack to be forcibly moved to a new location.
void move_stack(void *new_stack_start, uint32_t size);

// Returns the pid of the current process.
int getpid();

// Exits usermode
void _exit_usermode();

// Switches to user mode
void switch_to_user_mode();
#endif