#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

PROG=	syslogd
#SRCS=	syslogd.c ttymsg.c modules.c om_classic.c om_mysql.c im_bsd.c im_udp.c im_unix.c iolib.c
SRCS=	syslogd.c ttymsg.c modules.c om_classic.c im_bsd.c im_udp.c im_unix.c iolib.c
OBJS=	./peo/hash.o ./peo/rmd160.o ./peo/om_peo.o
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN=	syslogd.8 syslog.conf.5
CFLAGS=	-ggdb -Wall #-D WANT_MYSQL
LDADD=	-lc #-L/usr/local/lib/mysql -lmysqlclient

.include <bsd.prog.mk>
