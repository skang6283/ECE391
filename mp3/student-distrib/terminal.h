#ifndef TERMINAL_H
#define TERMINAL_H


#include "types.h"

#define BUFFER_SIZE   128

//Terminal system call functions. (see terminal.c for descriptions)
extern int32_t terminal_read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t terminal_write(int32_t fd, const void* buf, int32_t nbytes);
extern int32_t terminal_open(const uint8_t* filename);
extern int32_t terminal_close(int32_t fd);

//Connect keybaord to terminal functions. (see terminal.c for descriptions)
extern void get_char(char new_char);

#endif /* TERMINAL_H */
