#include "rtc.h"
#include "i8259.h"
#include "lib.h"

#define RTC_INDEX       0x70
#define RTC_CMOS        0x71
#define RTC_REGISTER_A  0x0A
#define RTC_REGISTER_C  0x0C
#define RTC_REGISTER_B  0x0B
#define RTC_IRQ_NUM     8
#define MAX_RATE        15
#define RATE_MASK       0x0F
#define PREV_MASK       0XF0
#define BIT_SIX         0X40
#define MIN_RATE        3
#define MAX_FREQ        1024
#define MIN_FREQ        2

volatile uint32_t rtc_counter = MAX_FREQ / MIN_FREQ;
volatile uint32_t rtc_int = 0;
uint32_t rtc_max_count = MAX_FREQ / MIN_FREQ;

extern void rtc_handler(void);
void rtc_change_rate(int32_t frequency);
char log2(int32_t);

/* void rtc_init(void);
 * Inputs: void
 * Return Value: none
 * Function: initializes rtc */
void rtc_init(void) {
    // Initialization code follows steps from https://wiki.osdev.org/RTC
    unsigned char prev;                 // select register B, (disable NMI is unnecessary for interrupts are not enabled)

    outb(RTC_REGISTER_B, RTC_INDEX);    // select register B, (disable NMI is unnecessary for interrupts are not enabled)
    prev = inb(RTC_CMOS);               // read the current value of register B

    outb(RTC_REGISTER_B,RTC_INDEX);     // set the index again (a read will reset the index to register D)
    outb(prev | BIT_SIX, RTC_CMOS);     //write the previous value ORed with 0x40. This turns on bit 6 of register B

    rtc_max_count = MAX_FREQ / MIN_FREQ;
    enable_irq(RTC_IRQ_NUM);       
    rtc_change_rate(MAX_FREQ);          // Default to maximum rate     
}

/* void rtc_handler(void);
 * Inputs: void
 * Return Value: none
 * Function: read from register C to handle interrupts  */
void rtc_handler(void) {
    unsigned char temp;
    outb(RTC_REGISTER_C,RTC_INDEX);     // select register C
    temp = inb(RTC_CMOS);               // just throw away contents
    (void) temp;

    rtc_counter--;
    if(rtc_counter == 0) {
        rtc_int = 1;
        rtc_counter = rtc_max_count;
    }

    send_eoi(RTC_IRQ_NUM);
}
//upon a IRQ 8,
//Status Register C will contain a bitmask telling which interrupt happened

/* rtc_write
 * Inputs:  fd      - File descriptor number
 *          buf     - Input data pointer
 *          nbytes  - Number of bytes (should be 4)
 * Return Value: 0 on success, -1 on failure
 * Function: Write RTC interrupt rate  */
int32_t rtc_write(int32_t fd, const void* buf, int32_t nbytes) {
    int32_t freq;

    if(buf == NULL) {
        return -1;
    }
    if(nbytes != sizeof(int32_t)) {
        return -1;
    }

    freq = *((int *) buf);

    // Bound the input
    if(freq > MAX_FREQ || freq < MIN_FREQ) {
        return -1;
    }

    // Check for powers of two (not neccesary for virtualized RTC, but may be tested for CP2)
    if((freq & (freq - 1)) != 0) {
        return -1;
    }

    cli();
    rtc_max_count = MAX_FREQ / freq;
    sti();
    return 0;
}

/* rtc_open
 * Inputs:  filename    - String filename
 * Return Value: positive FD number, -1 on failure
 * Function: Open and initialize RTC device  */
int32_t rtc_open(const uint8_t* filename) {
    rtc_max_count = MAX_FREQ / MIN_FREQ;
    return 0;
}

/* rtc_close
 * Inputs:  fd  - File descriptor number
 * Return Value: 0 on success, -1 on failure
 * Function: Close RTC device  */
int32_t rtc_close(int32_t fd) {
    return 0;
}

/* rtc_read
 * Inputs:  fd      - File descriptor number
 *          buf     - Output data pointer
 *          nbytes  - Number of bytes read
 * Return Value: 0 on success, -1 on failure
 * Function: Bock until an RTC int is received  */
int32_t rtc_read(int32_t fd, void* buf, int32_t nbytes) {
    rtc_int = 0;
    while(rtc_int == 0);
    return 0;
}

/* void log2(int32_t num);
 * Inputs: num -- frequency that wants to be changed to
 * Return Value: count -- log2(num)
 * Function: perform log2 operation to input  */
char log2(int32_t num) {
    char count = 0;
    while(num != 1){         // count number of bits in the number by dividing it by 2
        num /= 2;
        count++;
    }
    return count;
}

/* void rtc_change_rate(int32_t frequency);
 * Inputs: frequency -- frequency that wants to be changed to
 * Return Value: none
 * Function: calculate rate according to input frequency and change the frequency of rtc */
void rtc_change_rate(int32_t frequency) {
    char rate = MAX_RATE - log2(frequency) + 1;       // frequency =  32768 >> (rate-1);
    rate &= RATE_MASK;

    if (rate < MIN_RATE){                             // fastest rate selection is 3
        return;                                       // thus if rate is less than 3, return
    }
    cli();
    outb(RTC_REGISTER_A, RTC_INDEX);                  // set index to register A, (disable NMI is unnecessary for interrupts are not enabled)
    char prev = inb(RTC_CMOS);                        // get initial value of register A
    outb(RTC_REGISTER_A, RTC_INDEX);                  // reset index to A
    outb((prev & PREV_MASK) | rate, RTC_CMOS);        // write only our rate to A. Note, rate is the bottom 4 bits.
    sti();
}
