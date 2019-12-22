REPO = $(shell git rev-parse --show-toplevel)
CC = g++
CFLAGS = -O3 -g -std=c++11 -D_GNU_SOURCE -DCOLLECT_STAT=$(WITHSTATS)
LDLIBS = -lpthread -lm -lgsl -lgslcblas -lm -latomic
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

all:	$(OBJS)

%.o:	%.c hash.h hashstat.h cache-constants.h cache-params.h
	$(CC) $(CFLAGS) -c $< -o $@

cache-params.h:
	../src/getcacheparms > cache-params.h

clean:
	/bin/rm -f *.o *~ *#* cache-params.h


