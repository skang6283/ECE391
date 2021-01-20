#define ASM     1
#define SYSCALL_VEC_NUM (0x80)

#define EXCEPTION_ARGS(name, id)    \
.globl  name                        ;\
.align  4                           ;\
name:                               ;\
    pushfl                          ;\
    pushal                          ;\
    pushl   $id                     ;\
    call    exception_handler       ;\
    addl    $4, %esp                ;\
    popal                           ;\
    popfl                           ;\
    addl    $4, %esp                ;\
    iret        

#define EXCEPTION(name, id)         \
.globl  name                        ;\
.align  4                           ;\
name:                               ;\
    pushl   $0                      ;\
    pushfl                          ;\
    pushal                          ;\
    pushl   $id                     ;\
    call    exception_handler       ;\
    addl    $4, %esp                ;\
    popal                           ;\
    popfl                           ;\
    addl    $4, %esp                ;\
    iret

EXCEPTION(divide_error,                     0x00);
EXCEPTION(debug,                            0x01);
EXCEPTION(nmi,                              0x02);
EXCEPTION(breakpoint,                       0x03);
EXCEPTION(overflow,                         0x04);
EXCEPTION(bounds,                           0x05);
EXCEPTION(opcode,                           0x06);
EXCEPTION(coprocessor,                      0x07);
EXCEPTION_ARGS(double_fault,                0x08);
EXCEPTION(segment_overun,                   0x09);
EXCEPTION_ARGS(invalid_tss,                 0x0A);
EXCEPTION_ARGS(seg_not_present,             0x0B);
EXCEPTION_ARGS(stack_fault,                 0x0C);
EXCEPTION_ARGS(general_protection_fault,    0x0D);
EXCEPTION_ARGS(page_fault,                  0x0E);
EXCEPTION(math_fault,                       0x10);
EXCEPTION_ARGS(alignment_check,             0x11);
EXCEPTION(machine_check,                    0x12);
EXCEPTION(simd_math_fault,                  0x13);
EXCEPTION(virtualization_fault,             0x14);

EXCEPTION(syscall_placeholder,              SYSCALL_VEC_NUM);
