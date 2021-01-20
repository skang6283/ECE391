/*
 * tab:4
 *
 * maze.c - maze generation and display functions
 *
 * "Copyright (c) 2004-2009 by Steven S. Lumetta."
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
 * Version:       3
 * Creation Date: Fri Sep 10 09:58:42 2004
 * Filename:      maze.c
 * History:
 *    SL    1    Fri Sep 10 09:58:42 2004
 *        First written.
 *    SL    2    Sat Sep 12 14:17:45 2009
 *        Integrated original release back into main code base.
 *    SL    3    Sat Sep 12 18:41:31 2009
 *        Integrated Nate Taylor's "god mode."
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blocks.h"
#include "input.h"
#include "maze.h"
#include "modex.h"

/* Set to 1 to test maze generation routines. */
#define TEST_MAZE_GEN 0

/* Set to 1 to remove all walls as a debugging aid. (Nate Taylor, S07). */
#define GOD_MODE 1

/* local functions--see function headers for details */
static int mark_maze_area(int x, int y);
static void add_a_fruit_internal();
#if (TEST_MAZE_GEN == 0) /* not used when testing maze generation */
static unsigned char* find_block(int x, int y);
static void _add_a_fruit(int show);
#endif

/*
 * The maze array contains a one byte bit vector (maze_bit_t) for each
 * location in a maze.  The left and right boundaries of the maze are unified,
 * i.e., column 0 also forms the right boundary via wraparound.  Drawings
 * for each block in the maze are chosen based on a five-point stencil
 * that includes north, east, south, and west neighbor blocks.  For
 * simplicity, the maze array is extended with additional rows on the top
 * and bottom to avoid boundary conditions.
 *
 * Under these assumptions, the upper left boundary of the maze is at (0,1),
 * and the lower right boundary is at (2 X_DIM, 2 Y_DIM + 1).  The stencil
 * calculation for the lower right boundary includes the point below it,
 * which is (2 X_DIM, 2 Y_DIM + 2).  As the width of the maze is 2 X_DIM,
 * the index of this point is 2 X_DIM + (2 Y_DIM + 2) * 2 X_DIM, or
 * 2 X_DIM (2 Y_DIM + 3), and the space allocated is one larger than this
 * maximum index value.
 */
static unsigned char maze[2 * MAZE_MAX_X_DIM * (2 * MAZE_MAX_Y_DIM + 3) + 1];
static int maze_x_dim;          /* horizontal dimension of maze */
static int maze_y_dim;          /* vertical dimension of maze   */
static int n_fruits;            /* number of fruits in maze     */
static int exit_x, exit_y;      /* lattice point of maze exit   */

/*
 * maze array index calculation macro; maze dimensions are valid only
 * after a call to make_maze
 */
#define MAZE_INDEX(a,b) ((a) + ((b) + 1) * maze_x_dim * 2)

/*
 * mark_maze_area
 *   DESCRIPTION: Uses a breadth-first search to marks all parts of the
 *                maze accessible from (x,y) with the MAZE_REACH bit.
 *                Stops at walls and at maze locations already marked
 *                as reached.
 *   INPUTS: (x,y) -- starting coordinate within maze
 *   OUTPUTS: none
 *   RETURN VALUE: number of maze locations marked
 *   SIDE EFFECTS: leaves MAZE_REACH markers on marked portion of maze
 */
