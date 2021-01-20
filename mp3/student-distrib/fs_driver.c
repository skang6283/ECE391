#include "fs_driver.h"
#include "syscall.h"


/* file_sys_init 
 * Inputs: None
 * Return Value: None
 * Function: Initialize the file system pointers to appropriate addresses and initialize the file descriptor array */
void file_system_init() {
  
  // Initialize the file system pointers
  boot_block_ptr = (boot_block_t*)(FILE_SYS_BASE_ADDR);
  uint32_t num_inodes = boot_block_ptr->num_inodes;
  dentry_ptr = boot_block_ptr->dentry;
  inode_ptr = (inode_t* )(boot_block_ptr + 1); 
  data_block_ptr = (uint8_t*)(inode_ptr + num_inodes); 


}

/* read_dentry_by_name
 * Inputs: fname - file name to search
 *         dentry - dentry object to copy data into
 * Return Value: 0 if success, -1 otherwise.
 * Function: Find the file with name given as the input in the file system. 
 *           Copy the file name, file type and inode number into the dentry object given as input.
 */
int32_t read_dentry_by_name (const uint8_t* fname, dentry_t* dentry){
  int fname_len = strlen((int8_t*) fname);
  int dentry_name_len;
  // Iterate through the boot block to find file name "fname" parameter
  int i;
  for(i = 0; i <BOOT_BLOCK_SIZE; i++) {
    dentry_name_len = strlen((int8_t*)dentry_ptr[i].fname);
    if(dentry_name_len > MAX_FILENAME){
      dentry_name_len = MAX_FILENAME;
    } 
    if(fname_len == dentry_name_len) {
      if( !(strncmp((int8_t*) fname, (int8_t*)dentry_ptr[i].fname, MAX_FILENAME))) {
        // If file with given argument "fname" is found, copy everything into dentry object
        read_dentry_by_index(i, dentry);
        // Success. Return 0 and leave function.
        return 0;
      }
    }
  }
  // Failed to find file with given argument in "fname".
  return -1;
}

/* read_dentry_by_index
 * Inputs: index - index of the dentry to copy from
 *         dentry - dentry object to copy data into
 * Return Value: 0 is success. -1 otherwise.
 * Function: Find the file with index given as the input in the file system. 
 *           Copy the file name, file type and inode number into the dentry object given as input.
 */
int32_t read_dentry_by_index (uint8_t idx, dentry_t* dentry){
  // Make sure argument "index" is within the boot block
  if(idx < BOOT_BLOCK_SIZE - 1) {
    // Copy everything into the given "dentry"
    strncpy((int8_t*)dentry->fname, (int8_t*)dentry_ptr[idx].fname,MAX_FILENAME);
    dentry->ftype = dentry_ptr[idx].ftype;
    dentry->inode_num = dentry_ptr[idx].inode_num;
    // Success. Return 0 and leave function
    return 0;
  }
 // Argument given as "index" is outside the boot block. Failed. 
  return -1;
}

/* read_data
 * Inputs:  inode - inode number that points to inode block
 *          offset - offset from beginnging to start reading count  
 *          buf - buf to copy data into
 *          length - how much data to read 
 * Return Value: 0 is success. -1 otherwise.
 * Function: read data from file given inode number.
 */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length){
    int i;
    inode_t* inode_block_ptr = (inode_t*)(inode_ptr+inode); /* pointer to current inode block*/
    uint32_t* block_ptr;                                    /* pointer to data block*/
    uint32_t block_num;                                     /* data block number */
    uint32_t bytes_read = 0;                                
    uint32_t file_length = inode_block_ptr->length;           
    uint32_t N = boot_block_ptr->num_inodes;
    uint32_t D = boot_block_ptr->num_data_blocks;
    uint32_t data_block_index = offset / BLOCK_SIZE;        /* beginning number of data block */
    uint32_t byte_index = offset % BLOCK_SIZE;              /* beginning index within data block */

    if(offset >= file_length){                
      return 0; 
    }
    if(inode >= N || inode < 0){        
      return -1;
    }

    /* go though each byte and copy data into buf */
    for(i=0 ; i<length; i++, byte_index++, bytes_read++,buf++){

      if((i+offset) >= file_length){
        return bytes_read;
      }

      /* set byte index and data block index accordingly*/
      if(byte_index >= BLOCK_SIZE){
        byte_index = 0;
        data_block_index++;
      }
      /* based on byte index and data block index, set pointer to data block */
      block_num = inode_block_ptr->data_blocks_num[data_block_index];
      if(block_num >= D){
        return -1;
      }
      block_ptr = (uint32_t*)(data_block_ptr + (BLOCK_SIZE)*block_num + byte_index);

      memcpy(buf, block_ptr,1);
    }

    return bytes_read;
}

