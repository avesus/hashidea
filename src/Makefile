# define withstats to be 0 if not defined on command line
ifndef WITHSTATS
WITHSTATS=0
endif

CC = gcc
#LDFLAGS = -pg
#-pg
CFLAGS = -O3 -g -std=c11 -D_GNU_SOURCE -DCOLLECT_STAT=$(WITHSTATS)
LDLIBS = -lpthread -lm -lgsl -lgslcblas -lm
Hashtable = hashtable.o
Hashfunction = hash.o 

all:	harness 

harness: harness.o arg.o  util.o timing.o dist.o ../lib/$(Hashtable) ../lib/$(Hashfunction)

testrand: testrand.o dist.o util.o

harness.o: harness.c arg.h timing.h util.h ../lib/hashtable.h ../lib/hashstat.h
arg.o: arg.c arg.h
util.o: util.h util.c
timing.o: timing.c timing.h util.h
dist.o: dist.c dist.h
../lib/%.o:	../lib/%.c ../lib/hash.h ../lib/hashtable.h
	make -C ../lib WITHSTATS=$(WITHSTATS)

clean:
	/bin/rm -f *.o harness *~
	make -C ../lib clean