static int mark_maze_area(int x, int y) {
    /*
     * queue for breadth-first search
     *
     * The queue holds pointers to maze locations in memory.  It is
     * large enough to hold the whole maze, although it should never
     * hold more than twice the minimum of rows and columns at any
     * time with our maze graphs.
     *
     * q_start is the index of the first unexplored location in the queue
     * q_end is the index just after the last unexplored location in the
     *       queue
     * cur is the maze location being explored
     */
    unsigned char* q[MAZE_MAX_X_DIM * MAZE_MAX_Y_DIM];
    int q_start, q_end;
    unsigned char* cur;

    /* Mark the starting location as reached, then put it into the queue. */
    q[0] = &maze[MAZE_INDEX(x, y)];
    *(q[0]) |= MAZE_REACH;
    q_start = 0;
    q_end = 1;

    /* Loop until queue is empty. */
    while (q_start != q_end) {
        /* Get location from front of queue. */
        cur = q[q_start++];

        /*
         * Explore four directions from current position.  Maze
         * construction guarantees that an adjacent open space implies
         * that the subsequent space in the same direction both exists
         * and is open (not a MAZE_WALL).
         *
         * Newly found locations are marked as reached before being
         * added to the queue, which prevents the same location from
         * being added when reached by multiple paths of equal length
         * from the starting point.
         */
        if ((cur[-2 * maze_x_dim] & MAZE_WALL) == 0 &&
            (cur[-4 * maze_x_dim] & MAZE_REACH) == 0) {
            cur[-4 * maze_x_dim] |= MAZE_REACH;
            q[q_end++] = &cur[-4 * maze_x_dim];
        }
        if ((cur[1] & MAZE_WALL) == 0 &&
            (cur[2] & MAZE_REACH) == 0) {
            cur[2] |= MAZE_REACH;
            q[q_end++] = &cur[2];
        }
        if ((cur[2 * maze_x_dim] & MAZE_WALL) == 0 &&
            (cur[4 * maze_x_dim] & MAZE_REACH) == 0) {
            cur[4 * maze_x_dim] |= MAZE_REACH;
            q[q_end++] = &cur[4 * maze_x_dim];
        }
        if ((cur[-1] & MAZE_WALL) == 0 &&
            (cur[-2] & MAZE_REACH) == 0) {
            cur[-2] |= MAZE_REACH;
            q[q_end++] = &cur[-2];
        }
    }

    /*
     * The queue is empty.  The number of locations marked is just q_end,
     * the number that passed through the queue.
     */
    return q_end;
}

/*
 * make_maze
 *   DESCRIPTION: Create a maze of specified dimensions.  The maze is
 *                built as a two-dimensional lattice in which the points
 *       01234      with odd indices in both dimensions are always open,
 *     0 -----      those with even indices in both dimensions are always
 *     1 - ? -      walls, and other points are either open or walls to form
 *     2 -?*?-      the maze.  The maze to the left is a 2x2 example.  The
 *     3 - ? -      spaces are the four (odd,odd) lattice points.  The
 *     4 -----      boundary is marked with minus signs (these are always
 *            walls).  The one (even,even) lattice point--also always
 *            a wall--is marked with an asterisk.  Finally, the
 *            four question marks may or may not be walls; these
 *            four options are used to create different mazes.
 *
 *                The algorithm used consists of two phases.  In the first
 *                phase, metaphorical worms are dropped into the maze and
 *                allowed to wander about randomly, digging out the maze,
 *                until they decide to stop.  More worms are added until
 *                all of the (odd,odd) points have been cleared.  Each worm
 *              starts on an (odd,odd) point still marked as a wall.
 *
 *                Once the worms have done their work, the second phase
 *                of the algorithm begins.  This phase ensures that a path
 *                exists from any (odd,odd) lattice point to any other
 *                (odd,odd) point.  First, those points connected to the
 *                point (1,1) are marked as reachable.  Next, an unconnected
 *                point adjacent to a connected point is chosen at random,
 *                and the wall between them is removed, making another
 *                section of the maze reachable from (1,1).  This process
 *                continues until most of the maze is reachable or a
 *                certain number of attempts have been made, at which point
 *                we scan the maze for such adjacent pairs (rather than
 *                choosing randomly) until the entire maze is reachable
 *                from (1,1).
 *   INPUTS: (x_dim,y_dim) -- size of maze
 *           start_fruits -- number of fruits to place in maze
 *   OUTPUTS: none
 *   RETURN VALUE: 0 on success, -1 on failure (if requested maze size
 *              exceeds limits set by defined values, with minimum
 *              (MAZE_MIN_X_DIM,MAZE_MIN_Y_DIM) and maximum
 *              (MAZE_MAX_X_DIM,MAZE_MAX_Y_DIM))
 *   SIDE EFFECTS: leaves MAZE_REACH markers on marked portion of maze
 */
