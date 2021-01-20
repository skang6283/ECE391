#ifndef PAGING_H
#define PAGING_H

#ifndef ASM

#include "types.h"

#define   MAX_SPACES    1024      //Number of tables/pages in dir
#define   ALIGN_4KB		4096			 //(2^12)
#define   VIDMEM_ADDR   0xB8000    //Video memory address in physical memory
#define   KERNEL_ADDR   0x400000    //Kernel address in physical memory

#define   PAGE_4MB      0x400000  //4 MB page size
#define   USER_MEM      0x8000000 //Page address for user stack
#define   USER_INDEX    32        //The directory index for this address.
#define   VIDEO_INDEX   34        //directory used for vidmap  

#define VM_VIDEO 0x8800000
//See wiki.osdev.org/Paging for information on directory and table entries.

//The 32 bit entries used for the directory
typedef struct __attribute__((packed)) dir_entry_desc{
    uint32_t present            : 1;
    uint32_t read_write         : 1;
    uint32_t user               : 1;
    uint32_t write_through      : 1;
    uint32_t cache_disable      : 1;
    uint32_t accessed           : 1;
    uint32_t reserved           : 1;
    uint32_t size               : 1;      //4MB or 4kB
    uint32_t ignored            : 1;
    uint8_t  avail_11_9         : 3;
    uint32_t table_addr_31_12   : 20;
}dir_entry_desc_t;

//32 bit Entries used for the tables
typedef struct __attribute__((packed)) table_entry_desc{
    uint32_t present            : 1;
    uint32_t read_write         : 1;
    uint32_t user               : 1;
    uint32_t write_through      : 1;
    uint32_t cache_disable      : 1;
    uint32_t accessed           : 1;
    uint32_t dirty              : 1;
    uint32_t reserved           : 1;
    uint32_t global             : 1;
    uint8_t  avail_11_9         : 3;
    uint32_t page_addr_31_12    : 20;
}table_entry_desc_t;

//Arrays holding the 32-bit entries for directory and table
dir_entry_desc_t page_directory[MAX_SPACES] __attribute__((aligned (ALIGN_4KB)));
table_entry_desc_t page_table[MAX_SPACES] __attribute__((aligned (ALIGN_4KB)));
table_entry_desc_t page_table_vidmap[MAX_SPACES] __attribute__((aligned (ALIGN_4KB)));


// Initializes the pages
extern void paging_init();

#endif /* ASM */
#endif /* PAGING_H */
