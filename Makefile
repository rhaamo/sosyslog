#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c modules.c om_classic.c om_mysql.c om_peo.c im_bsd.c im_udp.c
OBJS=	./peo/hash.o
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN=	syslogd.8 syslog.conf.5
DEBUG=	-ggdb -Wall
LDADD= -L/usr/local/lib/mysql -lmysqlclient

.include <bsd.prog.mk>
