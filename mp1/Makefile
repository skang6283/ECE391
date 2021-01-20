CFLAGS=-Wall -g
LDFLAGS=-lm

.PHONY: all
all: ktest utest

ktest: mp1_testcases.o
	$(CC) $(LDFLAGS) $^ -o $@

utest: mp1_userspace.o mp1_testcases_u.o mp1_u.o
	$(CC) $(LDFLAGS) $^ -o $@ 

mp1_u.o: mp1.S mp1.h
	as -g -L mp1.S -o mp1_u.o

mp1_testcases.o: mp1_testcases.c mp1.h
	$(CC) $(CFLAGS) -c -o $@ $<

mp1_testcases_u.o: mp1_testcases.c mp1.h
	$(CC) $(CFLAGS) -D_USERSPACE -c -o $@ $<

mp1_userspace.o: mp1_userspace.c mp1.h
	$(CC) $(CFLAGS) -c -D_USERSPACE -o $@ $<

clean:
	rm -f *.o ktest utest

