#include "syscall.h"
#include "rtc.h"
#include "paging.h"
#include "terminal.h"
#include "x86_desc.h"

//variables for keeping track of the pid values
uint32_t cur_pid = 0;
uint32_t parent_pid = 0;
uint32_t pid_array[PID_MAX];

//Assembly functions. Descriptions in sycall_support.S
extern void flush_tlb();
extern void halt_ret(uint32_t execute_ebp, uint32_t execute_esp, uint8_t status);


/* int32_t halt(uint8_t status)
 * Inputs      : status
 * Return Value: always 0
 * Function    : terminates a process, returning the specified value to its parent process */
int32_t halt(uint8_t status){
  int i;
//---------Restore parent data-----------------------------------------

    pcb_t* cur_pcb_ptr = get_cur_pcb();
    //return to shell if it is the base shell
    if(cur_pcb_ptr->pid == 0){
        uint32_t eip_arg = cur_pcb_ptr->user_eip;
        uint32_t esp_arg = cur_pcb_ptr->user_esp;
        // eax = eip_arg, ebx = USER_DS, ecx = USER_CS, edx = esp_arg
        asm volatile ("\
            andl    $0x00FF, %%ebx  ;\
            movw    %%bx, %%ds      ;\
            pushl   %%ebx           ;\
            pushl   %%edx           ;\
            pushfl                  ;\
            popl    %%edx           ;\
            orl     $0x0200, %%edx  ;\
            pushl   %%edx           ;\
            pushl   %%ecx           ;\
            pushl   %%eax           ;\
            iret                    ;\
            "
            :
            : "a"(eip_arg), "b"(USER_DS), "c"(USER_CS), "d"(esp_arg)
            : "memory"
        );
    }
    //Update the pid values, so that the memory mapping and info are correct
    pcb_t* parent_pcb_ptr = get_pcb(cur_pcb_ptr->parent_pid);
    cur_pid = cur_pcb_ptr->parent_pid;
    parent_pid = parent_pcb_ptr->parent_pid;
    pid_array[cur_pcb_ptr->pid] = 0;

//---------restore parent paging----------------------------------------
    uint32_t phys_addr = (PHYS_MEM_START + cur_pid) * PAGE_4MB;
    page_directory[USER_INDEX].table_addr_31_12 = phys_addr/ALIGN_4KB; //pcb page_addr
    flush_tlb();

//---------Close any relevant FDs---------------------------------------
    for(i=0;i<MAX_FD_NUM;i++){
        cur_pcb_ptr->fd_array[i].flag = 0;
    }

//---------Write Parent process' info back to TSS(esp0)-----------------
    tss.ss0 = KERNEL_DS;
    tss.esp0 = EIGHT_MB - (EIGHT_KB*parent_pcb_ptr->pid) - sizeof(int32_t);

//---------Jump to execute return---------------------------------------
    halt_ret(cur_pcb_ptr->exec_ebp,cur_pcb_ptr->exec_esp,status);

    return -1;
}



/* int32_t execute(const uint8_t* command)
 * Inputs      : command - sequence of words used for commands or arguemtns
 * Return Value: -1   upon failure due to non-existing or non-executable file
 *               256  upon exception
 *              0-255 upon syscall halt
 * Function    :  load and execute a new program, handling off the processor to the new program till it terminates */
