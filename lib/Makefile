CC = g++
CFLAGS = -O3 -g -std=c++11 -D_GNU_SOURCE -DCOLLECT_STAT=$(WITHSTATS)
LDLIBS = -lpthread -lm -lgsl -lgslcblas -lm -latomic
SRCS = $(wildcard *.c)
OBJS = $(patsubst %.c,%.o,$(SRCS))

all:	$(OBJS)

%.o:	%.c hash.h hashstat.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	/bin/rm -f *.o *~


