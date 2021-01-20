#ifndef EXCEPTION_WRAPPER_H
#define EXCEPTION_WRAPPER_H

extern void divide_error(void);
extern void debug(void);
extern void nmi(void);
extern void breakpoint(void);
extern void overflow(void);
extern void bounds(void);
extern void opcode(void);
extern void coprocessor(void);
extern void double_fault(void);
extern void segment_overun(void);
extern void invalid_tss(void);
extern void seg_not_present(void);
extern void stack_fault(void);
extern void general_protection_fault(void);
extern void page_fault(void);
extern void math_fault(void);
extern void alignment_check(void);
extern void machine_check(void);
extern void simd_math_fault(void);
extern void virtualization_fault(void);

extern void syscall_placeholder(void);

#endif
