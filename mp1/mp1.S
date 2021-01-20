.data					# section declaration

        # Useful offset constants for accessing members of a
        # struct mp1_blink_struct structure
        LOCATION   = 0
        ON_CHAR    = 2
        OFF_CHAR   = 3
        ON_LENGTH  = 4
        OFF_LENGTH = 6
        COUNTDOWN  = 8
        STATUS     = 10
        NEXT       = 12

        STRUCT_SIZE = 16

# Pointer to head of list (initialized to NULL)
mp1_list_head:
        .long   0

.text					# section declaration

# Export the function symbol names
.global mp1_rtc_tasklet
.global mp1_ioctl

/* jump table for ioctl */
jump_table:             
  .long mp1_ioctl_add, mp1_ioctl_remove, mp1_ioctl_find, mp1_ioctl_sync

# void mp1_poke(void);
#
# Interface: Register-based arguments (not C-style)
#    Inputs: %cl  - The byte you wish to write
#            %eax - Offset from the start of video memory that you wish
#                   to write to
#   Outputs: Text-mode video screen is written to at location %eax with
#            the byte in %cl
# Registers: Clobbers EDX

mp1_poke:
	movl    vmem_base_addr(,1),%edx
	movb    %cl,(%edx,%eax,1)
	ret

/*-------------------------------------------------------------------------------------------*/
/* void mp1_rtc_tasklet (unsigned long);

    Interface: stack-based arguments 
    Function: Whenever RTC interrupt is generated, 
              tasklet goes through linked list to update the state of each blink struct. 
              It makes fish blink between two frames.               
    Registers: clobbers %ebx, %dx, %eax, %cl
*/

mp1_rtc_tasklet:
  pushl %ebp                  /* create stack frame */
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi           

  movl mp1_list_head, %ebx    

tasklet_base_case:              /* if pointer is 0x0, end */
  cmpl $0, %ebx               
  je tasklet_end
  decw COUNTDOWN(%ebx)          /* check countdown to determine whether to examine struct or not */     
  cmpw $0, COUNTDOWN(%ebx)    
  je tasklet_examine

tasklet_next:                   /* move to next struct */
  movl NEXT(%ebx), %ebx
  jmp tasklet_base_case

tasklet_examine:                /* check on or off status */
  cmpw $1, STATUS(%ebx)        
  je off_to_screen
  
on_to_screen:                   /* put ON to video memory and call mp_poke */       
  movzwl LOCATION(%ebx), %eax     
  shll %eax
  movb ON_CHAR(%ebx), %cl
  call mp1_poke

  movw ON_LENGTH(%ebx), %dx       /* set countdown to on_length and invert status */
  movw %dx, COUNTDOWN(%ebx)
  xorw $1, STATUS(%ebx)
  jmp tasklet_next

off_to_screen:                    /* put OFF to video memory and call mp_poke */              
  movzwl LOCATION(%ebx), %eax
  shll %eax
  movb OFF_CHAR(%ebx), %cl
  call mp1_poke

  movw OFF_LENGTH(%ebx), %dx      /* set countdown to off_length and invert status */
  movw %dx, COUNTDOWN(%ebx)
  xorw $1, STATUS(%ebx)
  jmp tasklet_next

tasklet_end:
  popl %edi         /*tear down stack frame and return*/
  popl %esi
  popl %ebx
  leave
  ret

/*-------------------------------------------------------------------------------------------*/
/* int mp1_ioctl (unsigned long arg, unsigned long cmd)
       Interface: stack-based arguments 
        Function: uses the value of cmd to determine which funcntion to jump to. "dispatcher"
       Registers: clobbers %eax
           Input: cmd value 
          Output: -1 if invallid 
*/

mp1_ioctl:             
  movl 8(%esp), %eax      
  cmpl $0, %eax             /* check if cmd is within range */
  jl ioctl_invalid
  cmpl $3, %eax
  jg ioctl_invalid
  jmp *jump_table(,%eax,4)  /* jump to where cmd points to */

ioctl_invalid:  
  movl $-1, %eax            /* out of range, return -1 */
  ret

/*-------------------------------------------------------------------------------------------*/
/* int mp1_ioctl_add (unsigned long arg)
          Interface: stack-based arguments 
          Function: Copy struct from user space into new struct and insert into linked list. 
         Registers: Clobbers %eax, %ebx, %esi, %dx 
             Input: pointer to a mp1_blink_struct 
            Output:  0 - successful 
                    -1 - fail
*/

mp1_ioctl_add:
  pushl %ebp            /* create stack frame */
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  pushl $STRUCT_SIZE     /* dynamically allocate memory using mp_malloc and check if is suceeded */ 
  call mp1_malloc
  add $4, %esp
  cmpl $0, %eax              
  je add_malloc_error
  
  movl %eax, %esi        /* %esi - address to newly allocated memory*/
  movl 8(%ebp), %ebx
  cmpl $0, %ebx          /* check if pointer to structure is valid */   
  je add_error

  pushl $STRUCT_SIZE     /* call copy_from_user function*/
  pushl %ebx
  pushl %esi
  call mp1_copy_from_user
  add $12, %esp
  
  cmpl $0, %eax            /* if copy didn't suceed, free memory and return -1 */ 
  je add_check_range
  jmp add_error

