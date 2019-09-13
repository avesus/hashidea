# define withstats to be 0 if not defined on command line
ifndef WITHSTATS
WITHSTATS=0
endif

CC = g++
#LDFLAGS = -pg
#-pg
CFLAGS = -O3 -g -std=c++11 -D_GNU_SOURCE -DCOLLECT_STAT=$(WITHSTATS)
LDLIBS = -lpthread -lm -lgsl -lgslcblas -lm
Hashtable = hashtable.o
Hashfunction = hash.o 

all:	harness waitfortemp getcacheparms

harness: harness.o arg.o  util.o timing.o dist.o temp.o ../lib/$(Hashtable) ../lib/$(Hashfunction)
waitfortemp: waitfortemp.o temp.o arg.o util.o
getcacheparms: getcacheparms.o util.o timing.o

testrand: testrand.o dist.o util.o

harness.o: harness.c arg.h timing.h util.h temp.h ../lib/hashtable.h ../lib/hashstat.h stat.h
arg.o: arg.c arg.h
util.o: util.h util.c stat.h
timing.o: timing.c timing.h util.h
dist.o: dist.c dist.h
temp.o: temp.c temp.h stat.h
../lib/%.o:	../lib/%.c ../lib/hash.h ../lib/hashtable.h
	make -C ../lib WITHSTATS=$(WITHSTATS) $*.o

clean:
	/bin/rm -f *.o harness *~
	make -C ../lib clean

