#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

CC	=	gcc
CFLAGS	=	-Wall -ggdb
LDADD=	-lc
COMPILE	=	$(CC) $(CFLAGS) -c $(.ALLSRC)
LINK	=	$(CC) $(CFLAGS) $(LDADD) -o $(.TARGET) $(.ALLSRC)


all: peochk om_peo

peochk:		peochk.o hash.o rmd160.o
	$(LINK)

om_peo:		om_peo.o hash.o rmd160.o

peochk.o:	peochk.c 
	$(COMPILE)

om_peo.o:	om_peo.c
	$(COMPILE)

hash.o:		hash.c
	$(COMPILE)

rmd160.o:	rmd160.c
	$(COMPILE)

clean:
	rm *.o peochk