add_check_range:
  cmpw $80*25, LOCATION(%esi)       /* check if location is within range 0 <  < 80*25 */
  ja add_error
  cmpw $0, LOCATION(%esi)
  jb add_error

  movw ON_LENGTH(%esi), %dx       /* set the countdown field to be on_length field, and set status as 1 */
  movw %dx, COUNTDOWN(%esi)
  movw $1, STATUS(%esi)          

  movl mp1_list_head, %eax        /* make this struct new head */      
  movl %eax, NEXT(%esi)
  movl %esi, mp1_list_head

  movzwl LOCATION(%esi), %eax     /* set offset and ASCII code and call mp1_poke */
  shll $1, %eax                     
  movb ON_CHAR(%esi), %cl
  call mp1_poke

add_success:
  movl $0, %eax     /*add is successful, return 0*/
  jmp add_tear_down     

add_error:          /* something happend, free memory and return*/
  pushl %esi 
  call mp1_free
  add $4, %esp

add_malloc_error: 
  movl $-1, %eax   

add_tear_down:        
  popl %edi         /*tear down frame*/
  popl %esi
  popl %ebx
  leave
  ret

/*-------------------------------------------------------------------------------------------*/
/*  int mp1_ioctl_remove(unsigned long arg)
            Interface: stack-based arguments 
              Function: find a particular struct with given location and remove it from the list
             Registers: clobbers %eax, %ebx, %esi, %dx
                 Input: an integer location 
                Output: 0 - successful
                       -1 - fail
*/

mp1_ioctl_remove:
  pushl %ebp            /*create stack frame*/
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  movw 8(%ebp), %dx     /*call utility search function to find the struct with given location*/    
  pushw %dx
  call search_util
  addl $2, %esp
  
  cmpl $-1, %eax
  je remove_error
  movl %eax, %ebx         /* %ebx - node found */

  pushl %ebx              /*call utility search function to find previous node*/
  call search_prev
  add $4, %esp
  cmpl $-1, %eax
  je remove_error
  
  movl NEXT(%ebx), %esi   /* %esi - next node of found node  */
  cmpl $0, %eax           
  je its_head
  movl %esi, NEXT(%eax)     
  jmp free_memory

its_head:                 /* set the next node as head */
  movl %esi, mp1_list_head

free_memory:              /* call mp1_free */
  pushl %ebx
  call mp1_free
  add $4, %esp

remove_success:            /*return 0 */
  movl $0, %eax
  jmp remove_tear_down

remove_error:               /*return -1 */
  movl $-1, %eax

remove_tear_down:  
  popl %edi         /*tear down frame*/
  popl %esi
  popl %ebx
  leave
  ret

/*  int search prev(unsigned long arg)
            Interface: stack-based arguments 
              Function: find the previous node
             Registers: clobbers %eax, %ebx
                 Input: pointer to node 
                Output: pointer to the previous node - successful
                        0 - indicate that there is no previous node (its head)
                       -1 - fail due to an error
*/
search_prev:
  pushl %ebp            /*create stack frame*/
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  movl 8(%ebp), %ebx    /* check if given node is head*/
  movl mp1_list_head, %eax
  cmpl %eax, %ebx
  je search_prev_head

search_prev_base_case:   /*search through list to find the previous node*/ 
  cmpl NEXT(%eax), %ebx
  je search_prev_tear_down    
  movl NEXT(%eax), %eax
  jmp search_prev_base_case

search_prev_error:
  movl $-1, %eax          /*return -1 */
  jmp search_prev_tear_down

search_prev_head:         /*return 0 */
  movl $0, %eax

search_prev_tear_down:  
  popl %edi               /*tear down frame*/
  popl %esi
  popl %ebx
  leave
  ret

/*-------------------------------------------------------------------------------------------*/
/*  int mp1_ioctl_find(unsigned long arg)
              Interface: stack-based arguments 
              Function: find a particular struct with given location and copy it to user level struct
             Registers: clobbers %eax, %ebx, %esi
                 Input: an integer location 
                Output: 0 - successful
                       -1 - fail
*/

mp1_ioctl_find:
  pushl %ebp            /*create stack frame*/
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  pushl $STRUCT_SIZE     /* dynamically allocate memory using mp_malloc and check if is suceeded */ 
  call mp1_malloc
  add $4, %esp
  cmpl $0, %eax              
  jne find_malloc_success   
  movl $-1, %eax
  jmp find_tear_down

