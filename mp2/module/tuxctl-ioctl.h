// All necessary declarations for the Tux Controller driver must be in this file

#ifndef TUXCTL_H
#define TUXCTL_H

#define TUX_SET_LED _IOR('E', 0x10, unsigned long)
#define TUX_READ_LED _IOW('E', 0x11, unsigned long*)
#define TUX_BUTTONS _IOW('E', 0x12, unsigned long*)
#define TUX_INIT _IO('E', 0x13)
#define TUX_LED_REQUEST _IO('E', 0x14)
#define TUX_LED_ACK _IO('E', 0x15)

#define BUTTON_PACKET_SIZE 2
#define INIT_BUF_SIZE   2
#define DISPLAY_SIZE    4
#define DOT_MASK		    0x10
#define LED_PACKET_SIZE 6

#define ACK_ON          0
#define ACK_OFF         1


#define MIN_LED_BYTES   2

#define ALL_LED         0x0F
#define EMTPY           0x00
#define DOT_MASK        0x10     /* 0001 0000 */
#define MASK_0          0x1      /*      0001 */
#define MASK_0TO3       0xF      /*      1111 */
#define MASK_5          0x20     /* 0010 0000 */
#define MASK_6          0x40     /* 0100 0000 */
#define MASK_47         0x90     /* 1001 0000 */

#endif
