TOP=	../..
PROG=	peochk
SRCS=	peochk.c	\
	../options.c	\
	${HASH_SRCS}

CPPFLAGS=	-I.. -I../..
CFLAGS+=	-O2 -Wall ${DEBUG_FLAGS} ${CPPFLAGS}
LDFLAGS+=

include ${TOP}/Makefile.config
include ${TOP}/mk/build.prog.mk