int make_maze(int x_dim, int y_dim, int start_fruits) {
    /*
     * worm turn weights; the first dimension is relative direction
     * (number of 90-degree turns clockwise from up); the second is
     * whether the resulting space is open (not a wall) or a wall.
     */
    static int turn_wt[4][2] = {{1, 84}, {1, 9}, {3, 3}, {1, 9}};

    int remaining, trials;
    int x, y, wt[4], pick, dir, pref_dir, total, i;
    unsigned char* cur;

    /* Check the requested size, and save in local state if it is valid. */
    if (x_dim < MAZE_MIN_X_DIM || x_dim > MAZE_MAX_X_DIM ||
        y_dim < MAZE_MIN_Y_DIM || y_dim > MAZE_MAX_Y_DIM)
        return -1;
    maze_x_dim = x_dim;
    maze_y_dim = y_dim;

    /* Fill the maze with walls. */
    memset(maze, MAZE_WALL, sizeof (maze));

    /* Seed the random number generator. */
    srandom(time (NULL));

    /*
     * 'worm' phase of maze generation
     */

    /*
     * Track the number of (odd,odd) lattice points still marked
     * as MAZE_WALL.
     */
    remaining = maze_x_dim * maze_y_dim;
    do {
    /* Pick an (odd,odd) lattice point still marked as a MAZE_WALL. */
        do {
            x = (random() % maze_x_dim) * 2 + 1;
            y = (random() % maze_y_dim) * 2 + 1;
        } while ((maze[MAZE_INDEX(x, y)] & MAZE_WALL) == 0);

        /* Empty the starting point. */
        maze[MAZE_INDEX(x, y)] = MAZE_NONE;
        remaining--;

        /* The worm's initial preferred direction is random. */
        pref_dir = (random() % 4);

        /* Move around the maze until worm turns back on itself. */
        while (1) {
            /*
             * Choose the next direction of motion using weighted random
             * selection.  The directions are hardcoded.  The wt array
             * is the cumulative weight for each direction, and total
             * holds the running total weight (==wt[3] at the end).
             * A random value from 0 to total-1 is then chosen, and the
             * direction picked according to the weight distribution.
             * Weighting depends on direction and whether or not the
             * maze location has already been visited (is not a MAZE_WALL).
             * This code
             */
            total = 0;
            if (y > 1)
                total += turn_wt[pref_dir][maze[MAZE_INDEX(x, y - 2)] == MAZE_WALL];
            wt[0] = total;
            if (x < maze_x_dim * 2 - 1)
                total += turn_wt[(pref_dir + 3) % 4][maze[MAZE_INDEX(x + 2, y)] == MAZE_WALL];
            wt[1] = total;
            if (y < maze_y_dim * 2 - 1)
                total += turn_wt[(pref_dir + 2) % 4][maze[MAZE_INDEX(x, y + 2)] == MAZE_WALL];
            wt[2] = total;
            if (x > 1)
                total += turn_wt[(pref_dir + 1) % 4][maze[MAZE_INDEX(x - 2, y)] == MAZE_WALL];
            wt[3] = total;
            pick = (random() % total);
            for (dir = 0; pick >= wt[dir]; dir++);

            /* If worm decides to turn around, it's done. */
            if (((dir - pref_dir + 4) % 4) == 2)
                break;

            /* Move one space in the preferred direction. */
            pref_dir = dir;
            switch (pref_dir) {
                case 0:
                    maze[MAZE_INDEX(x, y - 1)] = MAZE_NONE;
                    y -=2;
                    break;
                case 1:
                    maze[MAZE_INDEX(x + 1, y)] = MAZE_NONE;
                    x += 2;
                    break;
                case 2:
                    maze[MAZE_INDEX(x, y + 1)] = MAZE_NONE;
                    y +=2;
                    break;
                case 3:
                    maze[MAZE_INDEX(x - 1, y)] = MAZE_NONE;
                    x -= 2;
                    break;
            }

            /* If necessary, the worm 'eats' the wall at the new space. */
            if (maze[MAZE_INDEX(x, y)] == MAZE_WALL)
            remaining--;
            maze[MAZE_INDEX(x, y)] = MAZE_NONE;
        } /* loop for one worm */

        /*
         * The worm phase continues until all of the (odd,odd) lattice
         * points in the maze are all empty.
         */
    } while (remaining > 0);

    /*
     * Begin the second phase of the algorithm, in which we guarantee
     * connectivity between all (odd,odd) lattice points in the maze.
     * We start by marking everything connected to (1,1).
     */
    remaining = maze_x_dim * maze_y_dim - mark_maze_area (1, 1);
    trials = 0;
    do {
        /*
         * Once most of the maze is connected, or we have tried randomly
         * "enough" times (arbitrarily defined as 100 times here, we scan
         * the rest of the maze for opportunities for connecting new regions
         * of the maze to the (1,1) lattice point.
         */
        if (remaining < 20 || ++trials > 100) {
            if ((x += 2) > maze_x_dim * 2) {
                x -= maze_x_dim * 2;
                if ((y += 2) > 2 * maze_y_dim)
                    y -= 2 * maze_y_dim;
            }
            cur = &maze[MAZE_INDEX(x, y)];
            if ((cur[0] & MAZE_REACH) != 0)
                continue;
        } else {
            /* Pick an unconnected (odd,odd) lattice point at random. */
            do {
                x = (random() % maze_x_dim) * 2 + 1;
                y = (random() % maze_y_dim) * 2 + 1;
                cur = &maze[MAZE_INDEX(x, y)];
            } while ((cur[0] & MAZE_REACH) != 0);
        }
        /*
         * Try to connect the unconnected point by knocking down a wall
         * in some direction.
         */
        if (y > 1 && (cur[-4 * maze_x_dim] & MAZE_REACH) != 0)
            cur[-2 * maze_x_dim] = MAZE_NONE;
        else if (x > 1 && (cur[-2] & MAZE_REACH) != 0)
            cur[-1] = MAZE_NONE;
        else if (x < 2 * maze_x_dim - 1 && (cur[2] & MAZE_REACH) != 0)
            cur[1] = MAZE_NONE;
        else if (y < 2 * maze_y_dim - 1 && (cur[4 * maze_x_dim] & MAZE_REACH) != 0)
            cur[2 * maze_x_dim] = MAZE_NONE;
        else
            continue;
        /*
         * Success!  Mark the newly connected portion of the maze
         * as reachable.
         */
        remaining -= mark_maze_area(x, y);
    } while (remaining > 0);

    /*
     * Remove the MAZE_REACH markers--these are reused to mark those
     * portions of the maze already seen by the player.
     */
    for (x = 1; x < 2 * maze_x_dim; x += 2)
        for (y = 1; y < 2 * maze_y_dim; y += 2)
            maze[MAZE_INDEX(x, y)] &= ~MAZE_REACH;

#if 0 /* Be kind and show the maze boundary at start. */
    for (x = 0; x < 2 * maze_x_dim; x++) {
        maze[MAZE_INDEX(x, 0)] |= MAZE_REACH;
        maze[MAZE_INDEX(x, 2 * maze_y_dim)] |= MAZE_REACH;
    }
    /* The value at y == 2 * maze_y_dim is the bottom of the right boundary. */
    for (y = 0; y <= 2 * maze_y_dim + 1; y++)
        maze[MAZE_INDEX(0, y)] |= MAZE_REACH;
#endif

#if GOD_MODE /* Remove all walls! */
    for (x = 1; x < 2 * maze_x_dim; x++) {
        for (y = 1; y < 2 * maze_y_dim; y++) {
            maze[MAZE_INDEX(x, y)] = MAZE_NONE;
        }
    }
#endif

    /* Put the required number of fruits in the maze. */
    n_fruits = 0;
    for (i = 0; i < start_fruits; i++)
        add_a_fruit_internal();

    /* Find an unfruited maze point and put the maze exit there. */
    do {
        x = (random() % maze_x_dim) * 2 + 1;
        y = (random() % maze_y_dim) * 2 + 1;
    } while ((maze[MAZE_INDEX(x, y)] & MAZE_FRUIT));
    maze[MAZE_INDEX(x, y)] |= MAZE_EXIT;
    exit_x = x;
    exit_y = y;

    return 0;
}

