/*
 * tab:4
 *
 * mazegame.c - main source file for ECE398SSL maze game (F04 MP2)
 *
 * "Copyright (c) 2004 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:        Steve Lumetta
 * Version:       1
 * Creation Date: Fri Sep 10 09:57:54 2004
 * Filename:      mazegame.c
 * History:
 *    SL    1    Fri Sep 10 09:57:54 2004
 *        First written.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include "blocks.h"
#include "maze.h"
#include "modex.h"
#include "text.h"

#include "module/tuxctl-ioctl.h"

// New Includes and Defines
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/io.h>
#include <termios.h>
#include <pthread.h>

#define BACKQUOTE 96
#define UP        65
#define DOWN      66
#define RIGHT     67
#define LEFT      68
#define BLK_SIZE  144
#define LEVEL 10 /* maximum level */

/*
 * If NDEBUG is not defined, we execute sanity checks to make sure that
 * changes to enumerations, bit maps, etc., have been made consistently.
 */
#if defined(NDEBUG)
#define sanity_check() 0
#else
static int sanity_check();
#endif


int fruit_type;           /* variable to distinguish fruit type */
int got_fruit_timer;      /* timer to determine when fruit disappears */
//int tux_time;
/* a few constants */
#define PAN_BORDER      5  /* pan when border in maze squares reaches 5    */
#define MAX_LEVEL       10 /* maximum level number                         */

/* outcome of each level, and of the game as a whole */
typedef enum {GAME_WON, GAME_LOST, GAME_QUIT} game_condition_t;

/* structure used to hold game information */
typedef struct {
    /* parameters varying by level   */
    int number;                  /* starts at 1...                   */
    int maze_x_dim, maze_y_dim;  /* min to max, in steps of 2        */
    int initial_fruit_count;     /* 1 to 6, in steps of 1/2          */
    int time_to_first_fruit;     /* 300 to 120, in steps of -30      */
    int time_between_fruits;     /* 300 to 60, in steps of -30       */
    int tick_usec;         /* 20000 to 5000, in steps of -1750 */

    /* dynamic values within a level -- you may want to add more... */
    unsigned int map_x, map_y;   /* current upper left display pixel */
} game_info_t;

static game_info_t game_info;

/* local functions--see function headers for details */
static int prepare_maze_level(int level);
static void move_up(int* ypos);
static void move_right(int* xpos);
static void move_down(int* ypos);
static void move_left(int* xpos);
static int unveil_around_player(int play_x, int play_y);
static void *rtc_thread(void *arg);
static void *keyboard_thread(void *arg);

static void *tux_thread(void *arg);
static void show_time();
/*
 * prepare_maze_level
 *   DESCRIPTION: Prepare for a maze of a given level.  Fills the game_info
 *          structure, creates a maze, and initializes the display.
 *   INPUTS: level -- level to be used for selecting parameter values
 *   OUTPUTS: nonef
 *   RETURN VALUE: 0 on success, -1 on failure
 *   SIDE EFFECTS: writes entire game_info structure; changes maze;
 *                 initializes display
 */
static int prepare_maze_level(int level) {
    int i; /* loop index for drawing display */

    /*
     * Record level in game_info; other calculations use offset from
     * level 1.
     */
    game_info.number = level--;

    /* Set per-level parameter values. */
    if ((game_info.maze_x_dim = MAZE_MIN_X_DIM + 2 * level) > MAZE_MAX_X_DIM)
        game_info.maze_x_dim = MAZE_MAX_X_DIM;
    if ((game_info.maze_y_dim = MAZE_MIN_Y_DIM + 2 * level) > MAZE_MAX_Y_DIM)
        game_info.maze_y_dim = MAZE_MAX_Y_DIM;
    if ((game_info.initial_fruit_count = 1 + level / 2) > 6)
        game_info.initial_fruit_count = 6;
    if ((game_info.time_to_first_fruit = 300 - 30 * level) < 120)
        game_info.time_to_first_fruit = 120;
    if ((game_info.time_between_fruits = 300 - 60 * level) < 60)
        game_info.time_between_fruits = 60;
    if ((game_info.tick_usec = 20000 - 1750 * level) < 5000)
        game_info.tick_usec = 5000;

    /* Initialize dynamic values. */
    game_info.map_x = game_info.map_y = SHOW_MIN;

    /* Create a maze. */
    if (make_maze(game_info.maze_x_dim, game_info.maze_y_dim, game_info.initial_fruit_count) != 0)
        return -1;

    /* Set logical view and draw initial screen. */
    set_view_window(game_info.map_x, game_info.map_y);
    for (i = 0; i < SCROLL_Y_DIM; i++)
        (void)draw_horiz_line (i);

    /* Return success. */
    return 0;
}

