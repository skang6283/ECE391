#include "paging.h"

extern void enable(int directory);


void paging_init(){
  //Loop variables
  unsigned int j;
  unsigned int i;

  //Setup the first 4MB of memory with 4 kB tables
  page_directory[0].present       = 1;
  page_directory[0].read_write    = 1;
  page_directory[0].user          = 0;
  page_directory[0].write_through = 0;  //Not sure
  page_directory[0].cache_disable = 0;  //Not sure
  page_directory[0].accessed      = 0;
  page_directory[0].size          = 0;  //4 kB tables
  page_directory[0].reserved      = 0;
  //Aligned value for the table address
  page_directory[0].table_addr_31_12 = ((int)page_table)/ALIGN_4KB;

  //Sets up the rest as 4MB pages
  for(i = 1; i < MAX_SPACES; ++i){
    if(i == 1){
      //Bit setup for kernel memory.
      page_directory[i].present     = 1;
      page_directory[i].user        = 0;
      page_directory[i].table_addr_31_12 = KERNEL_ADDR/ALIGN_4KB;
    }
    else if(i == USER_INDEX){
      //Bit setup for User virtual memory
      page_directory[i].present = 1;
      page_directory[i].user    = 1;
      page_directory[i].table_addr_31_12 = USER_MEM/ALIGN_4KB;
    }
    else{
      //Bit setup for the rest of memory
      page_directory[i].present     = 0;
      page_directory[i].user        = 0;
    }
    page_directory[i].read_write    = 1;
    page_directory[i].write_through = 0;  //Not sure
    page_directory[i].cache_disable = 0;  //Not sure
    page_directory[i].accessed      = 0;
    page_directory[i].size          = 1;  //4MB memory
    page_directory[i].reserved      = 0;
  }


  //Fill in the page table for the first 4MB
  for(j = 0; j < MAX_SPACES; ++j){
    //Setup video memory page
    if(j * ALIGN_4KB == VIDMEM_ADDR){
      page_table[j].present     = 1;
    }
    //Set up for unused 4kB pages
    else{
      page_table[j].present     = 0;
    }
    page_table[j].read_write    = 1;
    page_table[j].user          = 0;
    page_table[j].write_through = 0;    //Not sure
    page_table[j].cache_disable = 0;    //Not sure
    page_table[j].accessed      = 0;
    page_table[j].dirty         = 0;
    page_table[j].reserved      = 0;
    page_table[j].global        = 0;    //Not sure
    page_table[j].page_addr_31_12 = j;
  }

for(j = 0; j < MAX_SPACES; ++j){
    //Setup video memory page
    page_table_vidmap[j].present     = 0;
    page_table_vidmap[j].read_write    = 1;
    page_table_vidmap[j].user          = 0;
    page_table_vidmap[j].write_through = 0;    //Not sure
    page_table_vidmap[j].cache_disable = 1;    //Not sure
    page_table_vidmap[j].accessed      = 0;
    page_table_vidmap[j].dirty         = 0;
    page_table_vidmap[j].reserved      = 0;
    page_table_vidmap[j].global        = 0;    //Not sure
    page_table_vidmap[j].page_addr_31_12 = j;
  }






  //Sets up the control registers for paging
  enable((int)page_directory);






}
