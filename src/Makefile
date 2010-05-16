TOP=	../
PROG=	${SOSYSLOG_DAEMON_NAME}
SRCS=	modules.c	\
	syslogd.c	\
	options.c

# Defined in ${TOP}/Makefile.config
PROG_FLAGS=	${LIBS} ${SYSLOGD_LIBS} ${MAIN_CPPFLAGS}
CFLAGS+=	-O2 -Wall ${DEBUG_FLAGS} ${PROG_FLAGS} -I.. -I.
LDFLAGS+=	${SYSLOGD_LIBS}

include ${TOP}/Makefile.config
include ${TOP}/mk/build.prog.mk

