/*	$Id: om_classic.c,v 1.17 2000/05/17 16:25:32 gera Exp $
 * Copyright (c) 1983, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

/*
 *  om_classic -- classic behaviour module
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *         form syslogd.c Eric Allman  and Ralph Campbell
 *
 */

#include <sys/time.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/syslog.h>

#include "syslogd.h"
#include "modules.h"

void	wallmsg __P((struct filed *, struct iovec *));
char   *ttymsg __P((struct iovec *, int, char *, int));

extern time_t now;
void    logerror __P((char *));

int
om_classic_doLog(f, flags, msg, context)
	struct filed *f;
	int flags;
	char *msg;
	struct om_header_ctx *context;
{
	struct iovec iov[6];
	struct iovec *v;
	int l;
	char line[MAXLINE + 1], greetings[500];

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_classic_doLog: no message!");
		return(-1);
	}

	v = iov;
	if (f->f_type == F_WALL) {
		v->iov_base = greetings;
		v->iov_len = snprintf(greetings, sizeof(greetings),
		    "\r\n\7Message from syslogd@%s at %.24s ...\r\n",
		    f->f_prevhost, ctime(&now));
		if (v->iov_len >= sizeof(greetings))
			v->iov_len = sizeof(greetings) - 1;
		v++;
		v->iov_base = "";
		v->iov_len = 0;
		v++;
	} else {
		v->iov_base = f->f_lasttime;
		v->iov_len = 15;
		v++;
		v->iov_base = " ";
		v->iov_len = 1;
		v++;
	}
	v->iov_base = f->f_prevhost;
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	v->iov_base = msg;
	v->iov_len = strlen(msg);
	v++;

	dprintf("Logging to %s", TypeNames[f->f_type]);
	f->f_time = now;

	switch (f->f_type) {
		case F_UNUSED:
			dprintf("\n");
			break;
	
		case F_FORW:
			dprintf(" %s\n", f->f_un.f_forw.f_hname);
			l = snprintf(line, sizeof(line) - 1, "<%d>%.15s %s", f->f_prevpri,
			    (char *) iov[0].iov_base, (char *) iov[4].iov_base);
			if (l > MAXLINE)
				l = MAXLINE;
			if (sendto(finet, line, l, 0,
			    (struct sockaddr *)&f->f_un.f_forw.f_addr,
			    sizeof(f->f_un.f_forw.f_addr)) != l) {
				f->f_type = F_UNUSED;
				logerror("sendto");
			}
			break;

		case F_CONSOLE:
			if (flags & IGN_CONS) {
				dprintf(" (ignored)\n");
				break;
			}
			/* FALLTHROUGH */

		case F_TTY:
		case F_FILE:
			dprintf(" %s\n", f->f_un.f_fname);
			if (f->f_type != F_FILE) {
				v->iov_base = "\r\n";
				v->iov_len = 2;
			} else {
				v->iov_base = "\n";
				v->iov_len = 1;
			}
			again:
			if (writev(f->f_file, iov, 6) < 0) {
				int e = errno;
				(void)close(f->f_file);
				/*
				 * Check for errors on TTY's due to loss of tty
				 */
				if ((e == EIO || e == EBADF) && f->f_type != F_FILE) {
					f->f_file = open(f->f_un.f_fname,
					    O_WRONLY|O_APPEND, 0);
					if (f->f_file < 0) {
						f->f_type = F_UNUSED;
						logerror(f->f_un.f_fname);
					} else
						goto again;
				} else {
					f->f_type = F_UNUSED;
					f->f_file = -1;
					errno = e;
					logerror(f->f_un.f_fname);
				}
			} else if (flags & SYNC_FILE)
				(void)fsync(f->f_file);
			break;

		case F_USERS:
		case F_WALL:
			dprintf("\n");
			v->iov_base = "\r\n";
			v->iov_len = 2;
			wallmsg(f, iov);
			break;
	}
	f->f_prevcount = 0;
	return(0);
}


/*
 *  INIT -- Initialize om_classic
 *
 *  taken mostly from syslogd's cfline
 */
int
om_classic_init(argc, argv, f, prog, context)
	int argc;
	char **argv;
	struct filed *f;
	char *prog;
	struct om_header_ctx **context;
{
	struct hostent *hp;
	int i;
	char *p, *q;