/*
 * The functions inside the preprocessor block below rely on block image
 * data in blocks.s.  These external data are neither available nor
 * necessary for testing maze generation, and are omitted to simplify
 * linking the test program (i.e., when TEST_MAZE_GEN is 1).
 */
#if (TEST_MAZE_GEN == 0)

/*
 * find_block
 *   DESCRIPTION: Find the appropriate image to be used for a given maze
 *                lattice point.
 *   INPUTS: (x,y) -- the maze lattice point
 *   OUTPUTS: none
 *   RETURN VALUE: a pointer to an image of a BLOCK_X_DIM x BLOCK_Y_DIM
 *                 block of data with one byte per pixel laid out as a
 *                 C array of dimension [BLOCK_Y_DIM][BLOCK_X_DIM]
 *   SIDE EFFECTS: none
 */
static unsigned char* find_block(int x, int y) {
    int fnum;     /* fruit found                           */
    int pattern;  /* stencil pattern for surrounding walls */

    /* Record whether fruit is present. */
    fnum = (maze[MAZE_INDEX(x, y)] & MAZE_FRUIT) / MAZE_FRUIT_1;

    /* The exit is always visible once the last fruit is collected. */
    if (n_fruits == 0 && (maze[MAZE_INDEX(x, y)] & MAZE_EXIT) != 0)
        return (unsigned char*)blocks[BLOCK_EXIT];

    /*
     * Everything else not reached is shrouded in mist, although fruits
     * show up as bumps.
     */
    if ((maze[MAZE_INDEX(x, y)] & MAZE_REACH) == 0) {
        if (fnum != 0)
            return (unsigned char*)blocks[BLOCK_FRUIT_SHADOW];
        return (unsigned char*)blocks[BLOCK_SHADOW];
    }

    /* Show fruit. */
    if (fnum != 0)
        return (unsigned char*)blocks[BLOCK_FRUIT_1 + fnum - 1];

    /* Show empty space. */
    if ((maze[MAZE_INDEX(x, y)] & MAZE_WALL) == 0)
        return (unsigned char*)blocks[BLOCK_EMPTY];

    /* Show different types of walls. */
    pattern = (((maze[MAZE_INDEX(x, y - 1)] & MAZE_WALL) != 0) << 0) |
              (((maze[MAZE_INDEX(x + 1, y)] & MAZE_WALL) != 0) << 1) |
              (((maze[MAZE_INDEX(x, y + 1)] & MAZE_WALL) != 0) << 2) |
              (((maze[MAZE_INDEX(x - 1, y)] & MAZE_WALL) != 0) << 3);
    return (unsigned char*)blocks[pattern];
}