int32_t execute(const uint8_t* command){
    int i,j;          //Loop variable
    int cmd_start = 0;    //file_cmd first index
    int arg_start = 0;
    int blank_count =0;

    //Variables for getting file name and args from command
    int length = strlen((const int8_t*)command);
    int file_cmd_length = 0;
    int file_arg_length = 0;

    //File system variables
    uint8_t file_cmd[MAX_FILENAME];
    uint8_t file_arg[MAX_FILENAME];
    
    dentry_t temp_dentry;
    uint8_t elf_buf[sizeof(int32_t)];

    //Arguments for switching to user context.
    uint32_t eip_arg;
    uint32_t esp_arg;


//----------Parse arguments ----------------------------------
    for (i=0;i<MAX_FILENAME;++i){
      file_cmd[i] = '\0';
      file_arg[i] = '\0';
    }

    //parse cmd
    for(i = 0; i < length; ++i){
        //Gets the args and file name from the command word.
        if(command[i] != ' '){
            file_cmd[cmd_start] = command[i];
            ++file_cmd_length;
            ++cmd_start;
        }
        else{
            ++blank_count;
            if(file_cmd_length > 0)
                break;
        }
    }

    //parse arg
    for(i = cmd_start+blank_count; i< length; ++i){
        if(command[i] != ' '){
            for(j=i;j<length;j++){
                file_arg[arg_start] = command[j];
                ++arg_start;                
            }    
            break;
        }
        else{
            if(file_arg_length > 0)
                break;
        }
    }

//-----------check file validity (Search file system)------------------------

    if(read_dentry_by_name(file_cmd, &temp_dentry)==-1){
        return -1; /* file does not exist*/
    }

    if(read_data(temp_dentry.inode_num, 0, elf_buf, sizeof(int32_t)) == -1){
        return -1;  /* something failed during read_data*/
    }

    if(!(elf_buf[0] == MAGIC0 && elf_buf[1] == MAGIC1 &&
         elf_buf[2] == MAGIC2 && elf_buf[3] == MAGIC3)) {
        return -1; /* ELF not found, not an executable*/
    }


//------------Set up paging (Start at 8MB and use PID to decide)--------------------------------
    pcb_t* pcb_ptr;
    // pcb_t* parent_pcb_ptr;
    int pid_flag = 0;
    for(i = 0; i < MAX_PID ;i++){         /* find available pid, max 3 for now */
        if(pid_array[i] == 0){
            pid_array[i] = 1;
            cur_pid = i;                  /* set cur_pid to new one*/
            pid_flag =1;    
            break;
        }
    }
    if(pid_flag == 0){
        printf("pid full");
        return -1;
    }

    uint32_t mem_index = PHYS_MEM_START + cur_pid;               // Skip first page
    uint32_t phys_addr = mem_index * PAGE_4MB;      // Physical address
    page_directory[USER_INDEX].table_addr_31_12 = phys_addr/ALIGN_4KB;//user virtual to physical
    flush_tlb();


//------------load file into memory---------------------------------------------------
    inode_t* temp_inode_ptr = (inode_t *)(inode_ptr+temp_dentry.inode_num);
    uint8_t* image_addr = (uint8_t*)PROGRAM_IMAGE_ADDR;           //it should stay the same, overwriting existing program image
    read_data(temp_dentry.inode_num, (uint32_t)0, image_addr,temp_inode_ptr->length); //copying entire file to memory starting at Virt addr 0x08048000

//-------------Create PCB/Open FDs-----------------------------------------------------

    pcb_ptr = get_pcb(cur_pid);          /* create PCB       */
    pcb_ptr->pid = cur_pid;

    //Update pid values.
    if(cur_pid != 0 ){
        pcb_ptr->parent_pid = parent_pid;
        parent_pid = cur_pid;
    }else{
        pcb_ptr->parent_pid = cur_pid;
    }

    // Initialize the file descriptor array
    for (i = 0; i < MAX_NUM_FILE;i++) {
        pcb_ptr->fd_array[i].fop_table_ptr = &null_fop;
        pcb_ptr->fd_array[i].inode = 0;
        pcb_ptr->fd_array[i].file_pos = INIT_FILE_POS;
        pcb_ptr->fd_array[i].flag = FD_FREE;
    }

    pcb_ptr->fd_array[0].fop_table_ptr = &stdin_fop;        /* stdin  */
    pcb_ptr->fd_array[0].inode = 0;
    pcb_ptr->fd_array[0].file_pos = INIT_FILE_POS;
    pcb_ptr->fd_array[0].flag = FD_BUSY;

    pcb_ptr->fd_array[1].fop_table_ptr = &stdout_fop;       /* stdout */
    pcb_ptr->fd_array[1].inode = 0;
    pcb_ptr->fd_array[1].file_pos = INIT_FILE_POS;
    pcb_ptr->fd_array[1].flag = FD_BUSY;

    strncpy((int8_t*)pcb_ptr->cmd_arg, (int8_t*)(file_arg), MAX_FILENAME);


  //------------Prepare for context switch--------------------------------------------------
    //Bytes 24 to 27 of the executable. entry point
    uint8_t eip_buf[ELF_SIZE];
    read_data(temp_dentry.inode_num, ELF_START, eip_buf, sizeof(int32_t)); // Read eip from elf (location 24)
    eip_arg = *((int*)eip_buf);

    esp_arg = USER_MEM + PAGE_4MB - sizeof(int32_t); // where program starts

    pcb_ptr->user_eip = eip_arg;
    pcb_ptr->user_esp = esp_arg;

    //For privilege level switch
    tss.ss0 = KERNEL_DS;
    tss.esp0 = EIGHT_MB - (EIGHT_KB*cur_pid) - sizeof(int32_t);
    pcb_ptr->tss_esp0 = tss.esp0;

    //Get the esp and ebp values for the user context switch.
    uint32_t esp;
    uint32_t ebp;
    asm("\t movl %%esp, %0" : "=r"(esp));
    asm("\t movl %%ebp, %0" : "=r"(ebp));
    pcb_ptr->exec_esp = esp;
    pcb_ptr->exec_ebp = ebp;

    //Enable interrupts
    sti();
  //------------Push IRET context to stack-------------------------------------------------
  // eax = eip_arg, ebx = USER_DS, ecx = USER_CS, edx = esp_arg
    asm volatile ("\
        andl    $0x00FF, %%ebx  ;\
        movw    %%bx, %%ds      ;\
        pushl   %%ebx           ;\
        pushl   %%edx           ;\
        pushfl                  ;\
        popl    %%edx           ;\
        orl     $0x0200, %%edx  ;\
        pushl   %%edx           ;\
        pushl   %%ecx           ;\
        pushl   %%eax           ;\
        iret                    ;\
        "
        :
        : "a"(eip_arg), "b"(USER_DS), "c"(USER_CS), "d"(esp_arg)
        : "memory"
    );

    return 0;
}