	dprintf("om_classic init\n");

	p = argv[1];

	switch (*p)
	{
	case '@':
		if (!InetInuse)
			break;
		(void)strncpy(f->f_un.f_forw.f_hname, ++p,
		    sizeof(f->f_un.f_forw.f_hname)-1);
		f->f_un.f_forw.f_hname[sizeof(f->f_un.f_forw.f_hname)-1] = '\0';
		hp = gethostbyname(f->f_un.f_forw.f_hname);
		if (hp == NULL) {
			extern int h_errno;

			logerror((char *)hstrerror(h_errno));
			break;
		}
		memset(&f->f_un.f_forw.f_addr, 0,
		    sizeof(f->f_un.f_forw.f_addr));
#ifndef HAVE_LINUX
		f->f_un.f_forw.f_addr.sin_len = sizeof(f->f_un.f_forw.f_addr);
#endif
		f->f_un.f_forw.f_addr.sin_family = AF_INET;
		f->f_un.f_forw.f_addr.sin_port = LogPort;
		memmove(&f->f_un.f_forw.f_addr.sin_addr, hp->h_addr,
		    hp->h_length);
		f->f_type = F_FORW;
		break;

	case '/':
#error Buffer overflow!!! Si, esta leyendo de la configuracion, pero que pasa
#error si mas adelante hacemos que un auditor remotamente pueda cambiar la
#error configuracion... No es lo mismo conceptualmente poder configurar el
#error syslog que ser root... no
		(void)strcpy(f->f_un.f_fname, p);
		if ((f->f_file = open(p, O_WRONLY|O_APPEND, 0)) < 0) {
			f->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file))
			f->f_type = F_TTY;
		else
			f->f_type = F_FILE;
		if (strcmp(p, ctty) == 0)
			f->f_type = F_CONSOLE;
		break;

	case '*':
		f->f_type = F_WALL;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void)strncpy(f->f_un.f_uname[i], p, UT_NAMESIZE);
			if ((q - p) > UT_NAMESIZE)
				f->f_un.f_uname[i][UT_NAMESIZE] = '\0';
			else
				f->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		f->f_type = F_USERS;
		break;
	}

	return(1);
}

int
om_classic_close(f, context)
	struct filed *f;
	struct om_header_ctx **context;
{
	int ret;

	ret = -1;
	switch (f->f_type) {
		case F_FILE:
		case F_TTY:
		case F_CONSOLE:
			ret = close(f->f_file);
			break;
		case F_FORW:
			ret = 0;
			break;
	}

	return (ret);
}

int
om_classic_flush(f, context)
	struct filed *f;
	struct om_header_ctx *context;
{
	/* flush any pending output */
	if (f->f_prevcount)
		om_classic_doLog(f, 0, (char *)NULL, NULL);

	return(1);

}

/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
void
wallmsg(f, iov)
	struct filed *f;
	struct iovec *iov;
{
	static int reenter;			/* avoid calling ourselves */
	FILE *uf;
	struct utmp ut;
	int i;
	char *p;
	char line[sizeof(ut.ut_line) + 1];

	if (reenter++)
		return;
	if ((uf = fopen(_PATH_UTMP, "r")) == NULL) {
		logerror(_PATH_UTMP);
		reenter = 0;
		return;
	}
	/* NOSTRICT */
	while (fread((char *)&ut, sizeof(ut), 1, uf) == 1) {
		if (ut.ut_name[0] == '\0')
			continue;
		strncpy(line, ut.ut_line, sizeof(ut.ut_line));
		line[sizeof(ut.ut_line)] = '\0';
		if (f->f_type == F_WALL) {
			if ((p = ttymsg(iov, 6, line, TTYMSGTIME)) != NULL) {
				errno = 0;	/* already in msg */
				logerror(p);
			}
			continue;
		}
		/* should we send the message to this user? */
		for (i = 0; i < MAXUNAMES; i++) {
			if (!f->f_un.f_uname[i][0])
				break;
			if (!strncmp(f->f_un.f_uname[i], ut.ut_name,
			    UT_NAMESIZE)) {
				if ((p = ttymsg(iov, 6, line, TTYMSGTIME))
								!= NULL) {
					errno = 0;	/* already in msg */
					logerror(p);
				}
				break;
			}
		}
	}
	(void)fclose(uf);
	reenter = 0;
}

