/*	$CoreSDI: syslogd.h,v 1.67 2000/07/14 01:04:32 alejo Exp $	*/

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

#ifndef SYSLOGD_H
#define SYSLOGD_H

#define	MAXLINE		1024		/* maximum line length */
#define	MAXSVLINE	120		/* maximum saved line length */
#define DEFUPRI		(LOG_USER|LOG_NOTICE)
#define DEFSPRI		(LOG_KERN|LOG_CRIT)
#define TIMERINTVL	30		/* interval for checking flush, mark */
#define TTYMSGTIME	1		/* timeout passed to ttymsg */
#define MAX_N_OMODULES	20		/* maximum types of out modules */
#define MAX_N_IMODULES	10		/* maximum types of in  modules */

#define VERSION		1.0

/* Some default timeouts for select */
#define SYSLOG_TIMEOUT_SEC	10
#define SYSLOG_TIMEOUT_USEC	0
#define SYSLOG_TIMEOUT_MINUSEC	100

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif /* HAVE_PATHS_H */
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <utmp.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "modules.h"

#ifndef _PATH_KLOG
#define	_PATH_KLOG	"/dev/klog"
#endif

#define	_PATH_LOGCONF	"/etc/syslog.conf"

#define	dprintf		if (sglobals->Debug) printf

#define MAXUNAMES	20	/* maximum number of user names */

/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */


/*
 * maximum number of unix sockets
 */
#define MAXFUNIX	21

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
	struct  filed *f_next;	  /* next in linked list */
	short   f_type;		 /* entry type, see below */
	short   f_file;		 /* file descriptor */
	time_t  f_time;		 /* time this was last written */
	u_char  f_pmask[LOG_NFACILITIES+1];     /* priority mask */
	char    *f_program;	     /* program this applies to */
	union {
		char    f_uname[MAXUNAMES][UT_NAMESIZE+1];
		struct {
			char    f_hname[MAXHOSTNAMELEN];
			struct sockaddr_in      f_addr;
		} f_forw;	       /* forwarding address */
		char    f_fname[MAXPATHLEN];
	} f_un;
	char    f_prevline[MAXSVLINE];	  /* last message logged */
	char    f_lasttime[16];		 /* time of last occurrence */
	char    f_prevhost[MAXHOSTNAMELEN];     /* host from which recd. */
	int     f_prevpri;		      /* pri of f_prevline */
	int     f_prevlen;		      /* length of f_prevline */
	int     f_prevcount;		    /* repetition cnt of prevline */
	int     f_repeatcount;		  /* number of "repeated" msgs */
	struct	o_module *f_omod;			/* module details */
};

struct sglobals {
	char	*TypeNames[9];		/* names for f_types */
	char	*ctty;
	char	LocalHostName[MAXHOSTNAMELEN];	/* our hostname */
	char	*LocalDomain;	/* our domain */
	int	finet;		/* Internet datagram socket */
	int	LogPort;	/* port number for INET connections */
	int	Debug;			/* debug flag */
	int	InetInuse;		/* non-zero if INET sockets are open */
	void	(*logerror)(char *);
	void	(*logmsg)(int, char *, char *, int);
	void	(*die)(int);
	char * (*ttymsg)(struct iovec *, int , char *, int );
};

/* standard output module header variables in context */
struct om_hdr_ctx {
        short   flags;
#define OM_FLAG_INITIALIZED 0x1
#define OM_FLAG_ERROR   0x2
#define OM_FLAG_LOCKED  0x4
        int     size;
};

struct omodule {
	struct	omodule *om_next;
	char	*om_name;
	int	(*om_init) (int, char **, struct filed *, char *,
		struct om_hdr_ctx **, struct sglobals *);
	int	(*om_doLog) (struct filed *, int, char *,
		struct om_hdr_ctx *, struct sglobals *);
	int	(*om_flush) (struct filed *, struct om_hdr_ctx *, struct sglobals *);
	int	(*om_close) (struct filed *, struct om_hdr_ctx *, struct sglobals *);
	int	(*om_timer) (struct filed *, struct om_hdr_ctx *, struct sglobals *);
	void	*h;  /* handle to open dynamic library */
};

struct imodule {
	struct	imodule *im_next;
	char   *im_name;
	int	(*im_init) (struct i_module *, char **, int, struct sglobals *);
	int	(*im_close) (struct i_module *, struct sglobals *);
	int	(*im_getLog) (struct i_module *, struct im_msg *, struct sglobals *);
	int	(*im_timer) (struct i_module *, struct im_msg *, struct sglobals *);
	void	*h;  /* handle to open dynamic library */
};

	
#define	MAXREPEAT ((sizeof(repeatinterval) / sizeof(repeatinterval[0])) - 1)
#define	REPEATTIME(f)	((f)->f_time + repeatinterval[(f)->f_repeatcount])
#define	BACKOFF(f)	{ if (++(f)->f_repeatcount > MAXREPEAT) \
				 (f)->f_repeatcount = MAXREPEAT; \
			}

/* values for f_type */
#define F_UNUSED	0		/* unused entry */
#define F_FILE		1		/* regular file */
#define F_TTY		2		/* terminal */
#define F_CONSOLE	3		/* console terminal */
#define F_FORW		4		/* remote machine */
#define F_USERS		5		/* list of users */
#define F_WALL		6		/* everyone logged on */
#define F_MODULE	7		/* module only */

/* values for integrity facilities */
#define I_NONE		0
#define I_PEO		1
#define	I_VCR		2
#define	I_OTS		3
#define	I_SHA		4
#define DEFAULT_INTEG_FACILITY	I_NONE


#endif