/*
 * fill_horiz_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the leftmost
 *                pixel of a line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *   INPUTS: (x,y) -- leftmost pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_horiz_buffer(int x, int y, unsigned char buf[SCROLL_X_DIM]) {
    int map_x, map_y;     /* maze lattice point of the first block on line */
    int sub_x, sub_y;     /* sub-block address                             */
    int idx;              /* loop index over pixels in the line            */
    unsigned char* block; /* pointer to current maze block image           */

    /* Find the maze lattice point and the pixel address within that block. */
    map_x = x / BLOCK_X_DIM;
    map_y = y / BLOCK_Y_DIM;
    sub_x = x - map_x * BLOCK_X_DIM;
    sub_y = y - map_y * BLOCK_Y_DIM;

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; ) {

        /* Find address of block to be drawn. */
        block = find_block(map_x++, map_y) + sub_y * BLOCK_X_DIM + sub_x;

        /* Write block colors from one line into buffer. */
        for (; idx < SCROLL_X_DIM && sub_x < BLOCK_X_DIM; idx++, sub_x++)
            buf[idx] = *block++;

        /*
         * All subsequent blocks are copied starting from the left side
         * of the block.
         */
        sub_x = 0;
    }
}

/*
 * fill_vert_buffer
 *   DESCRIPTION: Given the (x,y) map pixel coordinate of the top pixel of
 *                a vertical line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *   INPUTS: (x,y) -- top pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_vert_buffer(int x, int y, unsigned char buf[SCROLL_Y_DIM]) {
    int map_x, map_y;     /* maze lattice point of the first block on line */
    int sub_x, sub_y;     /* sub-block address                             */
    int idx;              /* loop index over pixels in the line            */
    unsigned char* block; /* pointer to current maze block image           */

    /* Find the maze lattice point and the pixel address within that block. */
    map_x = x / BLOCK_X_DIM;
    map_y = y / BLOCK_Y_DIM;
    sub_x = x - map_x * BLOCK_X_DIM;
    sub_y = y - map_y * BLOCK_Y_DIM;

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; ) {

        /* Find address of block to be drawn. */
        block = find_block(map_x, map_y++) + sub_y * BLOCK_X_DIM + sub_x;

        /* Write block colors from one line into buffer. */
        for (; idx < SCROLL_Y_DIM && sub_y < BLOCK_Y_DIM;
                idx++, sub_y++, block += BLOCK_X_DIM)
            buf[idx] = *block;

        /*
         * All subsequent blocks are copied starting from the top
         * of the block.
         */
        sub_y = 0;
    }

    return;
}

