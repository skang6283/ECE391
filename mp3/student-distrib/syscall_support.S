#define ASM     1
#include "x86_desc.h"

# Goes back to executes halt.
# Takes in the values execute esp and ebp and the status.
.globl halt_ret
.align 4
halt_ret:
    pushl %ebp
    movl %esp, %ebp

    xorl %ebx, %ebx
    movl 16(%ebp), %ebx     # get third arg (status)
    movl %ebx, %eax         

    movl 12(%ebp), %ebx     # get second arg (esp)
    movl %ebx, %esp   

    movl 8(%ebp), %ebx       # get first arg (ep)
    movl %ebx, %ebp

    leave
    ret

.align 4
syscall_table:
    .long 0x0           # There is no syscall zero
    .long halt
    .long execute
    .long read
    .long write
    .long open
    .long close
    .long getargs
    .long vidmap
    .long set_handler
    .long sigreturn

.globl syscall_handler
.align 4
syscall_handler:
    # Save all registers except eax (can't use pushal :( )
    pushl   %ecx
    pushl   %edx
    pushl   %ebx
    pushl   %esp
    pushl   %ebp
    pushl   %esi
    pushl   %edi
    pushfl

    # Push three arguments
    pushl %edx
    pushl %ecx
    pushl %ebx

    # Verify syscall number
    cmpl    $0, %eax        # No syscall zero
    jz      syscall_err
    cmpl    $10, %eax       # Max syscall number = 10
    ja      syscall_err

    # Call syscall
    call    *syscall_table(, %eax, 4)   # 4 bytes per entry in table
    jmp     syscall_leave

syscall_err:
    movl    $-1, %eax       # Return -1 as error

syscall_leave:
    addl    $12, %esp       # mov stack pointer  up by 3 regs
    popfl
    popl    %edi
    popl    %esi
    popl    %ebp
    popl    %esp
    popl    %ebx
    popl    %edx
    popl    %ecx
    iret