/*
 * move_up
 *   DESCRIPTION: Move the player up one pixel (assumed to be a legal move)
 *   INPUTS: ypos -- pointer to player's y position (pixel) in the maze
 *   OUTPUTS: *ypos -- reduced by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void move_up(int* ypos) {
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the upper pan border
     * while the top pixels of the maze are not on-screen.
     */
    if (--(*ypos) < game_info.map_y + BLOCK_Y_DIM * PAN_BORDER && game_info.map_y > SHOW_MIN) {
        /*
         * Shift the logical view upwards by one pixel and draw the
         * new line.
         */
        set_view_window(game_info.map_x, --game_info.map_y);
        (void)draw_horiz_line(0);
    }
}

/*
 * move_right
 *   DESCRIPTION: Move the player right one pixel (assumed to be a legal move)
 *   INPUTS: xpos -- pointer to player's x position (pixel) in the maze
 *   OUTPUTS: *xpos -- increased by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void move_right(int* xpos) {
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the right pan border
     * while the rightmost pixels of the maze are not on-screen.
     */
    if (++(*xpos) > game_info.map_x + SCROLL_X_DIM - BLOCK_X_DIM * (PAN_BORDER + 1) &&
        game_info.map_x + SCROLL_X_DIM < (2 * game_info.maze_x_dim + 1) * BLOCK_X_DIM - SHOW_MIN) {
        /*
         * Shift the logical view to the right by one pixel and draw the
         * new line.
         */
        set_view_window(++game_info.map_x, game_info.map_y);
        (void)draw_vert_line(SCROLL_X_DIM - 1);
    }
}

/*
 * move_down
 *   DESCRIPTION: Move the player right one pixel (assumed to be a legal move)
 *   INPUTS: ypos -- pointer to player's y position (pixel) in the maze
 *   OUTPUTS: *ypos -- increased by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void move_down(int* ypos) {
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the right pan border
     * while the bottom pixels of the maze are not on-screen.
     */
    if (++(*ypos) > game_info.map_y + SCROLL_Y_DIM - BLOCK_Y_DIM * (PAN_BORDER + 1) &&
        game_info.map_y + SCROLL_Y_DIM < (2 * game_info.maze_y_dim + 1) * BLOCK_Y_DIM - SHOW_MIN) {
        /*
         * Shift the logical view downwards by one pixel and draw the
         * new line.
         */
        set_view_window(game_info.map_x, ++game_info.map_y);
        (void)draw_horiz_line(SCROLL_Y_DIM - 1);
    }
}

/*
 * move_left
 *   DESCRIPTION: Move the player right one pixel (assumed to be a legal move)
 *   INPUTS: xpos -- pointer to player's x position (pixel) in the maze
 *   OUTPUTS: *xpos -- decreased by one from initial value
 *   RETURN VALUE: none
 *   SIDE EFFECTS: pans display by one pixel when appropriate
 */
static void move_left(int* xpos) {
    /*
     * Move player by one pixel and check whether display should be panned.
     * Panning is necessary when the player moves past the left pan border
     * while the leftmost pixels of the maze are not on-screen.
     */
    if (--(*xpos) < game_info.map_x + BLOCK_X_DIM * PAN_BORDER && game_info.map_x > SHOW_MIN) {
        /*
         * Shift the logical view to the left by one pixel and draw the
         * new line.
         */
        set_view_window(--game_info.map_x, game_info.map_y);
        (void)draw_vert_line (0);
    }
}

