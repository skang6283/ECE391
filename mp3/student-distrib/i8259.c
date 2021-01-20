/* i8259.c - Functions to interact with the 8259 interrupt controller
 * vim:ts=4 noexpandtab
 */

#include "i8259.h"
#include "lib.h"

/* Interrupt masks to determine which interrupts are enabled and disabled */
uint8_t master_mask = MASK_INIT; /* IRQs 0-7  */
uint8_t slave_mask = MASK_INIT;  /* IRQs 8-15 */


/* codes referenced from linux/i8259.c and O  */

/* void rtc_init(void);
 * Inputs: void
 * Return Value: none
 * Function: Initialize both PICs slave and master */
void i8259_init(void) {

    outb(ICW1, MASTER_8259_CMD_PORT);	            /* ICW1: select 8259 master init */
    outb(ICW1, SLAVE_8259_CMD_PORT);	            /* ICW1: select 8259 slave init */
    
    outb(ICW2_MASTER, MASTER_8259_DATA_PORT);	/* IR0-7 mapped to 0x20 - 0x27 */
    outb(ICW2_SLAVE, SLAVE_8259_DATA_PORT);	    /* IR0-7 mapped to 0x28 - 0x2f */

    outb(ICW3_MASTER, MASTER_8259_DATA_PORT);	/* slave on IR2 */
    outb(ICW3_SLAVE, SLAVE_8259_DATA_PORT);	    /* slave on master's IR2 */
    
    outb(ICW4, MASTER_8259_DATA_PORT);	        /* Select 8086 mode */
    outb(ICW4, SLAVE_8259_DATA_PORT);	        /* Select 8086 mode */

    enable_irq(SLAVE_IRQ_NUM);
 
}

/* void rtc_init(void);
 * Inputs: void
 * Return Value: none
 * Function:  Enable (unmask) the specified IRQ */
void enable_irq(uint32_t irq_num) {
    if(irq_num > MAX_IRQ_NUM){             /* check range of irq_num */
        return;
    }

    if(irq_num < 8){                            /* irq less than 8, master */
        master_mask &= ~(1 << irq_num);
        outb(master_mask, MASTER_8259_DATA_PORT);       
        return;           
    }

    slave_mask &= ~(1 << (irq_num-MAX_MASTER_IRQ_NUM));             /* irq bigger than 8, slave */
    outb(slave_mask, SLAVE_8259_DATA_PORT);       

}

/* void rtc_init(void);
 * Inputs: void
 * Return Value: none
 * Function:  Disable (mask) the specified IRQ */
 void disable_irq(uint32_t irq_num) {
    if(irq_num > MAX_IRQ_NUM){             /* check range of irq_num */
        return;
    }

    if(irq_num < MAX_MASTER_IRQ_NUM){                            /* irq less than 8, master */
        master_mask |= 1 << irq_num;
        outb(master_mask, MASTER_8259_DATA_PORT);       
        return;           
    }

    slave_mask |= 1 << (irq_num-8);             /* irq bigger than 8, slave */
    outb(slave_mask, SLAVE_8259_DATA_PORT);       
   
}

/* void rtc_init(void);
 * Inputs: void
 * Return Value: none
 * Function:  Send end-of-interrupt signal for the specified IRQ */
void send_eoi(uint32_t irq_num) {
    if(irq_num > MAX_IRQ_NUM){             /* check range of irq_num */
        return;
    }

    if(irq_num < MAX_MASTER_IRQ_NUM){                                   /* irq less than 8, master */
        outb(EOI | irq_num, MASTER_8259_CMD_PORT);       
        return;           
    }
    outb(EOI | (irq_num-MAX_MASTER_IRQ_NUM), SLAVE_8259_CMD_PORT);     /* irq bigger than 8, slave */                                     
    outb(EOI | SLAVE_IRQ_NUM, MASTER_8259_CMD_PORT);      
}
