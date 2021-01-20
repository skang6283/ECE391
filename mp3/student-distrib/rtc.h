/* rtc.h - Defines used in interactions with the rtc interrupt
 * controller
 */


#ifndef RTC_H
#define RTC_H

#include "types.h"

// initialize rtc 
void rtc_init(void);

// write rtc frequency
int32_t rtc_write(int32_t fd, const void* buf, int32_t nbytes);

// blocking read till rtc interrupt occurs
int32_t rtc_read(int32_t fd, void* buf, int32_t nbytes);

// open the rtc device
int32_t rtc_open(const uint8_t* filename);

// close the rtc device
int32_t rtc_close(int32_t fd);

#endif /* RTC_H */
