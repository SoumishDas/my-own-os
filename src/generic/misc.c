#include "misc.h"

assert_failed(char *file, int line, char *statement){
    
    kprint("ASSERTION FAILED\nFile: ");
    kprint(file);
    kprint(":");
    kprint_int(line);
    kprint("\nError at:");
    kprint(statement);
}