#	$CoreSDI$

CC	= cc
CFLAGS	= -O2 -Wall

#
# Uncomment this to add mysql support
#
#LDFLAGS= -L/usr/local/lib/mysql -lmysqlclient

OBJS	= syslogd.o ttymsg.o modules.o om_classic.o im_bsd.o im_udp.o \
	  im_unix.o iolib.o peo/om_peo.o peo/hash.o peo/rmd160.o

all:	syslogd peochk man

.c.o:
	$(CC) $(CFLAGS) -c $<

syslogd: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LDFLAGS)

peochk:
	@(cd peo && make peochk);

peo/om_peo.o peo/hash.o peo/rmd160.o:
	@(cd peo && make om_peo);

man:
	@make -f Makeman

clean:
	-@(cd peo && make clean);
	-rm -f core *.core *.o syslogd

