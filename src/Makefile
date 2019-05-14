CC = gcc
CFLAGS = -O1 -g -std=c11 -D_GNU_SOURCE
LDLIBS = -lpthread -lm

harness: harness.o arg.o  util.o timing.o
harness.o: harness.c arg.h timing.h util.h
arg.o: arg.c arg.h
util.o: util.h util.c
timing.o: timing.c timing.h util.h

clean:
	/bin/rm -f *.o harness
