#ifndef MEMALLOC_H
#define MEMALLOC_H
// Get assembly functions for enabling and loading paging
extern void loadPageDirectory(unsigned int*);
extern void enablePaging();

#include"../libc/string.h"
#include "../drivers/screen.h"
#include"paging.h"
#include <stdint.h>
#include<stdbool.h>

uint32_t page_directory[1024] __attribute__((aligned(4096)));
uint32_t first_page_table[1024] __attribute__((aligned(4096)));

typedef struct __s_block{  
    struct __s_block *next;  
    bool isfree;  
    unsigned int size;  
    void *memoryAddress;  
}_SBLOCK;  
  
  
#define BLOCK_SIZE sizeof(_SBLOCK)  
#define START_ADDR 0x10000
#define END_ADDR 0x10fff



_SBLOCK* InitMemory();
_SBLOCK* GetFreeBlock(unsigned int size);

#endif