/* exec.h -- Replace the current ring-3 program with a static ELF32 image. */
#ifndef EXEC_H
#define EXEC_H

#include "common.h"
#include "isr.h"

/*
 * Copy path/argv from the old userspace image, load a fresh image, and rewrite
 * the syscall return frame.  Success does not return to the old program: IRET
 * enters the new ELF entry point.  Failure returns -1 with the old image intact.
 * envp is accepted for ABI shape but only an empty environment is supported yet.
 */
int process_execve(const char *path, char *const argv[],
                   char *const envp[], registers_t *return_frame);

#endif
