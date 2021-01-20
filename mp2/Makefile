all: mazegame tr

HEADERS=blocks.h maze.h modex.h text.h Makefile

CFLAGS=-g -Wall

mazegame: mazegame.o maze.o blocks.o modex.o text.o
	gcc -g -lpthread -o mazegame mazegame.o maze.o blocks.o modex.o text.o

tr: modex.c ${HEADERS} text.o
	gcc ${CFLAGS} -DTEXT_RESTORE_PROGRAM=1 -o tr modex.c text.o

%.o: %.c ${HEADERS}
	gcc ${CFLAGS} -c -o $@ $<

%.o: %.s ${HEADERS}
	gcc ${CFLAGS} -c -o $@ $<

clean:: clear
	rm -f *.o *~ a.out

clear:
	rm -f mazegame tr input

