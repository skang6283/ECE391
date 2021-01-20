#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/io.h>
#include <linux/rtc.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "mp1.h"

#define WAIT 100
char *vmem_base_addr;
char *mp1_set_video_mode (void);
void print_struct(struct mp1_blink_struct *);
void add_frames(char *, char *, int);

char file0[] = "frame0.txt";
char file1[] = "frame1.txt";

int main(void)
{
	int rtc_fd, ret_val, i, garbage;
	struct mp1_blink_struct blink_struct;


	if(mp1_set_video_mode() == NULL) {
		return -1;
	}

	rtc_fd = open("/dev/rtc", O_RDWR);

	ret_val = ioctl(rtc_fd, RTC_ADD, (unsigned long)0);

	add_frames(file0, file1, rtc_fd);

	ret_val = ioctl(rtc_fd, RTC_IRQP_SET, 16);

	ret_val = ioctl(rtc_fd, RTC_PIE_ON, 0);

	for(i=0; i<WAIT; i++)
		read(rtc_fd, &garbage, 4);

	blink_struct.on_char = 'I';
	blink_struct.off_char = 'M';
	blink_struct.on_length = 7;
	blink_struct.off_length = 6;
	blink_struct.location = 6*80+60;

	ioctl(rtc_fd, RTC_ADD, (unsigned long)&blink_struct);

	for(i=0; i<WAIT; i++)
		read(rtc_fd, &garbage, 4);

	ioctl(rtc_fd, RTC_SYNC, (40 << 16 | (6*80+60)));

	for(i=0; i<WAIT; i++)
		read(rtc_fd, &garbage, 4);

	blink_struct.location = 60;
	ret_val = ioctl(rtc_fd, RTC_FIND, (unsigned long)&blink_struct);

	ioctl(rtc_fd, RTC_REMOVE, 6*80+60);

	for(i=0; i<WAIT; i++)
		read(rtc_fd, &garbage, 4);

	for(i=0; i<80*25; i++)
		ioctl(rtc_fd, RTC_REMOVE, i);

	ret_val = ioctl(rtc_fd, RTC_PIE_OFF, 0);
	close(rtc_fd);
	exit(0);
}

void
add_frames(char *f0, char *f1, int rtc_fd)
{
	int row, col, offset = 40, eof0 = 0, eof1 = 0, num_bytes;
	int fd0, fd1;
	struct mp1_blink_struct blink_struct;
	char c0 = '0', c1 = '0';

	blink_struct.on_length = 15;
	blink_struct.off_length = 15;

	row = 0;

	if( (fd0 = open(f0, O_RDONLY)) < 0 ) {
		perror("open");
		exit(-1);
	}
	if( (fd1 = open(f1, O_RDONLY)) < 0 ) {
		perror("open");
		exit(-1);
	}

	while(eof0 == 0 || eof1 == 0) {
		col = 0;
		while(1) {

			if(c0 != '\n') {
				num_bytes = read(fd0, &c0, 1);
				if(num_bytes == 0) {
					c0 = '\n';
					eof0 = 1;
				}
			}

			if(c1 != '\n') {
				num_bytes = read(fd1, &c1, 1);
				if(num_bytes == 0) {
					c1 = '\n';
					eof1 = 1;
				}
			}

			if(c0 == '\n' && c1 == '\n') {
				break;

			} else {
				if((c0 != ' ' && c0 != '\n') || (c1 != ' ' && c1 != '\n')) {
					blink_struct.on_char = ( (c0 == '\n') ? ' ' : c0);
					blink_struct.off_char = ( (c1 == '\n') ? ' ' : c1);
					blink_struct.location = row*80 + col + offset;
					ioctl(rtc_fd, RTC_ADD, (unsigned long)&blink_struct);
				}
			}
			col++;
		}

		if(eof0) {
			c0 = '\n';
			close(fd0);
		} else {
			c0 = '0';
		}

		if(eof1) {
			c1 = '\n';
			close(fd1);
		} else {
			c1 = '0';
		}

		row++;
	}
}

char*
mp1_set_video_mode (void)
{
	void *mem_image;

	int fd = open ("/dev/mem", O_RDWR);

	if (ioperm (0,1024,1) == -1 || iopl (3) == -1) {
		perror ("set port permissions");
		return NULL;
	}

	if ((mem_image = mmap(0, 1048576, PROT_READ | PROT_WRITE,
					MAP_SHARED, fd, 0)) == MAP_FAILED) {
		perror ("mmap low memory");
		return NULL;
	}

	vmem_base_addr = mem_image + 0xb8000;

	asm volatile (" \
			movw  $0xC, %%ax\n \
			movw  $0x03d4, %%dx\n \
			outw  %%ax,(%%dx)\n \
			movw  $0x0d, %%ax\n \
			outw  %%ax,(%%dx)\n \
			movw  $0xE, %%ax\n \
			outw  %%ax,(%%dx)\n \
			movw  $0x0F,%%ax\n \
			outw  %%ax,(%%dx)"
			: : );

	return vmem_base_addr;
}

void print_struct(struct mp1_blink_struct *mp1_timer_struct)
{
	printf("location: %u\n", mp1_timer_struct->location);
	printf("on_char: %c\n", mp1_timer_struct->on_char);
	printf("off_char: %c\n", mp1_timer_struct->off_char);
	printf("on_length: %u\n", mp1_timer_struct->on_length);
	printf("off_length: %u\n", mp1_timer_struct->off_length);
	printf("countdown: %u\n", mp1_timer_struct->countdown);
	printf("status: 0x%x\n", mp1_timer_struct->status);
	printf("next: 0x%lx\n", (unsigned long)mp1_timer_struct->next);
}