/*
 * unveil_around_player
 *   DESCRIPTION: Show the maze squares in an area around the player.
 *                Consume any fruit under the player.  Check whether
 *                player has won the maze level.
 *   INPUTS: (play_x,play_y) -- player coordinates in pixels
 *   OUTPUTS: none
 *   RETURN VALUE: 1 if player wins the level by entering the square
 *                 0 if not
 *   SIDE EFFECTS: draws maze squares for newly visible maze blocks,
 *                 consumed fruit, and maze exit; consumes fruit and
 *                 updates dFsetisplayed fruit counts
 */
static int unveil_around_player(int play_x, int play_y) {
    int x = play_x / BLOCK_X_DIM; /* player's maze lattice position */
    int y = play_y / BLOCK_Y_DIM;
    int i, j;            /* loop indices for unveiling maze squares */

    /* Check for fruit at the player's position. */
    fruit_type = check_for_fruit (x, y);

    if(fruit_type !=0){
      got_fruit_timer = 128;
    }
    /* Unveil spaces around the player. */
    for (i = -1; i < 2; i++)
        for (j = -1; j < 2; j++)
            unveil_space(x + i, y + j);
        unveil_space(x, y - 2);
        unveil_space(x + 2, y);
        unveil_space(x, y + 2);
        unveil_space(x - 2, y);

    /* Check whether the player has won the maze level. */
    return check_for_win (x, y);
}

#ifndef NDEBUG
/*
 * sanity_check
 *   DESCRIPTION: Perform checks on changes to constants and enumerated values.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0 if checks pass, -1 if any fail
 *   SIDE EFFECTS: none
 */
static int sanity_check() {
    /*
     * Automatically detect when fruits have been added in blocks.h
     * without allocating enough bits to identify all types of fruit
     * uniquely (along with 0, which means no fruit).
     */
    if (((2 * LAST_MAZE_FRUIT_BIT) / MAZE_FRUIT_1) < NUM_FRUIT_TYPES + 1) {
        puts("You need to allocate more bits in maze_bit_t to encode fruit.");
        return -1;
    }
    return 0;
}
#endif /* !defined(NDEBUG) */

// Shared Global Variables
int quit_flag = 0;
int winner= 0;
int next_dir = UP;
int play_x, play_y, last_dir, dir;
int move_cnt = 0;
int fd;
int ft;
unsigned long data;
static struct termios tio_orig;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

/* condition variable to prevent delay */
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;

int buttons_pressed =0 ;        /* shared variable to determine whether button is pressed or not */
unsigned char directions;     /* shared variable to determine direction */



/*
 * tux_thread
 *   DESCRIPTION: Thread that handles TUX INPUTS
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void *tux_thread(void *arg){
  while(winner == 0){
      if(quit_flag == 1){
        break;
      }

      pthread_mutex_lock(&mtx);

      /* if button is not pressed, release the lock and wait for condition varibale signal */
      while(!buttons_pressed){
        pthread_cond_wait(&cv, &mtx);
      }

      switch(directions){
        case TUX_BUTTON_LEFT:
          next_dir = DIR_LEFT;
          break;
        case TUX_BUTTON_DOWN:
          next_dir = DIR_DOWN;
          break;
        case TUX_BUTTON_RIGHT:
          next_dir = DIR_RIGHT;
          break;
        case TUX_BUTTON_UP:
          next_dir = DIR_UP;
          break;
        default:
          break;
      }
      pthread_mutex_unlock(&mtx);
  }
  return 0;
}