/*
 * unveil_space
 *   DESCRIPTION: Unveils a maze lattice point (marks as MAZE_REACH, which
 *                means that it is drawn normally rather than as under mist),
 *                redrawing it if necessary.
 *   INPUTS: (x,y) -- the lattice point to be unveiled
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: may draw to the screen
 */
void unveil_space(int x, int y) {
    unsigned char* cur; /* pointer to the maze lattice point */

    /*
     * There's a wee bitty little bug in this function in the sense
     * that, if the left boundary is exposed, and the player reaches
     * the right boundary without scrolling the screen, the right
     * boundary will not be redrawn, and will appear to remain obscured.
     * One fix is to draw both left and right boundaries when unveiling
     * a left/right boundary block.  But it doesn't matter much, since
     * left and right boundaries aren't on the screen at the same time
     * anyway for our mazes.
     */

    /*
     * Allow exposure of bottom and right boundaries (left and right
     * boundaries are the same lattice point in the maze).
     */
    if (x < 0 || x > 2 * maze_x_dim || y < 0 || y > 2 * maze_y_dim)
        return;

    /* Has the location already been seen?  If so, do nothing. */
    cur = &maze[MAZE_INDEX(x, y)];
    if (*cur & MAZE_REACH)
        return;

    /* Unveil the location and redraw it. */
    *cur |= MAZE_REACH;
    draw_full_block (x * BLOCK_X_DIM, y * BLOCK_Y_DIM, find_block(x, y));
}

/*
 * check_for_fruit
 *   DESCRIPTION: Checks a maze lattice point for fruit, eats the fruit if
 *                one is present (i.e., removes it from the maze), and updates
 *                the number of fruits (including displayed number).
 *   INPUTS: (x,y) -- the lattice point to be checked for fruit
 *   OUTPUTS: none
 *   RETURN VALUE: fruit number found (1 to NUM_FRUITS), or 0 for no fruit
 *   SIDE EFFECTS: may draw to the screen (empty fruit and, once last fruit
 *                 is eaten, the maze exit)
 */
