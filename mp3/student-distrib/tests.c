#include "tests.h"
#include "x86_desc.h"
#include "lib.h"
#include "idt.h"
#include "rtc.h"
#include "fs_driver.h"
#include "terminal.h"

#define PASS 1
#define FAIL 0

/* format these macros as you see fit */
#define TEST_HEADER     \
    printf("[TEST %s] Running %s at %s:%d\n", __FUNCTION__, __FUNCTION__, __FILE__, __LINE__)
#define TEST_OUTPUT(name, result)    \
    printf("[TEST %s] Result = %s\n", name, (result) ? "PASS" : "FAIL");

static inline void assertion_failure(){
    /* Use exception #15 for assertions, otherwise
       reserved by Intel */
    asm volatile("int $15");
}


/* Checkpoint 1 tests */

/* IDT Test - Example
 *
 * Asserts that first 10 IDT entries are not NULL
 * Inputs: None
 * Outputs: PASS/FAIL
 * Side Effects: None
 * Coverage: Load IDT, IDT definition
 * Files: x86_desc.h/S idt.c
 */
int idt_test() {
    TEST_HEADER;

    int i;
    int result = PASS;
    for (i = 0; i < 10; ++i){
        if ((idt[i].offset_15_00 == NULL) &&
            (idt[i].offset_31_16 == NULL)){
            assertion_failure();
            result = FAIL;
        }
    }

    return result;
}

/* division_exception_test
* Cause blue screen of death
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Exception handling
* Files: idt.c
*/
int division_exception_test() {
    TEST_HEADER;
    int a = 0;
    int b = 4;
    b = b / a;
    return FAIL; // If exception BSODs, we never get here
}

/* syscall_handler_test
* Cause blue screen of death
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Exception handling
* Files: idt.c
*/
int syscall_handler_test() {
    TEST_HEADER;
    __asm__("int    $0x80");
    return FAIL; // If exception BSODs, we never get here
}

/* paging_test
* Checks that the kernel and video memory addresses can be dereferenced.
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Kernel and video memory in physical memory from paging.
* Files: paging.c
*/
int paging_test() {
    TEST_HEADER;
    //Used to test dereference locations.
    char result;
    char* pointer = (char*)0x400000;    //Kernel memory
    result = *pointer;

    pointer = (char*)0xB8000;                    //Video memory address
    result = *pointer;

    pointer = (char*)0x7FFFFF;                 //Bottom of kernel memory
    result = *pointer;

    pointer = (char*)0xB8FFF;                 //Bottom of video memory
    result = *pointer;

    return PASS; // If exception BSODs, we never get here
}

/* kernel_up_bound_test
* Check to see if the memory before the kernel causes a page fault.
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Page fault handling of location before kernel memory.
* Files: paging.c
*/
int kernel_up_bound_test() {
    TEST_HEADER;
    char result;
    char* pointer = (char*)0x3FFFFF;
    result = *pointer;
    return FAIL; // If exception BSODs, we never get here
}

/* kernel_low_bound_test
* Check to see if the memory after the kernel causes a page fault.
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Page fault handling of location after kernel memory.
* Files: paging.c
*/
int kernel_low_bound_test() {
    TEST_HEADER;
    char result;
    char* pointer = (char*)0x800000;
    result = *pointer;
    return FAIL; // If exception BSODs, we never get here
}

/* vidmem_up_bound_test
* Check to see if the memory before video memory causes a page fault.
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Page fault handling of location before video memory.
* Files: paging.c
*/
int vidmem_up_bound_test() {
    TEST_HEADER;
    char result;
    char* pointer = (char*) 0xB7FFF;
    result = *pointer;
    return FAIL; // If exception BSODs, we never get here
}

/* vidmem_low_bound_test
* Check to see if the memory after video memory causes a page fault.
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Page fault handling of location after video memory.
* Files: paging.c
*/
int vidmem_low_bound_test() {
    TEST_HEADER;
    char result;
    char* pointer = (char*) 0xB9000;
    result = *pointer;
    return FAIL; // If exception BSODs, we never get here
}

/* null_test
* Cause blue screen of death if dereferencing 0 causes a page fault
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays fault message
* Coverage: Page fault handling of location 0.
* Files: paging.c
*/
int null_test() {
    TEST_HEADER;
    char result;
    char* pointer = (char*) 0;
    result = *pointer;
    return FAIL; // If exception BSODs, we never get here
}

/* seg_not_present_exception_test
* Cause blue screen of death if a keyboard is pressed
* Inputs: None
* Outputs: None
* Side Effects: Halts the system and displays segment not present message
* Coverage: Exception handling
* Files: idt.c
*/
int seg_not_present_exception_test() {
    TEST_HEADER;
    idt[KEYBOARD_VEC_NUM].present = 0;
    while(1);
    return FAIL;
}

/* Checkpoint 2 tests */

/* rtc_read_write_test
* Test all RTC frequencies
* Inputs: None
* Outputs: None
* Side Effects: Opens the RTC and writes many different frequencies to it.
* Coverage: RTC syscall functions
* Files: rtc.c
*/
int rtc_read_write_test() {
    TEST_HEADER;
    uint32_t i;
    uint32_t j;
    int32_t retval = 0;

    retval += rtc_open(NULL);
    for(i = 2; i <= 1024; i*=2) {
        retval += rtc_write(NULL, &i, sizeof(uint32_t));
        printf("Testing: %d Hz\n[", i);
        for(j = 0; j < i; j++) {
            retval += rtc_read(NULL, NULL, NULL);
            printf("#");
        }
        printf("]\n");
    }
    if(retval == 0) {
        return PASS;
    } else {
        return FAIL;
    }

}

