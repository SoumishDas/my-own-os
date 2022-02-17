#include "mem_allocator.h"



_SBLOCK* GetFreeBlock(unsigned int size){
    _SBLOCK *current = (_SBLOCK*)START_ADDR;
    _SBLOCK *nextblock ;
    

    if(current==0){  
        current = InitMemory();  
    }else{
        // Loop through all unuseable blocks
        while((current->isfree != true)||(current->size<size)){
            
            if(current->next == 0){
                kprint("hi\n");
                return 0;
            }
            else{
                kprint("Skipped Block\n");
                current = current->next;
            }
        }
        // Check if block size is equal to required size
        if (size == current->size){
            current->isfree = false;
            return current;
        }

        // Split the big block into two parts
        nextblock = current->next;

        current->size = size;
        current->isfree = false;
        current->next = (void*)current->memoryAddress+size;
        

        _SBLOCK *newblock = (void*)current->memoryAddress+size;  
    
        newblock->next = nextblock;  
        newblock->isfree = true;
        newblock->memoryAddress = (void*)newblock+BLOCK_SIZE;  
        newblock->size = ((void*)newblock->next)-((void*)newblock->memoryAddress);
        char pr[]="" ;
        
        hex_to_ascii(current->memoryAddress,pr);
        kprint(pr);
        kprint("\n");
        return current;

    }  

}

_SBLOCK* InitMemory(){

    initialise_paging();
    // _SBLOCK *block = (_SBLOCK*)START_ADDR;
    // void *memadr = (void*)START_ADDR;


    // block->next = 0;
    // block->isfree = true;
    // block->size = END_ADDR-START_ADDR;
    // block->memoryAddress = memadr+BLOCK_SIZE;
    // char* pr;
    // hex_to_ascii(block->memoryAddress,pr);
    // kprint(pr);
    // return block;
}

void PrintBlocks(){
    _SBLOCK *current = (_SBLOCK*)START_ADDR;
    char *printBuffer1 = (char*)0x100000;
    char *printBuffer = (char*)0x9fffffc;
    
    // while((strcmp(printBuffer,"")==0)){
    //     printBuffer= printBuffer+1;
    //     printBuffer[0]='A';
    //     }
    
    do
    {
        
        
        kprint("  Addr: ");
        printBuffer[0]='\0';
        hex_to_ascii(printBuffer,printBuffer);
        
        kprint(printBuffer);
        printBuffer[0]='\0';
        hex_to_ascii(current->next,printBuffer);
        kprint("   Next Header Addr:");
        kprint(printBuffer);
        printBuffer[0]='\0';
        hex_to_ascii(current->size+BLOCK_SIZE,printBuffer);
        kprint("   Size:");
        kprint(printBuffer);
        kprint("   isFree:");
        kprint(boolstring(current->isfree));
        printBuffer[0]='\0';
        hex_to_ascii((void*)current-10,printBuffer);
        kprint(printBuffer);
        current =  current->next;
        kprint("\n");
        
    }while (current->next!=0);
    

}