int check_for_fruit(int x, int y) {
    int fnum;  /* fruit number found */

    /* If outside the feasible fruit range, return no fruit. */
    if (x < 0 || x >= 2 * maze_x_dim || y < 0 || y >= 2 * maze_y_dim)
        return 0;

    /* Calculate the fruit number. */
    fnum = (maze[MAZE_INDEX(x, y)] & MAZE_FRUIT) / MAZE_FRUIT_1;

    /* If fruit was present... */
    if (fnum != 0) {
        /* ...remove it. */
        maze[MAZE_INDEX(x, y)] &= ~MAZE_FRUIT;

    /* Update the count of fruits. */
    --n_fruits;

    /* The exit may appear. */
    if (n_fruits == 0)
        draw_full_block (exit_x * BLOCK_X_DIM, exit_y * BLOCK_Y_DIM, find_block(exit_x, exit_y));

        /* Redraw the space with no fruit. */
        draw_full_block (x * BLOCK_X_DIM, y * BLOCK_Y_DIM, find_block(x, y));
    }

    /* Return the fruit number found. */
    return fnum;
}

/*
 * check_for_win
 *   DESCRIPTION: Checks whether the play has won the maze by reaching a
 *                given maze lattice point.  Winning occurs when no fruits
 *                remain in the maze, and the player reaches the maze exit.
 *   INPUTS: (x,y) -- the lattice point at which the player has arrived
 *   OUTPUTS: none
 *   RETURN VALUE: 1 if player has won, 0 if not
 *   SIDE EFFECTS: none
 */
int check_for_win(int x, int y) {
    /* Check that position falls within valid boundaries for exit. */
    if (x < 0 || x >= 2 * maze_x_dim || y < 0 || y >= 2 * maze_y_dim)
        return 0;

    /* Return win condition. */
    return (n_fruits == 0 && (maze[MAZE_INDEX(x, y)] & MAZE_EXIT) != 0);
}

/*
 * _add_a_fruit
 *   DESCRIPTION: Add a fruit to a random (odd,odd) lattice point in the
 *                maze.  Update the number of fruits, including the displayed
 *                value.  If requested, draw the new fruit on the screen.
 *   INPUTS: show -- 1 if new fruit should be drawn, 0 if not
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes displayed fruit value, may draw to screen
 */
static void _add_a_fruit(int show) {
    int x, y;    /* lattice point for new fruit */

    /*
     * Pick an unfruited lattice point at random.  Could fall on the
     * maze exit, if that is already defined.
     */
    do {
        x = (random() % maze_x_dim) * 2 + 1;
        y = (random() % maze_y_dim) * 2 + 1;
    } while ((maze[MAZE_INDEX(x, y)] & MAZE_FRUIT));

    /* Add a random fruit to that location. */
    maze[MAZE_INDEX(x, y)] |= ((random() % NUM_FRUIT_TYPES) + 1) * MAZE_FRUIT_1;

    /* Update the number of fruits. */
    ++n_fruits;

    /* If necessary, draw the fruit on the screen. */
    if (show)
    draw_full_block (x * BLOCK_X_DIM, y * BLOCK_Y_DIM, find_block(x, y));
}

/*
 * add_a_fruit
 *   DESCRIPTION: Add a fruit to a random (odd, odd) lattice point in the
 *                maze.  Update the number of fruits, including the displayed
 *                value.  Draw the new fruit on the screen.  If the new fruit
 *                is the only one in the maze, erase the maze exit.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: the number of fruits in the maze (after addition)
 *   SIDE EFFECTS: changes displayed fruit value, may draw to screen
 */
int add_a_fruit() {
    /* Most of the work is done by a helper function. */
    _add_a_fruit(1);

    /* The exit may disappear. */
    if (n_fruits == 1)
    draw_full_block (exit_x * BLOCK_X_DIM, exit_y * BLOCK_Y_DIM,
             find_block(exit_x, exit_y));

    /* Return the current number of fruits in the maze. */
    return n_fruits;
}

/*
 * add_a_fruit_internal
 *   DESCRIPTION: Add a fruit to a random (odd,odd) lattice point in the
 *                maze.  Update the number of fruits, including the displayed
 *                value.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes displayed fruit value
 */
static void add_a_fruit_internal() {
    /*
     * Call a helper function, indicating that fruit should not be drawn
     * on the screen at this point.
     */
    _add_a_fruit(0);
}

