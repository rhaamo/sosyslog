#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c modules.c om_classic.c om_mysql.c im_bsd.c im_socket.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN=	syslogd.8 syslog.conf.5
DEBUG=	-ggdb
LDADD= -L/usr/local/lib/mysql -lmysqlclient

.include <bsd.prog.mk>
