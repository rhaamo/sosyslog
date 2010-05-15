LIB=	${MLIBNAME}

DCCFLAGS=	${DCCFLAGS}
CPPFLAGS=	-I.. ${CPPFLAGS}
CFLAGS+=	-O2 -Wall ${DEBUG_FLAGS} ${CPPFLAGS} ${DCCFLAGS}

HASH_SRCS=	../peo/hash.c
SRCS=		${HASH_SRCS} ${MODULES_SRCS}

SHARED_PARAMS=	${SHARED_PARAMS}

include ${TOP}/Makefile.config
include ${TOP}/mk/build.lib.mk