/*
 * get_player_block
 *   DESCRIPTION: Get a graphical image for the player.
 *   INPUTS: cur_dir -- current direction of motion for the player
 *   OUTPUTS: none
 *   RETURN VALUE: a pointer to an image of a BLOCK_X_DIM x BLOCK_Y_DIM
 *                 block of data with one byte per pixel laid out as a
 *                 C array of dimension [BLOCK_Y_DIM][BLOCK_X_DIM]
 *   SIDE EFFECTS: none
 */
unsigned char* get_player_block(dir_t cur_dir) {
    return (unsigned char*)blocks[BLOCK_PLAYER_UP + cur_dir];
}

/*
 * get_player_mask
 *   DESCRIPTION: Get a graphical mask for the player.
 *   INPUTS: cur_dir -- current direction of motion for the player
 *   OUTPUTS: none
 *   RETURN VALUE: a pointer to an image of a BLOCK_X_DIM x BLOCK_Y_DIM
 *                 block of data with one byte per pixel laid out as a
 *                 C array of dimension [BLOCK_Y_DIM][BLOCK_X_DIM];
 *                 the bytes in this block indicate whether or not the
 *                 corresponding byte in the player image should be
 *                 drawn (1 is drawn/opaque, 0 is not drawn/fully
 *                 transparent)
 *   SIDE EFFECTS: none
 */
unsigned char* get_player_mask(dir_t cur_dir) {
    return (unsigned char*)blocks[BLOCK_PLAYER_MASK_UP + cur_dir];
}

/*
 * find_open_directions
 *   DESCRIPTION: Determine which directions are open to movement from a
 *              given maze point.
 *   INPUTS: (x,y) -- lattice point of interest in maze
 *   OUTPUTS: open[] -- array of boolean values indicating that a direction
 *                   is open; indexed by DIR_* enumeration values
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void find_open_directions(int x, int y, int op[NUM_DIRS]) {
    op[DIR_UP]    = (0 == (maze[MAZE_INDEX(x, y - 1)] & MAZE_WALL));
    op[DIR_RIGHT] = (0 == (maze[MAZE_INDEX(x + 1, y)] & MAZE_WALL));
    op[DIR_DOWN]  = (0 == (maze[MAZE_INDEX(x, y + 1)] & MAZE_WALL));
    op[DIR_LEFT]  = (0 == (maze[MAZE_INDEX(x - 1, y)] & MAZE_WALL));
}

int get_fruit_num(){
  return n_fruits;
}

#else /* TEST_MAZE_GEN == 1 */
/*
 * The code here allows you to test the maze generation routines visually
 * by creating a maze and printing it to the screen.
 */

/*
 * print_maze
 *   DESCRIPTION: Print a maze as ASCII text.
 *   INPUTS: none
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: prints to stdout
 */
void print_maze() {
    int i;  /* vertical loop index   */
    int j;  /* horizontal loop index */

    /* Loop over maze rows. */
    for (i = 0; i <= 2 * maze_y_dim; i++) {

        /* Loop over maze columns. */
        for (j = 0; j <= 2 * maze_x_dim; j++) {

            /*
             * Print open spaces and walls, reached and unreached, as
             * distinct characters.
             */
            printf("%c",
                  ((maze[MAZE_INDEX(j, i)] & MAZE_WALL) ?
                  ((maze[MAZE_INDEX(j, i)] & MAZE_REACH) ? '*' : '%') :
                  ((maze[MAZE_INDEX(j, i)] & MAZE_REACH) ? '.' : ' ')));
        }

        /* End the printed line. */
        puts("");
    }
}

/*
 * This function is called in maze generation. We define a stub to keep
 * the linker happy.
 */
static void add_a_fruit_internal() {}




/*
 * main
 *   DESCRIPTION: main program for testing maze generation; hardwired to
 *                build and print a maze of a certain size
 *   INPUTS: none (command line arguments are ignored)
 *   OUTPUTS: none
 *   RETURN VALUE: 0 on success (always!)
 */


int main() {
    make_maze(20, 20, 0);
    print_maze();
    return 0;
}

#endif /* TEST_MAZE_GEN */
