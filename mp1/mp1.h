#ifdef _USERSPACE

/* Alias the ioctl and open system calls to our own */
#define ioctl __mp1_ioctl
#define open __mp1_open

/* Stub out the kernel calls since we're in userspace */
#define copy_to_user __mp1_copy_to_user
#define copy_from_user __mp1_copy_from_user
#define __user 

#endif

#ifndef _ASM
#include <linux/rtc.h>
#include "mp1_userspace.h"
#define RTC_ADD _IOR('p', 0x13, struct mp1_blink_struct *) 
#define RTC_REMOVE _IOR('p', 0x14, unsigned long)  
#define RTC_FIND _IOW('p', 0x15, struct mp1_blink_struct *) 
#define RTC_SYNC _IOR('p', 0x16, unsigned long) 

struct mp1_blink_struct {
	unsigned short location;
	char on_char; 
	char off_char;
	unsigned short on_length;
	unsigned short off_length;
	unsigned short countdown;
	unsigned short status;
	struct mp1_blink_struct* next;
} __attribute((packed)); 

#endif
