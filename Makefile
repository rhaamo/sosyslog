#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 deraadt Exp $

PROG=	syslogd
SRCS=	syslogd.c ttymsg.c modules.c m_classic.c m_mysql.c
.PATH:	${.CURDIR}/../../usr.bin/wall
MAN=	syslogd.8 syslog.conf.5
DEBUG=	-ggdb

.include <bsd.prog.mk>
