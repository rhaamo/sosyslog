#	$CoreSDI$

CC	= cc
CFLAGS	= -Wall -O2
OBJS	= peochk.o hash.o rmd160.o

all: peochk om_peo

.c.o:
	$(CC) $(CFLAGS) -c $<

peochk: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

om_peo:	om_peo.o hash.o rmd160.o

clean:
	-rm -f core *.core *.o peochk

