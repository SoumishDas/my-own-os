#include"terminal.h"




void execute_command(char *input){
    char *wordlist[5];
    int wordcount = strsplitwords(input,wordlist);

    for (int i = 0; i < wordcount; i++)
    {
        kprint(wordlist[i]);
    }
    

    if (strcmp(wordlist[0], "END") == 0) {
        kprint("Stopping the CPU. Bye!\n");
        asm volatile("hlt");
    } else if (strcmp(wordlist[0], "MEM") == 0){
        
        unsigned int mem_size = ascii_to_int(wordlist[1]);
        kprint("#\n");
        void* add = kmalloc(mem_size);
        kprint_hex(add);
        kfree(add);
        kprint("####\n");
        void* add_2 = kmalloc(mem_size);
        kprint_hex(add);
        if (add == add_2){
            kprint("yes");
        
        }else{
            kprint("no");
        }
        kprint("######\n");

        // unsigned int a = ascii_to_int(wordlist[1]);
        // //kprint_int(a);
        // kprint("\n");
        // add = kmalloc(a);
        // for(int i =0;i<a;i++){
        //     add[i] = 1;
        // }
        // kprint_hex(add);
        // kfree(add);
        // kprint("\n");
        // add = kmalloc(a);
        // kprint_hex(add);
        // kfree(add);
        // kprint("\n");
    
    } else if (strcmp(wordlist[0], "FREE") == 0){
        
        // void* a = ascii_to_int(wordlist[1]);
        // kfree(add);
        // kprint("free");

    }

    
    
    kprint("\n> ");

    for (int i = 0; i < wordcount; i++)
    {
        kfree(wordlist[i]);
    }
    
}