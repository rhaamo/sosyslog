#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

CC	=	gcc
CFLAGS	=	-Wall -ggdb
LDADD=	-lc
#-L/usr/local/lib/mysql -lmysqlclient
COMPILE	=	$(CC) $(CFLAGS) -c $(.ALLSRC)
LINK	=	$(CC) $(CFLAGS) $(LDADD) -o $(.TARGET) $(.ALLSRC)


all:	syslogd	\
	peochk

syslogd:	syslogd.c\
		ttymsg.c\
		modules.c\
		om_classic.c\
		im_bsd.c\
		im_udp.c\
		im_unix.c\
		iolib.c\
		hash.o\
		rmd160.o\
		om_peo.o
	$(LINK)
	
peochk:		hash.o\
		rmd160.o\
		peochk.o
	$(LINK)

peochk.o:	peo/peochk.c
	$(COMPILE)

hash.o:		peo/hash.c
	$(COMPILE)

rmd160.o:	peo/rmd160.c
	$(COMPILE)

om_peo.o:	peo/om_peo.c
	$(COMPILE)

peo/hash.c\
peo/rmd160.c\
peo/om_peo.c\
peo/peochk.c:	peo/hash.h syslogd.h	


clean:
	rm *.o
