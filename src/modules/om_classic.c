/*	$CoreSDI: om_classic.c,v 1.63 2001/02/16 22:30:56 alejo Exp $	*/

/*
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

/*
 *  om_classic -- classic behaviour module
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *         from syslogd.c Eric Allman  and Ralph Campbell
 *
 */

#include "../../config.h"

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/uio.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <utmp.h>
/* if _PATH_UTMP isn't defined, define it here... */
#ifndef _PATH_UTMP
# ifdef UTMP_FILE
#  define _PATH_UTMP UTMP_FILE
# else  /* if UTMP_FILE */
#  define _PATH_UTMP "/var/adm/utmp"
# endif /* if UTMP_FILE */
#endif


#include "../../config.h"
#include "../modules.h"
#include "../syslogd.h"

#define TTYMSGTIME	1		/* timeout passed to ttymsg */

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_MODULE	7		/* module only */

/* names for f_types */
char    *TypeNames[] = { "UNUSED", "FILE", "TTY", "CONSOLE",
	    "FORW", "USERS", "WALL", "MODULE", NULL};
extern char     *ctty;


struct om_classic_ctx {
	union {
		char    f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char    f_hname[SIZEOF_MAXHOSTNAMELEN];
			struct sockaddr_in      f_addr;
		} f_forw;	      /* forwarding address */
		char    f_fname[MAXPATHLEN];
	} f_un;
	int   f_type;		 /* entry type, see below */
};

void wallmsg (struct filed *, struct iovec *, struct om_classic_ctx *c);
char *ttymsg(struct iovec *, int , char *, int);


/*
 * Write to file, tty, user and network udp
 */

int
om_classic_write(struct filed *f, int flags, char *msg, void *ctx)
{
	struct iovec iov[6];
	struct iovec *v;
	struct om_classic_ctx *c;
	int l;
	char line[MAXLINE + 1], greetings[500], time_buf[16];
	time_t now;

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_classic_write: no message!");
		return (-1);
	}

	c = (struct om_classic_ctx *) ctx;

	/* prepare buffers for writing */
	v = iov;
	if (c->f_type == F_WALL) {
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
		strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S", &f->f_tm);
		v->iov_base = time_buf;
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

	dprintf(DPRINTF_INFORMATIVE)("Logging to %s", TypeNames[c->f_type]);

	switch (c->f_type) {
	case F_UNUSED:
		dprintf(DPRINTF_INFORMATIVE)("\n");
		break;
	
	case F_FORW:
		if (finet < 0) {
			dprintf(DPRINTF_SERIOUS)("om_classic: write: "
			    "can't forward message, socket down\n");
			return(-1);
		}

		dprintf(DPRINTF_INFORMATIVE)(" %s\n", c->f_un.f_forw.f_hname);
		l = snprintf(line, sizeof(line), "<%d>%.15s %s", f->f_prevpri,
		    (char *) iov[0].iov_base, (char *) iov[4].iov_base);

		if (sendto(finet, line, l, 0,
		    (struct sockaddr *)&c->f_un.f_forw.f_addr,
		    sizeof(c->f_un.f_forw.f_addr)) != l) {
			c->f_type = F_UNUSED;
			logerror("sendto");
		}

		break;

	case F_CONSOLE:
		if (flags & IGN_CONS) {
			dprintf(DPRINTF_INFORMATIVE)(" (ignored)\n");
			break;
		}
		/* FALLTHROUGH */

	case F_TTY:
	case F_FILE:
		dprintf(DPRINTF_INFORMATIVE)(" %s\n", c->f_un.f_fname);
		if (c->f_type != F_FILE) {
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
			if ((e == EIO || e == EBADF) && c->f_type != F_FILE) {
				f->f_file = open(c->f_un.f_fname,
				    O_WRONLY|O_APPEND, 0);
				if (f->f_file < 0) {
					c->f_type = F_UNUSED;
					logerror(c->f_un.f_fname);
				} else
					goto again;
			} else {
				c->f_type = F_UNUSED;
				f->f_file = -1;
				errno = e;
				logerror(c->f_un.f_fname);
			}
		} else if (flags & SYNC_FILE)
			(void)fsync(f->f_file);
		break;

	case F_USERS:
	case F_WALL:
		dprintf(DPRINTF_INFORMATIVE)("\n");
		v->iov_base = "\r\n";
		v->iov_len = 2;
		wallmsg(f, iov, c);
		break;
	}
	f->f_prevcount = 0;

	return (1);
}


