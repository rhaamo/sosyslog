
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
#define MAX_N_MODULES	64		/* maximum types of modules */

#include <paths.h>
#include <utmp.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#define	_PATH_KLOG	"/dev/klog"
#define	_PATH_LOGCONF	"/etc/syslog.conf"
#define	_PATH_LOGPID	"/var/run/syslog.pid"

#define	dprintf		if (Debug) printf

#define MAXUNAMES	20	/* maximum number of user names */

/*
 * Flags to logmsg().
 */

#define IGN_CONS	0x001	/* don't print on console */
#define SYNC_FILE	0x002	/* do fsync on file after printing */
#define ADDDATE		0x004	/* add a date to the message */
#define MARK		0x008	/* this message is a mark */


/*
 * Keyword for unsing modules instead of classic code
 */

#define S_MODULE_KEYWORD "s_mod"
#define MAX_MODULE_NAME_LEN 255

/* standard module header variables in context */
struct m_header {
	short	flags;
#define M_FLAG_INITIALIZED 0x1
#define M_FLAG_ERROR 0x2
#define M_FLAG_LOCKED 0x4
#define M_FLAG_ROTATING 0x8
	int	size;
};

/*
 * This structure represents main details for the output modules
 */

struct o_module {
	struct	o_module *m_next;
	short	m_type;
	struct m_header	*context;
};

/*
 * This structure represents the files that will have log
 * copies printed.
 */

struct filed {
        struct  filed *f_next;          /* next in linked list */
        short   f_type;                 /* entry type, see below */
        short   f_file;                 /* file descriptor */
        time_t  f_time;                 /* time this was last written */
        u_char  f_pmask[LOG_NFACILITIES+1];     /* priority mask */
        char    *f_program;             /* program this applies to */
        union {
                char    f_uname[MAXUNAMES][UT_NAMESIZE+1];
                struct {
                        char    f_hname[MAXHOSTNAMELEN];
                        struct sockaddr_in      f_addr;
                } f_forw;               /* forwarding address */
                char    f_fname[MAXPATHLEN];
        } f_un;
        char    f_prevline[MAXSVLINE];          /* last message logged */
        char    f_lasttime[16];                 /* time of last occurrence */
        char    f_prevhost[MAXHOSTNAMELEN];     /* host from which recd. */
        int     f_prevpri;                      /* pri of f_prevline */
        int     f_prevlen;                      /* length of f_prevline */
        int     f_prevcount;                    /* repetition cnt of prevline */
        int     f_repeatcount;                  /* number of "repeated" msgs */
        struct	o_module *f_mod;			/* module details */
};

struct	Modules {
	char	*m_name;
	short	m_type;
	int	(*m_doLog) (struct filed *, int, char *, struct m_header *);
	int	(*m_init) (int, char **, struct filed *, char *, struct m_header **);
	int	(*m_close) (struct filed *, struct m_header **);
	int	(*m_flush) (struct filed *, struct m_header *);
} Modules[MAX_N_MODULES];

int modules_init();
int modules_create(char *, struct filed *, char *);

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

/* values for m_type */
#define	M_CLASSIC	0
#define	M_MYSQL		1
#define	M_PEO		2

/* values for integrity facilities */
#define I_NONE		0
#define I_PEO		1
#define	I_VCR		2
#define	I_OTS		3
#define	I_SHA		4
#define DEFAULT_INTEG_FACILITY	I_NONE


time_t now;

#endif