/*    print_filename
*    inputs: buf - buffer that contains filename to print
*    Function: prints file name with emtpy spaces
*    Files: fs_driver.c
*/
void print_filename(uint8_t* buf){
    int i =0;
    for(i =0;i<MAX_FILENAME;i++){
        if(buf[i] != NULL){
            putc(buf[i]);
        }else{
            printf(" ");
        }
    }
}

/*    dir_test
*    inputs: none
*    Coverage: checks correct functionality of read_dentry_by_index, dir_open, dir_read,dir_close,dir_write
*    Function: prints the list of filename and corresponding file types and file sizes.
*    Files: fs_driver.c
*/
int dir_test(){
    uint8_t empty_filename;
    int32_t empty_fd;
    uint8_t buf[32];
    int32_t empty_nbytes;
    int32_t size;
    inode_t* cur_inode_block_ptr;
    dentry_t* cur_dentry;
    int i;

    clear();
    dir_open(&empty_filename);
    uint32_t dentry_num = boot_block_ptr->num_dentries;

    for(i=0;i<dentry_num;i++){
        cur_dentry = (dentry_t*)&(boot_block_ptr->dentry[i]);
        cur_inode_block_ptr = (inode_t*)(inode_ptr + (cur_dentry->inode_num));
        size = cur_inode_block_ptr->length;
        dir_read(empty_fd,buf,empty_nbytes);
        printf("file_name: ");
        print_filename(buf);
        printf("  file_type: %d, file_size: %d", cur_dentry->ftype, size);
        printf("\n");
    }
    if(dir_write(empty_fd,buf,empty_nbytes) != -1){
        return FAIL;
    }
    dir_close(empty_fd);
    return PASS;
}

/*    term_driver_test
*    inputs: none
*    Coverage: Checks the functionality of terminal open, read, write, and close.
*    Function: Reads from user and then write back data that was read.
*    Files: terminal.c, keyboard.c
*/
int term_driver_test(){
    TEST_HEADER;
    //int result = PASS;
    int nbytes;
    char buf[1024];
    while(1){
        //Testing a buffer smaller than 128.
        terminal_write(0, (uint8_t*)"TESTING size 10\n", 16);
        nbytes = terminal_read(0, buf, 10);
        terminal_write(0, buf, nbytes);

        //Testing a buffer at the max buffer size
        terminal_write(0, (uint8_t*)"TESTING size 128\n", 17);
        nbytes = terminal_read(0, buf, 128);
        terminal_write(0, buf, nbytes);

        //Testing a buffer greater than the max buffer size
        terminal_write(0, (uint8_t*)"TESTING size 129\n", 17);
        nbytes = terminal_read(0, buf, 150);
        terminal_write(0, buf, nbytes);
    }
    return PASS;
}


/*    file_test
*    inputs: none
*    Coverage: checks correct functionality of read_dentry_by_name, file_open, file_read, file_close,file_write, read_data
*    Function: prints the data within file
*    Files: fs_driver.c
*/
int file_test(){
    uint8_t fname[MAX_FILENAME] = "grep";
    int32_t fd = FD_BEGIN;
    int32_t bytes_to_read;
    uint8_t block_buf[BLOCK_SIZE];
    int32_t bytes_read;
    int32_t total_bytes_read = 0;
    int i;
    clear();
    if(file_open((uint8_t*)&fname) == -1){
        printf("file does not exist");
        return PASS;
    }

    bytes_to_read = BLOCK_SIZE; //divide this number by 4 if you wanna see ELF in big files
    while(1){
        bytes_read = file_read(fd, block_buf, bytes_to_read);
        for(i =0; i <bytes_read ;i++){
            if(block_buf[i] != NULL){
                putc(block_buf[i]);
            }
        }
        printf("\n");
        printf("bytes_read: %d\n",bytes_read);

        total_bytes_read += bytes_read;
        if(bytes_read == 0){
            printf("you have reached the end of the file\n");
            break;
        }
        //break;       //comment out if you wanna see ELF in big files
    }
    printf("Total bytes read: %d\n",total_bytes_read );
    if(file_write(FD_BEGIN,block_buf,bytes_read) != -1){
        return FAIL;
    }

    file_close(FD_BEGIN);
    return PASS;
}

/* Checkpoint 3 tests */
/* Checkpoint 4 tests */
/* Checkpoint 5 tests */


/* Test suite entry point */
void launch_tests(){
    /* ---- Checkpoint 1 ---- */
    //TEST_OUTPUT("idt_test", idt_test());
    //TEST_OUTPUT("division_exception_test", division_exception_test());
    //TEST_OUTPUT("syscall_handler_test", syscall_handler_test());
    //TEST_OUTPUT("seg_not_present_exception_test: PRESS A BUTTON TO FIND OUT ", seg_not_present_exception_test());

    /* ---- Paging Tests ---- */
    //TEST_OUTPUT("paging_test", paging_test());
    //TEST_OUTPUT("kernel_up_bound_test", kernel_up_bound_test());
    //TEST_OUTPUT("kernel_low_bound_test", kernel_low_bound_test());
    //TEST_OUTPUT("vidmem_up_bound_test", vidmem_up_bound_test());
    //TEST_OUTPUT("vidmem_low_bound_test", vidmem_low_bound_test());
    //TEST_OUTPUT("null_test", null_test());

    /* ---- File System Tests ---- */
    //TEST_OUTPUT("dir_test", dir_test());
    //TEST_OUTPUT("file_test", file_test());

    /* ---- RTC Tests ---- */
    //TEST_OUTPUT("rtc_read_write_test", rtc_read_write_test());

    /* ---- Terminal Driver Tests ---- */
    //TEST_OUTPUT("term_driver_test", term_driver_test());
}
