#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

CC	=	gcc
CFLAGS	=	-Wall -ggdb
LDADD=	-lc
#-L/usr/local/lib/mysql -lmysqlclient
COMPILE	=	$(CC) $(CFLAGS) -c $(.ALLSRC)
LINK	=	$(CC) $(CFLAGS) $(LDADD) -o $(.TARGET) $(.ALLSRC)


all:	syslogd	\
	peochk \
	tester

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
	@(cd peo && make peochk);

peo/om_peo.o peo/hash.o peo/rmd160.o:
	@(cd peo && make om_peo);

tester:		tester.c

clean:
	@(cd peo && make clean); rm -f *.o syslogd 
	@rm -f *.o syslogd tester
