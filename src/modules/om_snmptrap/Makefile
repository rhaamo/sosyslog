# $CoreSDI: Makefile,v 1.14 2002/03/21 19:02:28 claudio Exp $
#
# Copyright (c) 2001, Core SDI S.A., Argentina
# All rights reserved
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither name of the Core SDI S.A. nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#

CFLAGS+=	-Wall -g
CPPFLAGS+=	-Iusr/local/lib -I/usr/local/include
pICFLAGS=	-fPIC
LDFLAGS=	-shared -L/usr/local/lib -lsnmp -lcrypto -lz -lm
INSTALL=	/usr/bin/install
NROFF=		/usr/bin/nroff
LIBDIR?=	${PREFIX}/lib/alat
MANDIR?=	${PREFIX}/man/cat8

SRCS=		om_snmptrap.c
OBJS=		${SRCS:.c=.o}
MAN=		${SRCS:.c=.0}
LIB=		libmsyslog_${SRCS:.c=.so}



.SUFFIXES: .c .0 .8 .0

.8.0:
	${NROFF} -mandoc $< > $@

.c.o:
	${CC} ${CFLAGS} ${CPPFLAGS} ${PICFLAGS} -c $< -o $@

all: ${LIB} ${MAN}

man: ${MAN}

fake: ${FAKE_LIBDIR}/${LIB} ${FAKE_MANDIR}/${MAN}

install: ${LIBDIR}/${LIB} ${MANDIR}/${MAN}

${OBJS}: ${SRCS}

${LIB}: ${OBJS}
	${LD} ${LDFLAGS} -o $@ ${OBJS}

${LIBDIR}/${LIB} ${FAKE_LIBDIR}/${LIB}: ${LIB}
	${INSTALL} -d ${@:H} && ${INSTALL} -s ${LIB} $@

${MANDIR}/${MAN} ${FAKE_MANDIR}/${MAN}: ${MAN}
	${INSTALL} -d ${@:H} && ${INSTALL} ${MAN} $@

clean:
	-rm -f ${OBJS} ${LIB} ${MAN}

distclean cleandist cleandir: clean
	-rm -rf ${FAKEDIR} ./packages