/*
 *  INIT -- Initialize om_classic
 *
 *  taken mostly from syslogd's cfline
 */
int
om_classic_init(int argc, char **argv, struct filed *f, char *prog, void **ctx)
{
	struct hostent *hp;
	struct  om_classic_ctx *c;
	int i;
	char *p, *q;

	dprintf(DPRINTF_INFORMATIVE)("om_classic init: Entering\n");

	if ((*ctx = (void *) calloc(1, sizeof(struct om_classic_ctx))) == NULL) {
		dprintf(DPRINTF_SERIOUS)("om_classic_init: cannot allocate "
		    "context\n");
		return (-1);
	}

	c = (struct om_classic_ctx *) *ctx;

	/* accepts "%classic /file" or "%classic -t TYPE /file" */
	if ( (argc != 2 && argc != 4) || argv == NULL) {
		dprintf(DPRINTF_SERIOUS)("om_classic_init: incorrect "
		    "parameters %d args\n", argc);
		free(*ctx);
		*ctx = NULL;
		return (-1);
	}

	if (argc > 2) {
		int i;

		if (strncmp(argv[1], "-t", 2)) {
			dprintf(DPRINTF_SERIOUS)("om_classic_init: incorrect" 
			    " parameter %s, should be '-t' %d\n", argv[1], i);
			free(*ctx);
			*ctx = NULL;
			return (-1);
		}

		/* look for entry # in table */
		for (i = 0; TypeNames[i] && strncmp(TypeNames[i], argv[2],
		    strlen(argv[2])); i++);
		if (TypeNames[i] == NULL) {
			dprintf(DPRINTF_SERIOUS)("om_classic_init: couldn't" 
			    " determine type %s\n", argv[2]);
			free(*ctx);
			*ctx = NULL;
			return (-1);
		}

		c->f_type = i;
		p = argv[3];

	} else
		/* regular config line, no type */
		p = argv[1];

	switch (*p) {
	case '@':
		if (!(DaemonFlags & SYSLOGD_INET_IN_USE)) {
			struct sockaddr_in sin;
			struct servent *sp;

			finet = socket(AF_INET, SOCK_DGRAM, 0);

			sp = getservbyname("syslog", "udp");
			if (sp == NULL) {
				errno = 0;
				logerror("syslog/udp: unknown service");
				free(*ctx);
				*ctx = NULL;
   				return (-1);
			}
			memset(&sin, 0, sizeof(sin));
			sin.sin_family = AF_INET;
			sin.sin_port = LogPort = sp->s_port;

			if (bind(finet, (struct sockaddr *)&sin,
					sizeof(sin)) < 0) {
				logerror("bind");
				free(*ctx);
				*ctx = NULL;
				return (-1);
			} else {
				DaemonFlags |= SYSLOGD_INET_IN_USE;
			}
		}

		strncpy(c->f_un.f_forw.f_hname, ++p,
		    sizeof(c->f_un.f_forw.f_hname) - 1);
		c->f_un.f_forw.f_hname[sizeof(c->f_un.f_forw.f_hname) - 1] = '\0';
		hp = gethostbyname(c->f_un.f_forw.f_hname);
		if (hp == NULL) {
			extern int h_errno;

			logerror((char *)hstrerror(h_errno));
			break;
		}

		c->f_un.f_forw.f_addr.sin_family = AF_INET;
		c->f_un.f_forw.f_addr.sin_port = LogPort;
		memmove(&c->f_un.f_forw.f_addr.sin_addr, hp->h_addr,
		    sizeof(struct in_addr));
		c->f_type = F_FORW;
		break;

	case '/':
		strncpy(c->f_un.f_fname, p, sizeof c->f_un.f_fname);
		c->f_un.f_fname[sizeof (c->f_un.f_fname) - 1]=0;
		if ((f->f_file = open(p, O_WRONLY|O_APPEND, 0)) < 0) {
			c->f_type = F_UNUSED;
			logerror(p);
			break;
		}
		if (isatty(f->f_file))
			c->f_type = F_TTY;
		else
			c->f_type = F_FILE;
		if (strcmp(p, ctty) == 0)
			c->f_type = F_CONSOLE;
		break;

	case '*':
		c->f_type = F_WALL;
		break;

	default:
		for (i = 0; i < MAXUNAMES && *p; i++) {
			for (q = p; *q && *q != ','; )
				q++;
			(void)strncpy(c->f_un.f_uname[i], p, UT_NAMESIZE);
			if ((q - p) > UT_NAMESIZE)
				c->f_un.f_uname[i][UT_NAMESIZE] = '\0';
			else
				c->f_un.f_uname[i][q - p] = '\0';
			while (*q == ',' || *q == ' ')
				q++;
			p = q;
		}
		c->f_type = F_USERS;
		break;
	}

	return (1);
}

