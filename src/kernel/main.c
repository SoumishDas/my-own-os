#include "../drivers/multiboot.h"
 
/* Check if the compiler thinks you are targeting the wrong operating system. */
#if defined(__linux__)
#error "You are not using a cross-compiler, you will most certainly run into trouble"
#endif
 
/* This will only work for the 32-bit ix86 targets. */
#if !defined(__i386__)
#error "This needs to be compiled with a ix86-elf compiler"
#endif


void init_descriptors(){
    init_GDT();
    // Access a memory location in the data segment
    isr_install();
    //asm volatile("int $0x01"); // Trigger software interrupt 0x01
    asm volatile("sti");
    init_keyboard();
    init_timer(100);
}
extern uint32_t _end;

uint32_t initial_esp; // New global variable

void user_code(){
    
    syscall_kprint("hey");
    syscall_exit();
}

uint32_t initrd_location;
uint32_t initrd_end;

void init_initrd(struct multiboot *mboot_ptr){
    // Find the location of our initial ramdisk.
   assert(mboot_ptr->mods_count > 0);
   initrd_location = *((uint32_t*)mboot_ptr->mods_addr);
   initrd_end = *(uint32_t*)(mboot_ptr->mods_addr+4);
   
   int initrd_offset = initrd_end-initrd_location;
   memcpy(placement_address,initrd_location,initrd_offset);
    initrd_location = placement_address;
    initrd_end = initrd_location + initrd_offset;
    placement_address += initrd_offset;
}

int kernel_main(struct multiboot *m_ptr,uint32_t initial_stack)
{   
    initial_esp = initial_stack;
    placement_address = (uint32_t)&_end ;
    
    clear_screen();
    init_descriptors();
    //init_initrd(m_ptr);
    initialise_paging();
    

    unsigned long cr0;

    // Read the value of the CR0 control register
    asm volatile("mov %%cr0, %0" : "=r"(cr0));

    // Check the PG (Paging) bit (bit 31) in CR0
    if (cr0 & (1UL << 31)) {
        kprint("Paging is enabled.\n");
    } else {
        kprint("Paging is disabled.\n");
    }

    // Initialise the initial ramdisk, and set it as the filesystem root.
//    fs_root = initialise_initrd(initrd_location);
//        // list the contents of /
//     int i = 0;
//     struct dirent *node = 0;
//     while ( (node = readdir_fs(fs_root, i)) != 0)
//     {
//     kprint("Found file ");
//     kprint(node->name);
//     fs_node_t *fsnode = finddir_fs(fs_root, node->name);

//     if ((fsnode->flags&0x7) == FS_DIRECTORY)
//         kprint("\n(directory)\n");
//     else
//     {
//         kprint("\ncontents: \"");
//         char buf[256];
//         uint32_t sz = read_fs(fsnode, 0, 256, buf);
//         int j;
//         for (j = 0; j < sz; j++){
//             char t[5];
//             t[0] = buf[j];
//             t[1] = '\00';
//             kprint(t);
//         }
//         kprint("\"\n");
//     }
//     i++;
//     }
    
    initialise_syscalls();
    uint32_t old_stack_pointer; asm volatile("mov %%esp, %0" : "=r" (old_stack_pointer));
    set_kernel_stack(old_stack_pointer+0x100);
    //syscall_kprint("HEYYYY");
    //switch_to_user_mode();
    
    
    
//      initialise_tasking();
// //        // Create a new process in a new address space which is a clone of this.
//    int ret = fork();

//    kprint("fork() returned ");
//    kprint_hex(ret);
//    kprint(", and getpid() returned ");
//    int pid = getpid();
//    kprint_hex(pid);
//    kprint("\n============================================================================\n");   
    
    
    while (1) {
        // Handle interrupts, events, and scheduling here
        // Implement your kernel's functionality
    }
    return 0;
}

