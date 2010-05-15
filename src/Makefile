TOP=	../
PROG=	${SOSYSLOGD_DAEMON_NAME}
SRCS=	modules.c	\
	syslogd.c	\
	options.c

# Defined in ${TOP}/Makefile.config
PROG_FLAGS=	${CFLAGS} ${LIBS} ${SYSLOGD_LIBS} ${MAIN_CPPFLAGS}
CFLAGS+=	-02 -Wall ${DEBUG_FLAGS} ${PROG_FLAGS}
LDFLAGS+=

include ${TOP}/Makefile.config
include ${TOP}/mk/build.prog.mk

