#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c modules.c om_classic.c om_mysql.c im_bsd.c im_udp.c im_unix.c iolib.c
#OBJS=	./peo/hash.o ./peo/rmd160.o ./peo/om_peo.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN=	syslogd.8 syslog.conf.5
DEBUG=	-ggdb -Wall
LDADD=	-L/usr/local/lib/mysql -lmysqlclient
CFLAGS=	-D WANT_MYSQL

.include <bsd.prog.mk>
