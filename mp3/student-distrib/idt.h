#ifndef IDT_H
#define IDT_H

#ifndef ASM

#include "lib.h"

#define NUM_EXCEPTIONS      (21)
#define SYSCALL_VEC_NUM     (0x80)
#define RTC_VEC_NUM         (40)
#define KEYBOARD_VEC_NUM    (33)

#define E0     (0)     
#define E1     (1)
#define E2     (2)  
#define E3     (3)  
#define E4     (4)
#define E5     (5)
#define E6     (6)
#define E7     (7)
#define E8     (8)
#define E9     (9)
#define E10    (10)
#define E11    (11)
#define E12    (12)
#define E13    (13)
#define E14    (14)
#define E15    (15)
#define E16    (16)
#define E17    (17)
#define E18    (18)
#define E19    (19)
#define E20    (20)

   
/**
 * This is what goes on the stack when running pushal
*/
 struct x86_regs {
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp;
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
 } __attribute__ (( packed ));

void idt_init(void);
void exception_handler(uint32_t id, struct x86_regs regs, uint32_t flags, uint32_t error);
void general_interrupt(void);

#endif /* ASM */
#endif /* IDT_H */
