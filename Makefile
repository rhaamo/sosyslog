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
		peo/om_peo.o\
		peo/hash.o\
		peo/rmd160.o
	$(LINK)
	
peochk:
	@cd peo; make peochk; cd ..

peo/om_peo.o peo/hash.o peo/rmd160.o:
	@cd peo; make om_peo; cd ..

clean:
	rm -f *.o peo/*.o syslogd peo/peochk
