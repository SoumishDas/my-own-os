#include"terminal.h"

void execute_command(char *input){
    char *wordlist[5];
    int wordcount = strsplitwords(input,wordlist);
    if (strcmp(wordlist[0], "END") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    } else if (strcmp(wordlist[0], "MEM") == 0){
        
        
        
        
        unsigned int a = ascii_to_int(wordlist[1]);
        kprint_int(a);
        kprint_hex(kmalloc(a));
    
    } else if (strcmp(wordlist[0], "FREE") == 0){
        
        kprint("free");

    }
    kprint("You said: ");
    for (int i=0;i<wordcount;i++){
        kprint(wordlist[i]);
        kprint("\n");
    }
    
    kprint("\n> ");
}