/*
 * show_time
 *   DESCRIPTION: convert time to send data to TUX LED to be displayed
 *   INPUTS: t -- time to be displayed on TUX LED
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void show_time(int t){
  unsigned long tux_time = t;
  unsigned long led_input = DEFAULT_LED_VALUE;            /* 0X040F0000 */

  unsigned long second = tux_time % 60;                   /* seconds  */
  unsigned long second1 = second % 10;                    /* 1st      */
  unsigned long second10 = second / 10;                   /* 10th     */

  unsigned long minute = tux_time / 60;                   /* minutes  */
  unsigned long minute1 = minute % 10;                    /* 1st      */
  unsigned long minute10 = minute / 10;                   /* 10th     */


  /* shift bits to put them in right position */
  led_input = led_input | (minute10 << 12);
  led_input = led_input | (minute1 << 8);
  led_input = led_input | (second10 << 4);
  led_input = led_input | second1;

  //if( t % 32 == 0){
    ioctl(ft,TUX_SET_LED,led_input);
  //}

}

/*
 * keyboard_thread
 *   DESCRIPTION: Thread that handles keyboard inputs
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void *keyboard_thread(void *arg) {
    char key;
    int state = 0;
    // Break only on win or quit input - '`'
    while (winner == 0) {
        // Get Keyboard Input
        key = getc(stdin);

        // Check for '`' to quit
        if (key == BACKQUOTE) {
            quit_flag = 1;
            break;
        }

        // Compare and Set next_dir
        // Arrow keys deliver 27, 91, ##
        if (key == 27) {
            state = 1;
        }
        else if (key == 91 && state == 1) {
            state = 2;
        }
        else {
            if (key >= UP && key <= LEFT && state == 2) {
                pthread_mutex_lock(&mtx);
                switch(key) {
                    case UP:
                        //if(!buttons_pressed){       /* prioritize TUX */
                          next_dir = DIR_UP;
                        //}
                        break;
                    case DOWN:
                      //if(!buttons_pressed){           /* prioritize TUX */
                        next_dir = DIR_DOWN;
                      //}
                        break;
                    case RIGHT:
                      //if(!buttons_pressed){           /* prioritize TUX */
                        next_dir = DIR_RIGHT;
                      //}
                        break;
                    case LEFT:
                      //if(!buttons_pressed){           /* prioritize TUX */
                        next_dir = DIR_LEFT;
                      //}
                        break;
                }
                pthread_mutex_unlock(&mtx);
            }
            state = 0;
        }
    }

    return 0;
}

/* some stats about how often we take longer than a single timer tick */
static int goodcount = 0;
static int badcount = 0;
static int total = 0;

