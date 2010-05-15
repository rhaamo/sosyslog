TOP=	../..
PROG=	peochk
SRCS=	peochk.c	\
	../options.c	\
	hash.c

CPPFLAGS=	-I.. ${CPPFLAGS}
CFLAGS+=	-O2 -Wall ${DEBUG_FLAGS} ${CPPFLAGS}
LDFLAGS+=

include ${TOP}/Makefile.config
include ${TOP}/mk/build.prog.mk