/* void fop_init()
 * Inputs      : none
 * Return Value: none
 * Function    :  initializes fop table   */
void fop_init(){
    null_fop.read = null_read;
    null_fop.write = null_write;
    null_fop.open = null_open;
    null_fop.close = null_close;

    rtc_fop.read = rtc_read;
    rtc_fop.write = rtc_write;
    rtc_fop.open = rtc_open;
    rtc_fop.close = rtc_close;

    dir_fop.read = dir_read;
    dir_fop.write = dir_write;
    dir_fop.open = dir_open;
    dir_fop.close = dir_close;

    file_fop.read = file_read;
    file_fop.write = file_write;
    file_fop.open = file_open;
    file_fop.close = file_close;

    stdin_fop.read = terminal_read;
    stdin_fop.write = null_write;
    stdin_fop.open = terminal_open;
    stdin_fop.close = terminal_close;

    stdout_fop.read = null_read;
    stdout_fop.write = terminal_write;
    stdout_fop.open = terminal_open;
    stdout_fop.close = terminal_close;
}

/* int32_t open(const uint8_t* fname)
 * Inputs      : fname - file to get access to
 * Return Value: file descriptor  that has access to file
 * Function    : provides access to file system    */
int32_t open(const uint8_t* fname){
    int i;
    dentry_t dentry;

    if(strlen((char*)fname) == NULL){
        return -1;  /* empty string*/
    }

    if(read_dentry_by_name(fname, &dentry) == -1){
        return -1;  /* return -1 upon failure*/
    }

  pcb_t* cur_pcb_ptr = get_cur_pcb();

    /* find open spot in fd array */
    for(i = FD_BEGIN; i < MAX_NUM_FILE; i++) {
        if(cur_pcb_ptr->fd_array[i].flag == FD_FREE) {
            cur_pcb_ptr->fd_array[i].file_pos = INIT_FILE_POS;
            cur_pcb_ptr->fd_array[i].inode = dentry.inode_num;
            cur_pcb_ptr->fd_array[i].flag = FD_BUSY;


            if(dentry.ftype == 0){                /* rtc*/
                cur_pcb_ptr->fd_array[i].fop_table_ptr = &rtc_fop;
            }else if (dentry.ftype == 1){         /* directory */
                cur_pcb_ptr->fd_array[i].fop_table_ptr = &dir_fop;
            }else if (dentry.ftype == 2){         /* regular file */
                cur_pcb_ptr->fd_array[i].fop_table_ptr = &file_fop;
            }

            return i; /* return fd upon success*/
        }
    }

  return -1;  /* return -1 upon failure*/
}


/* int32_t close(int32_t fd)
 * Inputs      : fd - file descriptor to close the fil
 * Return Value: 0
 * Function    : closes the specified file descriptor and make it available for next open call */
int32_t close(int32_t fd){
    if(fd < FD_BEGIN || fd > MAX_NUM_FILE){
        return -1;
    }

    pcb_t* cur_pcb_ptr = get_cur_pcb();
    if(cur_pcb_ptr->fd_array[fd].flag == FD_FREE){
        return -1; // cannot close unopend fd
    }

    cur_pcb_ptr->fd_array[fd].flag = FD_FREE;
    cur_pcb_ptr->fd_array[fd].file_pos= INIT_FILE_POS;
    cur_pcb_ptr->fd_array[fd].inode = 0;
    
    return cur_pcb_ptr->fd_array[fd].fop_table_ptr->close(fd);
;
}