/*
 * rtc_thread
 *   DESCRIPTION: Thread that handles updating the screen
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
static void *rtc_thread(void *arg) {
    int ticks = 0;
    int level;
    int ret;
    int open[NUM_DIRS];
    int goto_next_level = 0;
    int color_idx =0;
    int n_fruits;                                   /* constant to keep track of number of fruits*/
    int time=0;                                       /* constant to keep track of time*/
    int time_refresh = 0;                           /* constant used to refesh time when level is changed*/
    unsigned char old_block_buffer[BLK_SIZE];       /* buffer to save the old background of the block to draw on */
    int xbuffer =0;                                 /* old x position holder */
    int ybuffer =0;                                 /* old y position holder */

    int head_idx =0;                                /* index to change color on player's head */
    int head_cycle =0;                              /* cycle time to change color on player's head */

    int fruit_xbuffer=0;                            /* buffer to hold x coordinate where string will be drawn */
    int fruit_ybuffer =0;                           /* buffer to hold y coordinate where string will be drawn */
    int old_fruit_xbuffer=0;                        /* buffer to hold old x coordinate where string will be drawn */
    int old_fruit_ybuffer=0;                        /* buffer to hold old y coordinate where string will be drawn */


    unsigned char old_fruit_buffer[TXT_X_DIM*TXT_Y_DIM];  /* buffer that holds background data TXT_X_DIM by TXT_Y_DIM */
    unsigned char fruit_txt_buffer[TXT_X_DIM*TXT_Y_DIM];  /* buffer that holds string data drawn on TXT_X_DIM by TXT_Y_DIM image */

    unsigned char tr;                               /* transparent r value */
    unsigned char tg;                               /* transparent g value */
    unsigned char tb;                               /* transparent b value */

    char* fruit;                                    /* pointer to string data to be drawn */



    /* buffer that holds strings data */
    char* fruit_txt[NUM_FRUITS] ={"an apple!", "a grape!","a peach!","a strawberry!","a banana!","a watermelon!","a Dew!"};

    /* buffer that holds color value for status bar */
    int status_bar_colors[LEVEL] = {0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A};

    /* buffer that holds r values that will change per level */
    int r[LEVEL] ={0x3F,0x0A,0x00,0x0F,0x00,0x3E,0x07,0x08,0x09,0x3F};
    /* buffer that holds g values that will change per level */
    int g[LEVEL] ={0x00,0x0A,0x3F,0x07,0x00,0x0F,0x1F,0x3E,0x0A,0x03};
    /* buffer that holds b values that will change per level */
    int b[LEVEL] ={0x00,0x0A,0x00,0x0A,0x3F,0x0F,0x03,0x04,0x3F,0x06};


    // Loop over levels until a level is lost or quit.
    for (level = 1; (level <= MAX_LEVEL) && (quit_flag == 0); level++) {
        // Prepare for the level.  If we fail, just let the player win.
        if (prepare_maze_level(level) != 0)
            break;
        goto_next_level = 0;

        // Start the player at (1,1)
        play_x = BLOCK_X_DIM;
        play_y = BLOCK_Y_DIM;

        // move_cnt tracks moves remaining between maze squares.
        // When not moving, it should be 0.
        move_cnt = 0;

        // Initialize last direction moved to up
        last_dir = DIR_UP;

        // Initialize the current direction of motion to stopped
        dir = DIR_STOP;
        next_dir = DIR_STOP;

        /* change color palette where WALL  WALL FILL depends on current level */
        color_idx = level-1;
        //set_palette_colors(WALL_OUTLINE_COLOR,r[color_idx],g[color_idx],b[color_idx]);
        set_palette_colors(WALL_FILL_COLOR,r[color_idx],g[color_idx],b[color_idx]);

        /* calculate transparent color and change color palette */
        tr = (r[color_idx] + WHITE)/2;
        tg = (g[color_idx] + WHITE)/2;
        tb = (b[color_idx] + WHITE)/2;

        //set_palette_colors(WALL_OUTLINE_COLOR+0x40,tr,tg,tb);
        set_palette_colors(WALL_FILL_COLOR+T_OFFSET,tr,tg,tb);


        // Show maze around the player's original position
        (void)unveil_around_player(play_x, play_y);

        /*store old position of the player*/
        xbuffer = play_x;
        ybuffer = play_y;

        // get first Periodic Interrupt
        ret = read(fd, &data, sizeof(unsigned long));

        while ((quit_flag == 0) && (goto_next_level == 0)) {
            // Wait for Periodic Interrupt
            ret = read(fd, &data, sizeof(unsigned long));


            /* execute show_time every second to prevent spamming */


            // Update tick to keep track of time.  If we missed some
            // interrupts we want to update the player multiple times so
            // that player velocity is smooth
            ticks = data >> 8;

            total += ticks;
            time_refresh +=ticks;

            /* change head color every half a second for 10 cycles*/
            head_cycle = time_refresh/HALF_SEC;
            head_idx = head_cycle % NUM_HEAD_CYCLES;
            show_status_bar(level, n_fruits,time,status_bar_colors[level]);

            set_palette_colors(PLAYER_CENTER_COLOR,g[head_idx],b[head_idx],r[head_idx]);
            tr = (r[color_idx] + WHITE)/2;
            tg = (g[color_idx] + WHITE)/2;
            tb = (b[color_idx] + WHITE)/2;
            set_palette_colors(PLAYER_CENTER_COLOR+0x40,tg,tb,tr);


            time = time_refresh / SEC;
            /* prevent spamming */
            if(time_refresh % SEC < 2){
              show_time(time);
            }

            n_fruits = get_fruit_num();

            // If the system is completely overwhelmed we better slow down:
            if (ticks > 8) ticks = 8;

            if (ticks > 1) {
                badcount++;
            }
            else {
                goodcount++;
            }

            unsigned long buttons;
            while (ticks--) {

                // Lock the mutex
                //pthread_mutex_lock(&mtx);

                ioctl(ft,TUX_BUTTONS, &buttons);
                directions = buttons & 0xFF;

                if(directions != 0xFF){
                  buttons_pressed = 1;
                }else{
                  buttons_pressed = 0;
                }

                pthread_mutex_lock(&mtx);

                /* if button is pressed, send signal to cv */
                if(buttons_pressed){
                  pthread_cond_signal(&cv);
                }
                pthread_mutex_unlock(&mtx);

                pthread_mutex_lock(&mtx);
                // Check to see if a key has been pressed
                if (next_dir != dir) {
                    // Check if new direction is backwards...if so, do immediately
                    if ((dir == DIR_UP && next_dir == DIR_DOWN) ||
                        (dir == DIR_DOWN && next_dir == DIR_UP) ||
                        (dir == DIR_LEFT && next_dir == DIR_RIGHT) ||
                        (dir == DIR_RIGHT && next_dir == DIR_LEFT)) {
                        if (move_cnt > 0) {
                            if (dir == DIR_UP || dir == DIR_DOWN)
                                move_cnt = BLOCK_Y_DIM - move_cnt;
                            else
                                move_cnt = BLOCK_X_DIM - move_cnt;
                        }
                        dir = next_dir;
                    }
                }

                // New Maze Square!
                if (move_cnt == 0) {
                    // The player has reached a new maze square; unveil nearby maze
                    // squares and check whether the player has won the level.
                    if (unveil_around_player(play_x, play_y)) {
                        pthread_mutex_unlock(&mtx);
                        goto_next_level = 1;
                        time_refresh = 0;
                        got_fruit_timer =0;
                        break;
                    }

                    // Record directions open to motion.
                    find_open_directions (play_x / BLOCK_X_DIM, play_y / BLOCK_Y_DIM, open);

                    // Change dir to next_dir if next_dir is open
                    if (open[next_dir]) {
                        dir = next_dir;
                    }

                    // The direction may not be open to motion...
                    //   1) ran into a wall
                    //   2) initial direction and its opposite both face walls
                    if (dir != DIR_STOP) {
                        if (!open[dir]) {
                            dir = DIR_STOP;
                        }
                        else if (dir == DIR_UP || dir == DIR_DOWN) {
                            move_cnt = BLOCK_Y_DIM;
                        }
                        else {
                            move_cnt = BLOCK_X_DIM;
                        }
                    }
                }

                // Unlock the mutex
                pthread_mutex_unlock(&mtx);

                if (dir != DIR_STOP) {
                    // move in chosen direction
                    last_dir = dir;
                    move_cnt--;
                    switch (dir) {
                        case DIR_UP:
                            move_up(&play_y);
                            break;
                        case DIR_RIGHT:
                            move_right(&play_x);
                            break;
                        case DIR_DOWN:
                            move_down(&play_y);
                            break;
                        case DIR_LEFT:
                            move_left(&play_x);
                            break;
                    }

                }
            }


                /* save location of blocks to copy */
                xbuffer = play_x;
                ybuffer = play_y;

                /*draw masked player image on the address*/
                draw_mask(play_x,play_y,get_player_mask(last_dir), get_player_block(last_dir), old_block_buffer);

                /* save location where string will be drawn */
                fruit_xbuffer = play_x - TXT_X_OFFSET;
                fruit_ybuffer = play_y - TXT_Y_DIM;

                /* check if player got fruit, save it to fruit if it did */
                if(fruit_type !=0){
                  fruit = fruit_txt[fruit_type-1];
                }
                /* if player got the fruit, start draw procedure*/
                if(got_fruit_timer > 0){

                  /* prevent string from going out of top frame */
                  if(fruit_ybuffer <= TXT_Y_LIMIT){
                    fruit_ybuffer = TXT_Y_LIMIT;
                  }

                  /* save the old string location */
                  old_fruit_xbuffer = fruit_xbuffer;
                  old_fruit_ybuffer = fruit_ybuffer;

                  /* save block of image to old_fruit_buffer and fruit_txt_buffer */
                  draw_fruit_txt(fruit_xbuffer, fruit_ybuffer, old_fruit_buffer,fruit_txt_buffer);
                  /* write string to fruit_txt_buffer*/
                  string_to_buf_fruit(fruit,fruit_txt_buffer);
                  /* draw fruit_txt_buffer which contains txt to vga buffer */
                  draw_string(fruit_xbuffer,fruit_ybuffer,fruit_txt_buffer);

                }
                /* show it to screen */
                show_screen();

                /* cover the string image with old background and decrement fruit timer */
                if(got_fruit_timer > 0){
                  draw_string(old_fruit_xbuffer,old_fruit_ybuffer,old_fruit_buffer);
                  got_fruit_timer--;
                }

                /*cover the player image with old background image*/
                draw_full_block(xbuffer, ybuffer, old_block_buffer);
        }
    }
    if (quit_flag == 0)
        winner = 1;

    return 0;
}


