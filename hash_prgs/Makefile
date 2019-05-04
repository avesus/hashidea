C = gcc
CXX = g++
CXXFLAGS = -std=c++11
CFLAGS= -std=c99
LDLIBS=  -lpthread

all: hash hashint lhi lh hashint2 hashint_trace hashint_trace2

hash.o: hash.c
	$(C) $(CFLAGS) -c hash.c $(LDLIBS)

hash: hash.o

hashint.o: hashint.c
	$(C) $(CFLAGS) -c hashint.c $(LDLIBS)

hashint: hashint.o

hashint_trace.o: hashint_trace.c
	$(C) $(CFLAGS) -c hashint_trace.c $(LDLIBS)

hashint_trace: hashint_trace.o

hashint_trace2.o: hashint_trace2.c
	$(C) $(CFLAGS) -c hashint_trace2.c $(LDLIBS)

hashint_trace2: hashint_trace2.o

hashint2.o: hashint2.c
	$(C) $(CFLAGS) -c hashint2.c $(LDLIBS)

hashint2: hashint2.o

lhi.o: lhi.c 
	$(C) $(CFLAGS) -c lhi.c $(LDLIBS)

lhi: lhi.o

lh.o: lh.c 
	$(C) $(CFLAGS) -c lh.c $(LDLIBS)

lh: lh.o

clean:
	rm -f *~ *.o ./#* hash hashint lh lhi hashint2 hashint_trace hashint_trace2 non-block-ll non-block-ll_trace