/* int32_t read(int32_t fd, void* buf, int32_t nbytes)
 * Inputs      : fd     - file descriptor to read data
 *               buf    - buffer to read data into
 *               nbytes - number of bytes to read from file
 * Return Value: ret - number of bytes read
 * Function    : reads data from keyboard ,file, device, or directory*/
int32_t read(int32_t fd, void* buf, int32_t nbytes){
    sti();
    
    if(fd < 0 || fd > MAX_NUM_FILE){
        return -1;
    }
    
    if (buf == NULL){
       return -1;
    }

    if (nbytes < 0){
        return -1;
    }    

    pcb_t* cur_pcb_ptr = get_cur_pcb();
    if(cur_pcb_ptr->fd_array[fd].flag == FD_FREE) {
        return -1;
    }

    int32_t ret = cur_pcb_ptr->fd_array[fd].fop_table_ptr->read(fd,buf,nbytes);
    return ret;
}


/* int32_t write(int32_t fd, void* buf, int32_t nbytes)
 * Inputs      : fd     - file descriptor to write data
 *               buf    - buffer that contains data to write
 *               nbytes - number of bytes to write into
* Function: writes data to the terminal or to a device  (not required)*/
int32_t write(int32_t fd, void* buf, int32_t nbytes){
    if(fd < 0 || fd > MAX_NUM_FILE){
        return -1;
    }
    
    if (buf == NULL){
       return -1;
    }
    
    if (nbytes < 0){
        return -1;
    }

    pcb_t* cur_pcb_ptr = get_cur_pcb();

    if(cur_pcb_ptr->fd_array[fd].flag == FD_FREE) {
        return -1;
    }

    return cur_pcb_ptr->fd_array[fd].fop_table_ptr->write(fd,buf,nbytes);
}

//-----------------mp 3.4-------------------------------------

/* int32_t getargs(uint8_t* buf, int32_t nbytes)
 * Inputs      : buf    - buffer to which argument will be written 
 *               nbytes - number of bytes to write into
* Function: write argument into input buf */
int32_t getargs(uint8_t* buf, int32_t nbytes){
    if(buf == NULL){
        return -1;
    }
    pcb_t* cur_pcb = get_cur_pcb();
    if(cur_pcb->cmd_arg[0] == NULL){
        return -1;
    }
    strncpy((int8_t*)buf, (int8_t*)(cur_pcb->cmd_arg), nbytes);
    return 0;
}

/* int32_t vidmap(uint32_t** screen_start)
 * Inputs      : buf    - double pointer to user address space 
* Function: pass virtual address which points to video memory */
int32_t vidmap(uint32_t** screen_start){

    // check if screen start is valid 
    if ( screen_start == NULL)
        return -1;

    int addr = (uint32_t)screen_start;
    if ( addr < USER_MEM || addr > (USER_MEM+PAGE_4MB)) 
        return -1;


    // set up virtual add at 136mb for vidmap
    page_directory[VIDEO_INDEX].size = 0;
    page_directory[VIDEO_INDEX].present = 1;
    page_directory[VIDEO_INDEX].user = 1;
    page_directory[VIDEO_INDEX].table_addr_31_12 =((int)page_table_vidmap)/ALIGN_4KB;
    
    page_table_vidmap[0].present =1;
    page_table_vidmap[0].user = 1;
    page_table_vidmap[0].page_addr_31_12 = VIDMEM_ADDR/ALIGN_4KB; // 0xB8000

    flush_tlb();

    *screen_start = (uint32_t*)(VM_VIDEO);  //0x8800000
    return 0;
}

int32_t set_handler(int32_t signum, void* handler_address){
    return -1;
}
int32_t sigreturn(void){
    return -1;
}
//-------------------------------------------------------------


//---------------helper functions -----------------------------
/* pcb_t* get_cur_pcb(){
 * Function: get addres to current pcb */
pcb_t* get_cur_pcb(){
    return (pcb_t*)(EIGHT_MB - EIGHT_KB*(cur_pid+1));
}
/* pcb_t* get_pcb(){
 * Function: get addres to pcb corresponding to input pid */
pcb_t* get_pcb(uint32_t pid){
    return (pcb_t*)(EIGHT_MB - EIGHT_KB*(pid+1));
}

int32_t null_read(int32_t fd, void* buf, int32_t nbytes){
    return -1;
}

int32_t null_write(int32_t fd, const void* buf, int32_t nbytes){
    return -1;
}

int32_t null_open(const uint8_t* fname){
    return -1;
}
int32_t null_close(int32_t fd){
    return -1;
}


