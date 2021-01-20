#ifndef _FS_DRIVER_H
#define _FS_DRIVER_H

#include "lib.h"
#include "types.h"

// Magic number defines
#define BOOT_BLOCK_SIZE 64  // boot block size in bytes (64B)
#define BLOCK_SIZE 4096     // block size in bytes (4KB)
#define MAX_NUM_FILE 8      // maximum number of open files 
#define INIT_FILE_POS 0     
#define FD_FREE 1
#define FD_BUSY 0
#define MAX_FILENAME 32
#define FD_BEGIN 2         // 0 is stdin, 1 is stdout

#define DENTRY_RESERVED_BYTES 24
#define BOOT_RESERVED_BYTES   52
#define BOOT_DENTRY_NUM       63
#define INODE_DATA_BLOCK_NUM  1023  

// typedef struct {
//   uint32_t file_op_table_ptr; // To implement in later checkpoints, when we implement wrap drivers around a unified file system call interface (like the POSIX API)
//   uint32_t inode;
//   uint32_t file_pos;
//   uint32_t flag;
// } file_descriptor_t;

typedef struct {
    uint8_t fname[MAX_FILENAME];
    uint32_t ftype;
    uint32_t inode_num;
    uint8_t reserved[DENTRY_RESERVED_BYTES];
} dentry_t;

typedef struct {
    uint32_t num_dentries;
    uint32_t num_inodes;
    uint32_t num_data_blocks;
    uint8_t reserved[BOOT_RESERVED_BYTES];
    dentry_t dentry[BOOT_DENTRY_NUM];
} boot_block_t;

typedef struct {
    int32_t length;
    int32_t data_blocks_num[INODE_DATA_BLOCK_NUM];
} inode_t;

//file_descriptor_t fd_array[MAX_NUM_FILE];

// counter to keep track of dentry number
uint32_t counter;

// Base address of file system. Declared and defined in kernel.c
extern unsigned int FILE_SYS_BASE_ADDR;

// File system data structure pointers. Needed in read_data func.
inode_t* inode_ptr;
uint8_t* data_block_ptr;
boot_block_t* boot_block_ptr;
dentry_t* dentry_ptr;

/* initializes file system data structures */
void file_system_init();

/* File open() initialize any temporary structures, return 0 */
int32_t file_open(const uint8_t* fname);

/* File close() undo what you did in the open function, return 0 */
int32_t file_close(int32_t fd);

/* File read() reads count bytes of data from file into buf */ 
/* uses read_data */
int32_t file_read(int32_t fd, void* buf, int32_t nbytes);

/* File write() should do nothing, return -1 */
int32_t file_write(int32_t fd, const void* buf, int32_t nbytes);

/* Directory open() opens a directory file (note file types), return 0*/
/* uses read_dentry_by_name */
int32_t dir_open(const uint8_t* fname);

/* Directory close() probably does nothing, return 0 */
int32_t dir_close(int32_t fd);

/* Directory write() should do nothing, return -1 */
int32_t dir_write(int32_t fd, const void* buf, int32_t nbytes);

/* Directory read() read files filename by filename, including “.”*/
int32_t dir_read();

/*Find the file with name given as the input in the file system. 
 *           Copy the file name, file type and inode number into the dentry object given as input. 
 * */
int32_t read_dentry_by_name (const uint8_t* fname, dentry_t* dentry);

/*Find the file with index given as the input in the file system. 
 *           Copy the file name, file type and inode number into the dentry object given as input. 
 * */
int32_t read_dentry_by_index (uint8_t index, dentry_t* dentry);

/*  read data from file given inode number. */
int32_t read_data(uint32_t inode, uint32_t offset, uint8_t* buf, uint32_t length);



#endif 
