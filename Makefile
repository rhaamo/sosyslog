#	$CoreSDI$

CC	=	gcc
CFLAGS	=	-O2 -Wall
LDADD	=	-lc
#-L/usr/local/lib/mysql -lmysqlclient
COMPILE	=	$(CC) $(CFLAGS) -c $(.ALLSRC)
LINK	=	$(CC) $(CFLAGS) $(LDADD) -o $(.TARGET) $(.ALLSRC)


all:	syslogd	\
	peochk	\
	man

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

man:
	@make -f Makeman

clean:
	-@(cd peo && make clean);
	-@rm -f core *.core *.o syslogd

