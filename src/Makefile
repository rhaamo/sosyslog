TOP=	../
PROG=	${SYSLOGD_DAEMON_NAME}
SRCS=	syslogd.c	\
	options.c	\
	modules.c

# Defined in ${TOP}/Makefile.config:
# 	${DEBUG_FLAGS}
# 	${SYSLOGD_LIBS}
PROG_FLAGS=	${CFLAGS} ${CPPFLAGS} ${LIBS} ${SYSLOGD_LIBS} ${MAIN_CPPFLAGS}
CFLAGS+=	-02 -Wall ${DEBUG_FLAGS} ${PROG_FLAGS}
LDFLAGS+=

include ${TOP}/Makefile.config
include ${TOP}/mk/build.prog.mk

####CC=		gcc
####CFLAGS=		-g -O2 -Wall
####CPPFLAGS=	 -I.
####MAIN_CPPFLAGS=	
####LIBS=		
####SYSLOGD_LIBS=	 -Wl,-E
####
####INSTALL=	/usr/bin/install -c
####INSTALL_DIR=    /usr/local/sbin
####
####PROG=		sosyslogd
####PROG_FLAGS=	$(CFLAGS) $(CPPFLAGS) $(LIBS) $(SYSLOGD_LIBS) $(MAIN_CPPFLAGS)
####SRCS=		modules.c syslogd.c options.c
####OBJS=		$(SRCS:.c=.o)
####
####all: $(PROG)
####
####.c.o:
####	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@
####
####modules.o: config.h
####
####$(OBJS): $(SRCS)
####	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(@:.o=.c) -o $@
####
####$(PROG): $(OBJS)
####	$(CC) $(PROG_FLAGS) $(OBJS) -o $@
####
####.PHONY:	clean distclean
####
####clean:
####	-rm -f $(OBJS) core *.core $(PROG)
####
####distclean: clean
####	-rm -f Makefile config.h
####
####install-prog:
####	@if [ ! -d $(DESTDIR)$(INSTALL_DIR) ]; then \
####		mkdir -p $(DESTDIR)$(INSTALL_DIR); \
####	fi
####	@if [ -f "$(INSTALL_DIR)/$(PROG)" ]; then \
####		mv -f "$(INSTALL_DIR)/$(PROG)" "$(INSTALL_DIR)/$(PROG).old"; \
####	fi
####	$(INSTALL) -s $(PROG) $(DESTDIR)$(INSTALL_DIR)/ ;
####
####install: install-prog
####	@echo -e \
####	" **********************************************************\n"\
####	"**       A new syslog daemon was installed !!           **\n"\
####	"**   Please read the INSTALL and README files           **\n"\
####	"**   to get your syslog configuration ready             **\n"\
####	"**********************************************************\n"
