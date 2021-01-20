#include "terminal.h"
#include "keyboard.h"
#include "i8259.h"
#include "lib.h"

#define BCKSPACE    0x08

//Holds the values entered by the keyboard
static char char_buffer[BUFFER_SIZE];
//Flag to break out of read loop.
volatile static int enter_flag = 0;
//Keeps track of the current location to be filled in the char_buffer
static int char_count = 0;


/* int32_t terminal_read(int32_t fd, void* buf, int32_t nbytes);
 * Reads in keyboard presses and stores it in the char_buffer
 * until the newline is entered. It then stores the resulting char_buffer
 * in the buf array entered by the user.
 *
 * Inputs: fd - The file descriptor value.
 *        buf - The array that terminal read will be storing the entered
 *              keyboard text to.
 *     nbytes - The maximum number of characters that can be entered in buf.
 * Return Value: The number of bytes (characters) written to the buf array.
 * Side effects: char_buffer, char_count, and enter_flag are changed */
int32_t terminal_read(int32_t fd, void* buf, int32_t nbytes) {
    int bytes_read = 0;
    int i;

    //Fills in the buffer as the keyboard interrupts. Exits after newline.
    while(enter_flag == 0){}

    cli();
    //The max size of the buffer returned ranges from 1 to 128.
    if(nbytes < BUFFER_SIZE) {
        for(i = 0; i < nbytes; ++i) {
            //Fills in the user entered buffer with the resulting characters.
            ((char*) buf)[i] = char_buffer[i];
            char_buffer[i] = ' ';                   //Clears char_buffer
            //If it is smaller than nbytes it finishes here.
            if(((char*)buf)[i] == '\n') {
                bytes_read = i + 1;
                break;
            }
            //Makes sure that the last character in buf is a newline
            if((i == (nbytes - 1)) && (((char*)buf)[i] != '\n')){
                ((char*) buf)[i] = '\n';
                bytes_read = i + 1;
                break;
            }
        }
    } else {
        for(i = 0; i < BUFFER_SIZE; ++i) {
            //Fill in the user entered buffer
            ((char*) buf)[i] = char_buffer[i];
            char_buffer[i] = ' ';               //Clear char_buffer
            if(((char*)buf)[i] == '\n') {
                bytes_read = i + 1;
                break;
            }
        }
    }
    char_count = 0;  //Go back to the start of the char_buffer.
    enter_flag = 0;
    sti();

    return bytes_read;
}

/* int32_t terminal_write(int32_t fd, const void* buf, int32_t nbytes);
 * Outputs the data from the user given buf to video memory.
 * Inputs: fd - The file descriptor value.
 *        buf - The array that terminal write will be getting the values
 *              to print to video memory.
 *     nbytes - The number of characters to print from buf.
 * Return Value: The number of bytes (characters) written to video memory.
 * Side effects: char_buffer, char_count, and enter_flag are changed */
int32_t terminal_write(int32_t fd, const void* buf, int32_t nbytes){
    int i;
    char curr_char;
    for(i = 0; i < nbytes; ++i) {
        //Prints the given characters to the screen.
        curr_char = ((char*) buf)[i];
        if(curr_char != '\0')           //Skips printing out the null terminator
            putc(curr_char);
    }
    return nbytes;
}

/* int32_t terminal_open(const uint8_t* filename);
 * Initializes things specific to the terminal, or return 0.
 *
 * Inputs: filename - The name of the file to initialize from.
 * Return Value: 0 for success in initialization. 1 for failure.
 * Side effects: none */
int32_t terminal_open(const uint8_t* filename) {
    return 0;
}

/* int32_t terminal_close(int32_t fd);
 * Clears things specific to the terminal, or return 0.
 *
 * Inputs: fd - The file descriptor value.
 * Return Value: 0 for success in initialization. 1 for failure.
 * Side effects: none */
int32_t terminal_close(int32_t fd) {
    return 0;
}


/* void get_char(char new_char);
 * Puts the most recently entered keyboard character into the char_buffer
 * for terminal_read and updates if enter has been pressed.
 *
 * Inputs: new_char - The character entered by the keyboard
 * Return Value: none
 * Side effects: char_buffer, char_count, and enter_flag are changed */
void get_char(char new_char) {
    //End the buffer with the newline and enable the enter_flag.
    if(new_char == '\n') {
        enter_flag = 1;
        if(char_count >= BUFFER_SIZE)
            char_buffer[BUFFER_SIZE - 1] = '\n';
        else
            char_buffer[char_count] = '\n';
    //Clear one space of the buffer.
    } else if(new_char == BCKSPACE) {
        if(char_count > 0){
            if(char_count <= BUFFER_SIZE)
                char_buffer[char_count - 1] = ' ';
            --char_count;
        }
    //Add a new character to the buffer.
    } else if(char_count < (BUFFER_SIZE - 1)){
        char_buffer[char_count] = new_char;
        ++char_count;
    }
    //Keep track of the available number of backspaces that can be used.
    else{
        ++char_count;
    }
}
