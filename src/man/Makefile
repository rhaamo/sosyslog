# $CoreSDI: Makefile.in,v 1.9 2001/11/20 00:48:41 alejo Exp $
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

prefix= /usr/local
MANDIR= ${prefix}/share/man
MAN=    syslog.conf.5 syslogd.8 im_bsd.8 im_udp.8 om_udp.8 im_unix.8 im_file.8 om_classic.8 om_tcp.8 im_tcp.8 om_oracle8i.8 om_mysql.8 om_pgsql.8 om_peo.8 peochk.8 om_regex.8
OMAN=   $(MAN:=.gz)
INSTALL = /usr/bin/install -c

all:
	@echo "all done"

install maninstall: $(OMAN)

$(OMAN):
	@if [ ! -d $(DESTDIR)$(MANDIR)/man$(subst .,,$(suffix $(@:%.gz=%))) ]; then \
		mkdir -p $(DESTDIR)$(MANDIR)/man$(subst .,,$(suffix $(@:%.gz=%))); \
	fi
	-gzip -cf $(@:%.gz=%) > $(DESTDIR)$(MANDIR)/man$(subst .,,$(suffix $(@:%.gz=%)))/$@


.PHONY: clean distclean
clean:
distclean:
	-rm -f Makefile
	-rm -f BSDmakefile
	-rm -f GNUmakefile
