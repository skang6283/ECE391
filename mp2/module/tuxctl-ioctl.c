/* tuxctl-ioctl.c
 *
 * Driver (skeleton) for the mp2 tuxcontrollers for ECE391 at UIUC.
 *
 * Mark Murphy 2006
 * Andrew Ofisher 2007
 * Steve Lumetta 12-13 Sep 2009
 * Puskar Naha 2013
 */

#include <asm/current.h>
#include <asm/uaccess.h>

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/sched.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/kdev_t.h>
#include <linux/tty.h>
#include <linux/spinlock.h>

#include "tuxctl-ld.h"
#include "tuxctl-ioctl.h"
#include "mtcp.h"

#define debug(str, ...) \
	printk(KERN_DEBUG "%s: " str, __FUNCTION__, ## __VA_ARGS__)


/* fill a buffer with initializing commands and send them to TUX*/
int init(struct tty_struct* tty);
int set_led(struct tty_struct* tty, unsigned long arg);
int set_buttons(struct tty_struct* tty, unsigned long arg);
unsigned char packets [BUTTON_PACKET_SIZE];	/* array for holding packets b and c for MTCP_BIOC_EVENT */
unsigned char prev_state [LED_PACKET_SIZE];	/* array for holding previous state in case of reset */
int ack;																		/* MTCP_ACK flag */

/************************ Protocol Implementation *************************/

/* tuxctl_handle_packet()
 * IMPORTANT : Read the header for tuxctl_ldisc_data_callback() in
 * tuxctl-ld.c. It calls this function, so all warnings there apply
 * here as well.
 */

void tuxctl_handle_packet (struct tty_struct* tty, unsigned char* packet)
{
    unsigned a, b, c;

    a = packet[0]; 	/* byte 0 */
    b = packet[1]; 	/* byte 1 */
    c = packet[2];	/* byte 2 */

		switch(a) {

			case MTCP_RESET:
				init(tty);

				/* send data saved before reset to TUX */
				tuxctl_ldisc_put(tty,prev_state,6);
				break;

			case MTCP_BIOC_EVENT:
			// 		Byte 0 - MTCP_BIOC_EVENT
			// 		byte 1  +-7-----4-+-3-+-2-+-1-+---0---+
			// 			| 1 X X X | C | B | A | START |
			// 			+---------+---+---+---+-------+
			// 		byte 2  +-7-----4-+---3---+--2---+--1---+-0--+
			// 			| 1 X X X | right | down | left | up |
			// 			+---------+-------+------+------+----+
					packets[0] = b;
					packets[1] = c;
					break;

			case MTCP_ACK:
					ack = ACK_ON;
					break;
			default:
				break;

		}

    //printk("packet : %x %x %x\n", a, b, c);
}

/******** IMPORTANT NOTE: READ THIS BEFORE IMPLEMENTING THE IOCTLS ************
 *                                                                            *
 * The ioctls should not spend any time waiting for responses to the commands *
 * they send to the controller. The data is sent over the serial line at      *
 * 9600 BAUD. At this rate, a byte takes approximately 1 millisecond to       *
 * transmit; this means that there will be about 9 milliseconds between       *
 * the time you request that the low-level serial driver send the             *
 * 6-byte SET_LEDS packet and the time the 3-byte ACK packet finishes         *
 * arriving. This is far too long a time for a system call to take. The       *
 * ioctls should return immediately with success if their parameters are      *
 * valid.                                                                     *
 *                                                                            *
 ******************************************************************************/
int
tuxctl_ioctl (struct tty_struct* tty, struct file* file,
	      unsigned cmd, unsigned long arg)
{

    switch (cmd) {

	case TUX_INIT:
			init(tty);
			return 0;

	case TUX_BUTTONS:
			return set_buttons(tty,arg);

	case TUX_SET_LED:
			if(ack == ACK_OFF){
				return 0;
			}
			ack = ACK_OFF;					/* set ack flag to off to wait for another ACK*/
			return set_led(tty, arg);

	case TUX_LED_ACK:
			return -EINVAL;
	case TUX_LED_REQUEST:
			return -EINVAL;
	case TUX_READ_LED:
			return -EINVAL;
	default:
	    return -EINVAL;
    }
}


/*	set_buttons()
*		takes packets value and mask and shift accordingly and send the data to user for behavior.
*		input:  arg -- pointer to a 32bit integer where low byte will be set.
*
*/
int set_buttons(struct tty_struct* tty, unsigned long arg){
	/* variable used for TUX_INIT */

	/* variables used for TUX_BUTTONS */
	unsigned char tmp1;						/* temporary holder for 0000 CBAS  */
	unsigned char tmp2;						/* temporary holder for RDLU 0000 */
	unsigned char bit5;			/* temporary holder for 5th bit */
	unsigned char bit6;			/* temporary holder for 6th bit */
	unsigned char result;	/* final output RLDU CBAS to send to user */

	/* mask packet data and shift accordingly*/
	tmp1 = packets[0] & 0x0F;					/* 0000 CBAS */
	tmp2 = (packets[1] & 0x0F) << 4;	/* RDLU 0000 */

	/* mask 5th and 6th bit and swap them*/
	bit5 = tmp2 & MASK_5;				/* 0010 0000 */
	bit5 = bit5 << 1;
	bit6 = tmp2 & MASK_6;				/* 0100 0000*/
	bit6 = bit6 >> 1;
	tmp2 = tmp2 & MASK_47;			/* 1001 0000 */

	tmp2 = tmp2 | bit5 | bit6; /* RLDU 0000 */
	result = tmp1 | tmp2;			 /* RLDU CBAS */

	/* send data to user and check if arg is NULL */
	if (copy_to_user( (unsigned long*)arg, &result ,sizeof(result)) != 0){
		return -EINVAL;
	}else{
	return 0;
	}
}


/*	set_led()
*		takes argument and mask and shift accordingly and send the data to TUX to set led.
*		input:  arg -- 32 bit interger that consists of led information.
*
*/
int set_led(struct tty_struct* tty, unsigned long arg){

	// 	/* varibles used for TUX_SET_LED */
	//
	// /*
	// *	 	__7___6___5___4____3___2___1___0__
	// * 	| A | E | F | dp | G | C | B | D |
	// *	 	+---+---+---+----+---+---+---+---+
	// *		led hex value from 0-F
	// */
	unsigned char led_bit;													/* determines which led to turn on								 */
	unsigned char dot_bit;													/* determine which dot to turn on 								 */
	int i;																					/* varible used for looping 										   */
	unsigned char led[16] ={0xE7, 0x06, 0xCB, 0x8F, 0x2E, 0xAD, 0xED, 0x86, 0xEF, 0xAE, 0xEE, 0x6D, 0xE1, 0x4F, 0xE9, 0xE8};


  unsigned char output[LED_PACKET_SIZE];					/* buffer that holds final led data to send to TUX */
  unsigned char display_value[DISPLAY_SIZE];			/* buffer that holds display data 								 */


		output[0] = MTCP_LED_SET;

	 /* intialize display with zero*/
	 for(i = 0; i < DISPLAY_SIZE ;i++){
		 output[MIN_LED_BYTES+i] = EMTPY;
	 }

	 /* mask input arg and shift to get led_bit and dot_bit */
	 led_bit = (arg >> 16) & MASK_0TO3;									/* get low 4bits of third byte 		*/
	 dot_bit = (arg >> 24) & MASK_0TO3;									/* get low 4bits of higheset byte */

	 /* save hexadecimal values whose led is set to ON */
	 for(i = 0; i < DISPLAY_SIZE ;i++){
		 display_value[i] = (arg >> (i*4)) & MASK_0TO3;
		 if(((led_bit >> i) & 0x1) == 1){
			 output[i+2] = led[display_value[i]];
		 }
			 if(((dot_bit >> i) & 0x1) == 1){											/* set dots to ON accordingly		 */
					output[i+2] |= DOT_MASK;
			}

	 }
	 output[1] = ALL_LED;

		/* save current state to a buffer for RESET */
		 for(i=0; i<LED_PACKET_SIZE ;i++){
			 prev_state[i] = output[i];
		 }

		 tuxctl_ldisc_put(tty,output, 6);						/* always send all to erase previous data, send the packet to TUX for LED		*/
		 return 0;
}


/*	init()
*		it populates a buffer with MTCP_BIOC_ON and MTCP_LED_USR
*		and send it to TUX to initialize it.
*/
int init(struct tty_struct* tty){
	unsigned char command[INIT_BUF_SIZE];						/* buffer for holding commandsd for initializing */
	ack = ACK_OFF;																	/* set ack flag to 1 to wait for MTCP_ACK				 */

	/* send these commands to initialize a TUX */
	command[0] = MTCP_BIOC_ON;
	command[1] = MTCP_LED_USR;
	tuxctl_ldisc_put(tty, command, INIT_BUF_SIZE);

	return 0;
}
