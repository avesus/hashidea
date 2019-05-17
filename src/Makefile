CC = gcc
CFLAGS = -O1 -g -std=c11 -D_GNU_SOURCE
LDLIBS = -lpthread -lm -lgsl -lgslcblas -lm -latomic

all:	harness testrand

harness: harness.o arg.o  util.o timing.o ../lib/hashtable.o ../lib/hash.o
harness.o: harness.c arg.h timing.h util.h ../lib/hashtable.h 
arg.o: arg.c arg.h
util.o: util.h util.c
timing.o: timing.c timing.h util.h
hash.o: ../lib/hash.c ../lib/hash.h
hashtable.o: ../lib/hashtable.c ../lib/hashtable.h ../lib/hash.h
dist.o: dist.c dist.h

testrand: testrand.o dist.o util.o


clean:
	/bin/rm -f *.o harness *~ 