/*
 * main
 *   DESCRIPTION: Initializes and runs the two threads
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: 0 on success, -1 on failure
 *   SIDE EFFECTS: none
 */
int main() {
    int ret;
    struct termios tio_new;
    unsigned long update_rate = 32; /* in Hz */

    pthread_t tid1;
    pthread_t tid2;
    pthread_t tid3; //tux_thread

    // Initialize RTC
    fd = open("/dev/rtc", O_RDONLY, 0);

    ft = open("/dev/ttyS0", O_RDWR | O_NOCTTY);
    int ldisc_num = N_MOUSE;
    ioctl(ft, TIOCSETD, &ldisc_num);
    ioctl(ft, TUX_INIT,0);

    // Enable RTC periodic interrupts at update_rate Hz
    // Default max is 64...must change in /proc/sys/dev/rtc/max-user-freq
    ret = ioctl(fd, RTC_IRQP_SET, update_rate);
    ret = ioctl(fd, RTC_PIE_ON, 0);

    // Initialize Keyboard
    // Turn on non-blocking mode
    if (fcntl(fileno(stdin), F_SETFL, O_NONBLOCK) != 0) {
        perror("fcntl to make stdin non-blocking");
        return -1;
    }

    // Save current terminal attributes for stdin.
    if (tcgetattr(fileno(stdin), &tio_orig) != 0) {
        perror("tcgetattr to read stdin terminal settings");
        return -1;
    }

    // Turn off canonical (line-buffered) mode and echoing of keystrokes
    // Set minimal character and timing parameters so as
    tio_new = tio_orig;
    tio_new.c_lflag &= ~(ICANON | ECHO);
    tio_new.c_cc[VMIN] = 1;
    tio_new.c_cc[VTIME] = 0;
    if (tcsetattr(fileno(stdin), TCSANOW, &tio_new) != 0) {
        perror("tcsetattr to set stdin terminal settings");
        return -1;
    }

    // Perform Sanity Checks and then initialize input and display
    if ((sanity_check() != 0) || (set_mode_X(fill_horiz_buffer, fill_vert_buffer) != 0)){
        return 3;
    }

    // Create the threads
    pthread_create(&tid1, NULL, rtc_thread, NULL);
    pthread_create(&tid2, NULL, keyboard_thread, NULL);
    pthread_create(&tid3, NULL, tux_thread, NULL);


    // Wait for all the threads to end
    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);
    pthread_cancel(tid3); /* terminate TUX thread */

    // Shutdown Display
    clear_mode_X();

    // Close Keyboard
    (void)tcsetattr(fileno(stdin), TCSANOW, &tio_orig);

    // Close RTC
    close(fd);

    // close tux
    close(ft);

    // Print outcome of the game
    if (winner == 1) {
        printf("You win the game! CONGRATULATIONS!\n");
    }
    else if (quit_flag == 1) {
        printf("Quitter!\n");
    }
    else {
        printf ("Sorry, you lose...\n");
    }

    // Return success
    return 0;
}
