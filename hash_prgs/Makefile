C = gcc
CFLAGS= -std=c99
LDLIBS=  -lpthread

all: hash hashint lhi lh hashint2 hashint_trace hashint_trace2 me-nb-ll me-nb-ll_trace ll_rowresize ll_totalresize

hash.o: hash.c
	$(C) $(CFLAGS) -c hash.c $(LDLIBS)

hash: hash.o

hashint.o: hashint.c
	$(C) $(CFLAGS) -c hashint.c $(LDLIBS)

hashint: hashint.o


me-nb-ll.o: me-nb-ll.c
	$(C) $(CFLAGS) -c me-nb-ll.c $(LDLIBS)

me-nb-ll: me-nb-ll.o

ll_rowresize.o: ll_rowresize.c
	$(C) $(CFLAGS) -c ll_rowresize.c $(LDLIBS)

ll_rowresize: ll_rowresize.o


ll_totalresize.o: ll_totalresize.c
	$(C) $(CFLAGS) -c ll_totalresize.c $(LDLIBS)

ll_totalresize: ll_totalresize.o


me-nb-ll_trace.o: me-nb-ll_trace.c
	$(C) $(CFLAGS) -c me-nb-ll_trace.c $(LDLIBS)

me-nb-ll_trace: me-nb-ll_trace.o

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
	rm -f core.* *~ *.o ./#* hash hashint lh lhi hashint2 hashint_trace hashint_trace2 non-block-ll non-block-ll_trace me-nb-ll me-nb-ll_trace ll_rowresize ll_totalresize 
