#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 alejo Exp $

PROG=	peochk
SRCS=	om_peo.c hash.c rmd160.c peochk.c
OBJS=	hash.o rmd160.o om_peo.o
.PATH:	${.CURDIR}/../../usr.bin/wall
DEBUG=	-ggdb -Wall

.include <bsd.prog.mk>