find_malloc_success:
  movl %eax, %esi        /* %esi - address to newly allocated memory*/
  movl 8(%ebp), %ebx

  pushl $STRUCT_SIZE     /* call copy_from_user function*/
  pushl %ebx
  pushl %esi
  call mp1_copy_from_user
  add $12, %esp
  
  cmpl $0, %eax            /* if copy didn't suceed, free memory and return -1 */ 
  je find_check_range
  jmp find_error

find_check_range:
  cmpw $80*25, LOCATION(%esi)       /* check if location is within range 0 <  < 80*25 */
  ja add_error
  cmpw $0, LOCATION(%esi)
  jb add_error

  pushw LOCATION(%esi)    /*call search utility function to find the node with given location*/
  call search_util
  addl $2, %esp 
  cmpl $-1, %eax
  je find_error

  pushl $STRUCT_SIZE    /*call mp1_copy_to_user*/
  pushl %eax
  pushl 8(%ebp)
  call mp1_copy_to_user
  add $12, %esp
  
  cmp $0, %eax      /*check whether copy succeeded*/
  je find_sucess

find_error:
  movl $-1, %eax    /*set return -1*/
  jmp find_free 

find_sucess:        /*set return 0*/  
  movl $0, %eax

find_free:          /*free allocated memory*/
  pushl %esi 
  call mp1_free
  add $4, %esp

find_tear_down:        
  popl %edi         /*tear down frame*/
  popl %esi
  popl %ebx
  leave
  ret


/*-------------------------------------------------------------------------------------------*/
/*  int mp1_ioctl_sync(unsigned long arg)
              Interface: stack-based arguments 
              Function: synchronizes two existing location on the screen
             Registers: clobbers %eax, %ebx, %edi, %esi, %dx, %cl
                 Input: two 2byte unsigned short integer 
                Output: 0 - successful
                       -1 - fail
*/

mp1_ioctl_sync:
  pushl %ebp            /*create stack frame*/
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  movl 8(%ebp), %ebx    /*check if structure is valid*/
  cmpl $0, %ebx 
  je sync_error

  andl $0x0000FFFF, %ebx    /*extract lower by anding with 0x0000FFFF and get corresponding struct*/
  pushl %ebx
  call search_util
  add $4, %esp
  cmpl $-1, %eax
  je sync_error

  movl %eax, %edi         /* %edi - second*/

  movl 8(%ebp), %ebx      /*extract upper by shifting two bytes and get corresponding struct*/
  shr $16, %ebx
  pushl %ebx
  call search_util
  add $4, %esp
  cmpl $-1, %eax
  je sync_error     

  movl %eax, %esi         /* %esi - first*/

  movw ON_LENGTH(%esi), %dx       /* copy from first to second */
  movw %dx, ON_LENGTH(%edi)
  movw OFF_LENGTH(%esi), %dx
  movw %dx, OFF_LENGTH(%edi)
  movw COUNTDOWN(%esi), %dx
  movw %dx, COUNTDOWN(%edi)
  movw STATUS(%esi), %dx
  movw %dx, STATUS(%edi)
    
  cmpw $1, %dx                /* check status determine whether to display on or off */
  je sync_off
  
sync_on:  
  movzwl LOCATION(%edi), %eax     /* move location offset to %eax and ON ascii code to %cl and call mp_poke*/
  shll $1,%eax  
  movb ON_CHAR(%edi), %cl
  call mp1_poke
  jmp sync_success

sync_off:
  movzwl LOCATION(%edi), %eax     /* move location offset to %eax and OFF ascii code to %cl and call mp_poke*/
  shll $1,%eax
  movb OFF_CHAR(%edi), %cl
  call mp1_poke

sync_success:       /*return 0*/
  movl $0, %eax
  jmp sync_tear_down

sync_error:
  movl $-1, %eax    /*return -1*/

sync_tear_down:        
  popl %edi         /*tear down frame*/
  popl %esi
  popl %ebx
  leave
  ret

/*  *int search_util (unsigned long arg)
            Interface: stack-based arguments 
              Function: find a particular struct with given location
             Registers: clobbers %eax, %ax, %ebx
                 Input: two 2byte unsigned short integer 
                Output: pointer to the node - successful
                       -1 - fail
*/
search_util:
  pushl %ebp            /*create stack frame*/
  movl %esp, %ebp
  pushl %ebx
  pushl %esi
  pushl %edi

  movw 8(%ebp), %ax          /* %ax - input location */
  movl mp1_list_head, %ebx   /* %ebx - pointer to head  */
  
search_base_case:       /*go through the list to find the node with given location*/
  cmpl $0, %ebx               
  je search_fail
  cmpw %ax, LOCATION(%ebx)
  je search_sucess
  movl NEXT(%ebx), %ebx
  jmp search_base_case


search_sucess:       /*return the pointer to struct with corresponding location*/
  movl %ebx, %eax
  jmp search_tear_down

search_fail:          /*return -1*/
  movl $-1, %eax
      
search_tear_down:
  popl %edi         /*tear down frame*/
  popl %esi
  popl %ebx
  leave
  ret


.end
