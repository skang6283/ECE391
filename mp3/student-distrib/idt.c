#include "idt.h"
#include "exception_wrapper.h"
#include "x86_desc.h"
#include "lib.h"
#include "handlers.h"
#include "syscall.h"

static char* exception_names[] = {
    "divide_error",
    "debug",
    "nmi",
    "breakpoint",
    "overflow",
    "bounds",
    "opcode",
    "coprocessor",
    "double_fault",
    "segment_overun",
    "invalid_tss",
    "seg_not_present",
    "stack_fault",
    "general_protection_fault",
    "page_fault",
    "reserved",
    "math_fault",
    "alignment_check",
    "machine_check",
    "simd_math_fault",
    "virtualization_fault"
};

void idt_init(void) {
    unsigned int i;

    for(i = 0; i < NUM_VEC; i++) {

        // Mark all exceptions as present
        if(i < NUM_EXCEPTIONS && i != E15) { // 15 is reserved by Intel
            idt[i].present = 0x1;
        } else {
            idt[i].present = 0x0;
        }

        idt[i].seg_selector = KERNEL_CS;
        idt[i].dpl = 0x0;           // Need to set this to 3 for sys calls
        idt[i].reserved0 = 0x0;
        idt[i].reserved1 = 0x1;
        idt[i].reserved2 = 0x1;
        idt[i].reserved3 = 0x0;     // Need to set this to 1 for interrupts
        idt[i].reserved4 = 0x0;
        idt[i].size = 0x1;          // always 32 bit
    }

    // Register all handlers for exceptions
    SET_IDT_ENTRY(idt[E0], divide_error);
    SET_IDT_ENTRY(idt[E1], debug);
    SET_IDT_ENTRY(idt[E2], nmi);
    SET_IDT_ENTRY(idt[E3], breakpoint);
    SET_IDT_ENTRY(idt[E4], overflow);
    SET_IDT_ENTRY(idt[E5], bounds);
    SET_IDT_ENTRY(idt[E6], opcode);
    SET_IDT_ENTRY(idt[E7], coprocessor);
    SET_IDT_ENTRY(idt[E8], double_fault);
    SET_IDT_ENTRY(idt[E9], segment_overun);
    SET_IDT_ENTRY(idt[E10], invalid_tss);
    SET_IDT_ENTRY(idt[E11], seg_not_present);
    SET_IDT_ENTRY(idt[E12], stack_fault);
    SET_IDT_ENTRY(idt[E13], general_protection_fault);
    SET_IDT_ENTRY(idt[E14], page_fault);
    SET_IDT_ENTRY(idt[E16], math_fault);
    SET_IDT_ENTRY(idt[E17], alignment_check);
    SET_IDT_ENTRY(idt[E18], machine_check);
    SET_IDT_ENTRY(idt[E19], simd_math_fault);
    SET_IDT_ENTRY(idt[E20], virtualization_fault);

    // Register temporary syscall handler
    SET_IDT_ENTRY(idt[SYSCALL_VEC_NUM], syscall_handler);
    idt[SYSCALL_VEC_NUM].dpl = 0x3;     // Specify userspace callable
    idt[SYSCALL_VEC_NUM].present = 0x1;

    // Register device interrupts
    idt[KEYBOARD_VEC_NUM].present = 1;
    idt[RTC_VEC_NUM].present = 1;
    idt[KEYBOARD_VEC_NUM].reserved3 = 0x1;
    idt[RTC_VEC_NUM].reserved3 = 0x1;
    SET_IDT_ENTRY(idt[KEYBOARD_VEC_NUM], KEYBOARD_WRAPPER);
    SET_IDT_ENTRY(idt[RTC_VEC_NUM], RTC_WRAPPER);


    lidt(idt_desc_ptr);
}

void exception_handler(uint32_t id, struct x86_regs regs, uint32_t flags, uint32_t error) {
    uint32_t cs;
    asm("\t movl %%cs, %0" : "=r"(cs));

    clear();
    printf("    .--.\n   |o_o |\n   |:_/ |\n  //   \\ \\\n (|     | )\n/'\\_   _/`\\\n\\___)=(___/  \n\n");

    if(id < NUM_EXCEPTIONS)
        printf("Caught exception: %s (%d)\n\n", exception_names[id], id);

    printf("ESP: %#x \t ESI: %#x\n", regs.esp, regs.esi);
    printf("EBP: %#x \t EDI: %#x\n", regs.ebp, regs.edi);
    printf("EAX: %#x \t EBX: %#x\n", regs.eax, regs.ebx);
    printf("ECX: %#x \t EDX: %#x\n", regs.ecx, regs.edx);
    printf("EFLAGS: %#x\n", flags);
    printf("\nERROR: %#x\n", error);
    
    
    halt(255); // Kill responsible process with error code -1
}