int
om_classic_close(struct filed *f, void *ctx)
{
	struct om_classic_ctx *c;

	c = (struct om_classic_ctx *) ctx;

	switch (c->f_type) {
	case F_FILE:
	case F_TTY:
	case F_CONSOLE:
		return (close(f->f_file));
	case F_FORW:
		if ((finet > -1) && (DaemonFlags & SYSLOGD_INET_IN_USE)
		    && !(DaemonFlags & SYSLOGD_INET_READ))
			return (close(finet));
	default:
		return (0);
	}
}

int
om_classic_flush(struct filed *f, void *ctx)
{
	/* flush any pending output */
	if (f->f_prevcount)
		om_classic_write(f, 0, (char *)NULL, NULL);

	return (1);

}

/*
 *  WALLMSG -- Write a message to the world at large
 *
 *	Write the specified message to either the entire
 *	world, or a list of approved users.
 */
void
wallmsg( struct filed *f, struct iovec *iov, struct om_classic_ctx *c)
{
	static int reenter;			/* avoid calling ourselves */
	FILE *uf;
	struct utmp ut;
	int i;
	char *p;
	char line[sizeof(ut.ut_line) + 1];

	if (reenter++)
		return;
	if ( (uf = fopen(_PATH_UTMP, "r")) == NULL) {
		logerror(_PATH_UTMP);
		reenter = 0;
		return;
	}
	/* NOSTRICT */
	while (fread(&ut, sizeof(ut), 1, uf) == 1) {

#ifndef __linux__
		if (ut.ut_name[0] == '\0')
#else
		if ((ut.ut_type != USER_PROCESS && ut.ut_type != LOGIN_PROCESS) ||
		    ut.ut_line[0] == ':' /* linux logs users that are not logged in (?!) */)
#endif
			continue;

		strncpy(line, ut.ut_line, sizeof(ut.ut_line));
		line[sizeof(ut.ut_line)] = '\0';
		if (c->f_type == F_WALL) {
			if ((p = ttymsg(iov, 6, line, TTYMSGTIME)) != NULL) {
				errno = 0;	/* already in msg */
				logerror(p);
			}
			continue;
		}
		/* should we send the message to this user? */
		for (i = 0; i < MAXUNAMES; i++) {
			if (!c->f_un.f_uname[i][0])
				break;
			if (!strncmp(c->f_un.f_uname[i], ut.ut_name,
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
	fclose(uf);
	reenter = 0;
}
