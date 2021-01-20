#define ASM     1

.globl enable
.align 4
enable:
  pushl %ebp
  movl  %esp, %ebp
  movl  8(%ebp), %eax     # Gets the argument for the function

  movl %eax, %cr3

  movl %cr4, %eax
  orl  $0x00000010, %eax  # Sets the bit for using 4MB pages
  movl %eax, %cr4

  movl %cr0, %eax
  orl  $0x80000001, %eax  # Sets PG (paging) and PE (protection) bits for paging
  movl %eax, %cr0

  movl %cr3, %eax         # Flush TLB
  movl %eax, %cr3

  leave
  ret


.globl flush_tlb
.align 4
flush_tlb:
    movl %cr3, %eax    
    movl %eax, %cr3
    ret