/* file_open
 * Inputs: None
 * Return Value: 0 if success. -1 otherwise 
 * Function: Find an open spot in file descriptor array for the file and initialize the file descriptor for the file
 */
int32_t file_open(const uint8_t* fname){

  return 0;
}

/* file_close
 * Inputs: None
 * Return Value: 0 is success. Undefined behavior otherwise
 * Function: Re-Initialize the free/busy flag of the file descriptor array entry.
 */
int32_t file_close(int32_t fd){
  //fd_array[fd].flag = FD_FREE;
  return 0;

}


/* file_read
 * Inputs: fd - index of the file to read in the file descriptor array
 *         buf - buffer to which read the file into
 *         nbytes - number of bytes to read into the buffer
 * Return Value: number of bytes read if success. -1 otherwise
 * Function: Initialize given buffer. 
 *           Then read number of bytes given into buf from file at given file descriptor 
 *           (utilizing read_data function)
 */
int32_t file_read(int32_t fd, void* buf, int32_t nbytes){
  if(buf == NULL) {
    return -1;
  } 

  pcb_t* cur_pcb_ptr = get_cur_pcb();
  uint32_t num_bytes = read_data(cur_pcb_ptr->fd_array[fd].inode, cur_pcb_ptr->fd_array[fd].file_pos, buf, nbytes);
  if(num_bytes == -1) {
    return -1;
  } else {
    cur_pcb_ptr->fd_array[fd].file_pos += num_bytes; /*need to include pcb*/
  }
  return num_bytes;
}

/* file_write
 * Inputs: None
 * Return Value: -1
 * Function: Returns -1 since we have a read only filesystem
 */
int32_t file_write(int32_t fd, const void* buf, int32_t nbytes){
  return -1;
}

/* dir_open
 * Inputs: fname - not used 
 * Return Value: 0 
 * Function: initializes a counter to keep track of dentry number  
 */
int32_t dir_open(const uint8_t* fname){
  //counter = 0;
  return 0;
}

/* dir_close
 * Inputs: None
 * Return Value: 0 
 * Function: does nothing
 */
int32_t dir_close(int32_t fd){
  return 0;
}

/* dir_write
 * Inputs: none
 * Return Value: -1 
 * Function: does nothing
 */
int32_t dir_write(int32_t fd, const void* buf, int32_t nbytes) {
  return -1;
}

/* dir_read
 * Inputs: none
 * Return Value: 0 
 * Function: read files filename by filename, including “.”
 */
int32_t dir_read(int32_t fd, const void* buf, int32_t nbytes){
  dentry_t dentry;   
  pcb_t* cur_pcb_ptr = get_cur_pcb();
  int32_t counter = cur_pcb_ptr->fd_array[fd].file_pos;

  if( read_dentry_by_index(counter, &dentry) == -1){
    return 0;
  }
  strncpy((int8_t*)buf, (int8_t*)&(dentry.fname), MAX_FILENAME);
  int32_t bytes_read = strlen((int8_t*)&(dentry.fname));

  counter ++;  
  cur_pcb_ptr->fd_array[fd].file_pos = counter;


  if(bytes_read > MAX_FILENAME){
    return MAX_FILENAME;
  }
  return bytes_read;
}




