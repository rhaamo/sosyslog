/*	$Id: syslogd.c,v 1.42 2000/04/19 22:04:30 alejo Exp $
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";*/
static char rcsid[] = "$NetBSD: syslogd.c,v 1.5 1996/01/02 17:48:41 perry Exp $";
#endif /* not lint */

/*
 *  syslogd -- log system messages
 *
 * This program implements a system log. It takes a series of lines.
 * Each line may have a priority, signified as "<n>" as
 * the first characters of the line.  If this is
 * not present, a default priority is used.
 *
 * To kill syslogd, send a signal 15 (terminate).  A signal 1 (hup) will
 * cause it to reread its configuration file.
 *
 * Defined Constants:
 *
 * MAXLINE -- the maximimum line length that can be handled.
 * DEFUPRI -- the default priority for user messages
 * DEFSPRI -- the default priority for kernel messages
 *
 * Author: Eric Allman
 * extensive changes by Ralph Campbell
 * more extensive changes by Eric Allman (again)
 */

#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/resource.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SYSLOG_NAMES
#include <sys/syslog.h>
#include "syslogd.h"

char	*ConfFile = _PATH_LOGCONF;
char	*PidFile = _PATH_LOGPID;
char	ctty[] = _PATH_CONSOLE;

/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */

char	*TypeNames[8] = {
	"UNUSED",	"FILE",		"TTY",		"CONSOLE",
	"FORW",		"USERS",	"WALL",		"MODULE"
};

struct	filed *Files;
struct	filed consfile;

int	Debug;			/* debug flag */
char	LocalHostName[MAXHOSTNAMELEN];	/* our hostname */
char	*LocalDomain;		/* our local domain name */
int	InetInuse = 0;		/* non-zero if INET sockets are being used */
int	finet;			/* Internet datagram socket */
int	LogPort;		/* port number for INET connections */
int	Initialized = 0;	/* set when we have initialized ourselves */
int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
int	SecureMode = 1;		/* when true, speak only unix domain socks */

void	cfline __P((char *, struct filed *, char *));
int	decode __P((const char *, CODE *));
void	die __P((int));
void	domark __P((int));
void	doLog __P((struct filed *, int, char *));
void	init __P((int));
void	logerror __P((char *));
void	logmsg __P((int, char *, char *, int));
void	printline __P((char *, char *));
void	reapchild __P((int));
void	usage __P((void));

int nfunix = 1;
char *funixn[MAXFUNIX] = { _PATH_LOG };
int funix[MAXFUNIX];

int
main(argc, argv)
	int argc;
	char *argv[];
{
	int ch;
	FILE *fp;
	char *p;
	struct	i_module *Inputs;
	int	inputBSD, inputSYSV;

	inputBSD = 0; inputSYSV = 0;

	while ((ch = getopt(argc, argv, "dubSf:m:p:a:")) != -1)
		switch (ch) {
		case 'd':		/* debug */
			Debug++;
			break;
		case 'f':		/* configuration file */
			ConfFile = optarg;
			break;
		case 'm':		/* mark interval */
			MarkInterval = atoi(optarg) * 60;
			break;
		case 'p':		/* path */
			funixn[0] = optarg;
			break;
		case 'u':		/* allow udp input port */
			SecureMode = 0;
			break;
		case 'i':		/* BSD-like input (AF_LOCAL socket) */
			if (!strncmp(optarg, "bsd", 3)) {
				inputBSD++;
			} else if (!strncmp(optarg, "sysv", 4)) {
				inputSYSV++;
			} else
				usage();
			break;
		case 'a':		/* additional AF_UNIX socket name */
			if (nfunix < MAXFUNIX)
				funixn[nfunix++] = optarg;
			else
				fprintf(stderr,
				    "syslogd: out of descriptors, ignoring %s\n",
				    optarg);
			break;
		case '?':
		default:
			usage();
		}
	if ((argc -= optind) != 0)
		usage();

	if (!Debug)
		(void)daemon(0, 0);
	else
		setlinebuf(stdout);

	/* assign functions and init input */
	modules_init(&Inputs);

	consfile.f_type = F_CONSOLE;
        /* this should get into Files and be way nicer */
        consfile.f_mod = (struct o_module *) calloc(1, sizeof(struct o_module));
        consfile.f_mod->om_type = M_CLASSIC;

	(void)strcpy(consfile.f_un.f_fname, ctty);
	(void)gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.')) != NULL) {
		*p++ = '\0';
		LocalDomain = p;
	} else
		LocalDomain = "";
	(void)signal(SIGTERM, die);
	(void)signal(SIGINT, Debug ? die : SIG_IGN);
	(void)signal(SIGQUIT, Debug ? die : SIG_IGN);
	(void)signal(SIGCHLD, reapchild);
	(void)signal(SIGALRM, domark);
	(void)alarm(TIMERINTVL);

	/* tuck my process id away */
	if (!Debug) {
		fp = fopen(PidFile, "w");
		if (fp != NULL) {
			fprintf(fp, "%d\n", getpid());
			(void) fclose(fp);
		}
	}

	dprintf("off & running....\n");

	init(0);
	(void)signal(SIGHUP, init);

	for (;;) {
		fd_set readfds;
		int nfds = 0;
		struct i_module *im;

		FD_ZERO(&readfds);
		for (im = Inputs; im ; im = im->im_next) {
			if (im->fd != -1) {
				FD_SET(im->fd, &readfds);
				if (im->fd > nfds)
					nfds = im->fd;
			}
		}

		/*dprintf("readfds = %#x\n", readfds);*/
		nfds = select(nfds+1, &readfds, (fd_set *)NULL,
		    (fd_set *)NULL, (struct timeval *)NULL);
		if (nfds == 0)
			continue;
		if (nfds < 0) {
			if (errno != EINTR)
				logerror("select");
			continue;
		}
		/*dprintf("got a message (%d, %#x)\n", nfds, readfds);*/
		for (im = Inputs; im ; im = im->im_next) {
		   if (im->fd != -1 && FD_ISSET(im->fd, &readfds)) {
		       struct im_msg *log;
		       int i;

		       log = (struct im_msg *) calloc(sizeof(struct im_msg), 1);

		       for (i = 0; i < MAX_N_IMODULES &&
					(im->im_type != IModules[i].im_type); i++);
		       if (i == MAX_N_IMODULES) {
		       		dprintf("Error getting input module %i on IMODULES\n",
						im->im_type);
		       		continue;
		       }

		       if ((*(IModules[i].im_getLog))(im->fd, log) == -1) {
		       	dprintf("Syslogd: error calling input module"
		       		" %s, for fd %d\n", im->im_name,
		       		im->fd);
		       }

		       /* log it */
		       printline(log->host, log->msg);
		       if (log->host != NULL) free(log->host);
		       if (log->msg != NULL) free(log->msg);
		       free(log->msg);
		   }
		}

	}
}

void
usage()
{

	(void)fprintf(stderr,
	    "usage: syslogd [-u] [-f conffile] [-m markinterval] [-p logpath] [-a logpath]\n");
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(hname, msg)
	char *hname;
	char *msg;
{
	int c, pri;
	char *p, *q, line[MAXLINE + 1];

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit(*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

	/* don't allow users to log kernel messages */
	if (LOG_FAC(pri) == LOG_KERN)
		pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));

	q = line;

	while ((c = *p++ & 0177) != '\0' &&
	    q < &line[sizeof(line) - 1])
		if (iscntrl(c))
			if (c == '\n')
				*q++ = ' ';
			else if (c == '\t')
				*q++ = '\t';
			else {
				*q++ = '^';
				*q++ = c ^ 0100;
			}
		else
			*q++ = c;
	*q = '\0';

	logmsg(pri, line, hname, 0);
}

time_t	now;

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(pri, msg, from, flags)
	int pri;
	char *msg, *from;
	int flags;
{
	struct filed *f;
	int fac, msglen, prilev, i;
	sigset_t mask, omask;
	char *timestamp;
 	char prog[NAME_MAX+1];

	dprintf("logmsg: pri 0%o, flags 0x%x, from %s, msg %s\n",
	    pri, flags, from, msg);

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGHUP);
	sigprocmask(SIG_BLOCK, &mask, &omask);

	/*
	 * Check to see if msg looks non-standard.
	 */
	msglen = strlen(msg);
	if (msglen < 16 || msg[3] != ' ' || msg[6] != ' ' ||
	    msg[9] != ':' || msg[12] != ':' || msg[15] != ' ')
		flags |= ADDDATE;

	(void)time(&now);
	if (flags & ADDDATE)
		timestamp = ctime(&now) + 4;
	else {
		timestamp = msg;
		msg += 16;
		msglen -= 16;
	}

	/* extract facility and priority level */
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	else
		fac = LOG_FAC(pri);
	prilev = LOG_PRI(pri);

	/* extract program name */
	for(i = 0; i < NAME_MAX; i++) {
		if (!isalnum(msg[i]))
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

	/* log the message to the particular outputs */
	if (!Initialized) {
		f = &consfile;
		f->f_file = open(ctty, O_WRONLY, 0);

		if (f->f_file >= 0) {
			doLog(f, flags, msg);
			(void)close(f->f_file);
			f->f_file = -1;
		}
		(void)sigprocmask(SIG_SETMASK, &omask, NULL);
		return;
	}
	for (f = Files; f; f = f->f_next) {
		/* skip messages that are incorrect priority */
		if (f->f_pmask[fac] < prilev ||
		    f->f_pmask[fac] == INTERNAL_NOPRI)
			continue;

		/* skip messages with the incorrect program name */
		if (f->f_program)
			if (strcmp(prog, f->f_program) != 0)
				continue;

		if (f->f_type == F_CONSOLE && (flags & IGN_CONS))
			continue;

		/* don't output marks to recently written files */
		if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/*
		 * suppress duplicate lines to this file
		 */
		if ((flags & MARK) == 0 && msglen == f->f_prevlen &&
		    !strcmp(msg, f->f_prevline) &&
		    !strcmp(from, f->f_prevhost)) {
			(void)strncpy(f->f_lasttime, timestamp, 15);
			f->f_prevcount++;
			dprintf("msg repeated %d times, %ld sec of %d\n",
			    f->f_prevcount, (long)(now - f->f_time),
			    repeatinterval[f->f_repeatcount]);
			/*
			 * If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often
			 * in the future.
			 */
			if (now > REPEATTIME(f)) {
				doLog(f, flags, (char *)NULL);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */
			if (f->f_prevcount)
				doLog(f, 0, (char *)NULL);
			f->f_repeatcount = 0;
			f->f_prevpri = pri;
			(void)strncpy(f->f_lasttime, timestamp, 15);
			(void)strncpy(f->f_prevhost, from,
					sizeof(f->f_prevhost)-1);
			f->f_prevhost[sizeof(f->f_prevhost)-1] = '\0';
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				(void)strcpy(f->f_prevline, msg);
				doLog(f, flags, (char *)NULL);
			} else {
				f->f_prevline[0] = 0;
				f->f_prevlen = 0;
				doLog(f, flags, msg);
			}
		}
	}
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
}

void
doLog(f, flags, msg)
        struct filed *f;
        int flags;
        char *msg;
{
	struct	o_module *m;

        for (m = f->f_mod; m; m = m->om_next) {
		if(OModules[m->om_type].om_doLog == NULL) {
			dprintf("Unsupported module type [%i] "
			        "for message [%s]\n", m->om_type, msg);
			continue;
		};

		/* call this module doLog */
		if((*(OModules[m->om_type].om_doLog))(f,flags,msg,m->context) != 0) {
			dprintf("doLog error with module type [%i] "
			        "for message [%s]\n",
				m->om_type, msg);
		}
	}
}


void
reapchild(signo)
	int signo;
{
	union wait status;
	int save_errno = errno;

	while (wait3((int *)&status, WNOHANG, (struct rusage *)NULL) > 0)
		;
	errno = save_errno;
}

void
domark(signo)
	int signo;
{
	struct filed *f;

	now = time((time_t *)NULL);
	MarkSeq += TIMERINTVL;
	if (MarkSeq >= MarkInterval) {
		logmsg(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE|MARK);
		MarkSeq = 0;
	}

	for (f = Files; f; f = f->f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			dprintf("flush %s: repeated %d times, %d sec.\n",
			    TypeNames[f->f_type], f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			doLog(f, 0, (char *)NULL);
			BACKOFF(f);
		}
	}
	(void)alarm(TIMERINTVL);
}

/*
 * Print syslogd errors some place.
 */
void
logerror(type)
	char *type;
{
	char buf[100];

	if (errno)
		(void)snprintf(buf, sizeof(buf), "syslogd: %s: %s",
		    type, strerror(errno));
	else
		(void)snprintf(buf, sizeof(buf), "syslogd: %s", type);
	errno = 0;
	dprintf("%s\n", buf);
	logmsg(LOG_SYSLOG|LOG_ERR, buf, LocalHostName, ADDDATE);
}

void
die(signo)
	int signo;
{
	struct filed *f;
	int was_initialized = Initialized;
	char buf[100];
	int i;

	Initialized = 0;		/* Don't log SIGCHLDs */
	for (f = Files; f != NULL; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			doLog(f, 0, (char *)NULL);
	}
	Initialized = was_initialized;
	if (signo) {
		dprintf("syslogd: exiting on signal %d\n", signo);
		(void)sprintf(buf, "exiting on signal %d", signo);
		errno = 0;
		logerror(buf);
	}
	for (i = 0; i < nfunix; i++)
		if (funixn[i] && funix[i] != -1)
			(void)unlink(funixn[i]);
	exit(0);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
void
init(signo)
	int signo;
{
	int i;
	FILE *cf;
	struct filed *f, *next, **nextp;
	char *p;
	char cline[LINE_MAX];
 	char prog[NAME_MAX+1];
	struct o_module *m;

	dprintf("init\n");

	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	for (f = Files; f != NULL; f = next) {
		/* flush any pending output */
		if (f->f_prevcount)
			doLog(f, 0, (char *)NULL);

		for (m = f->f_mod; m; m = m->om_next) {
			/* flush any pending output */
			if (f->f_prevcount &&
			    OModules[m->om_type].om_flush != NULL) {
				(*OModules[m->om_type].om_flush) (f,m->context);
			}

			if (OModules[m->om_type].om_close != NULL) {
				(*OModules[m->om_type].om_close) (f,&(m->context));
			}
		}
		next = f->f_next;
		if (f->f_program)
			free(f->f_program);
		free((char *)f);
	}
	Files = NULL;
	nextp = &Files;

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		dprintf("cannot open %s\n", ConfFile);
		*nextp = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.ERR\t/dev/console", *nextp, "*");
		(*nextp)->f_next = (struct filed *)calloc(1, sizeof(*f));
		cfline("*.PANIC\t*", (*nextp)->f_next, "*");
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	strcpy(prog, "*");
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. #!prog  and !prog are treated
		 * specially: the following lines apply only to that program.
		 */
		for (p = cline; isspace(*p); ++p)
			continue;
		if (*p == '\0')
			continue;
		if (*p == '#') {
			p++;
			if (*p != '!')
				continue;
		}
		if (*p == '!') {
			p++;
			while (isspace(*p))
				p++;
			if (!*p) {
				strcpy(prog, "*");
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isalnum(p[i]))
					break;
				prog[i] = p[i];
			}
			prog[i] = 0;
			continue;
		}
		p = cline + strlen(cline);
		while (p > cline)
			if (!isspace(*--p)) {
				p++;
				break;
			}
		*p = '\0';
		f = (struct filed *)calloc(1, sizeof(*f));
		*nextp = f;
		nextp = &f->f_next;
		cfline(cline, f, prog);
	}

	/* close the configuration file */
	(void)fclose(cf);

	Initialized = 1;

	if (Debug) {
		for (f = Files; f; f = f->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("%s: ", TypeNames[f->f_type]);
			switch (f->f_type) {
			case F_FILE:
			case F_TTY:
			case F_CONSOLE:
				printf("%s", f->f_un.f_fname);
				break;

			case F_FORW:
				printf("%s", f->f_un.f_forw.f_hname);
				break;

			case F_USERS:
				for (i = 0; i < MAXUNAMES && *f->f_un.f_uname[i]; i++)
					printf("%s, ", f->f_un.f_uname[i]);
				break;
			}
			if (f->f_program)
				printf(" (%s)", f->f_program);
			printf("\n");
		}
	}

	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE);
	dprintf("syslogd: restarted\n");
}

/*
 * Crack a configuration file line
 */
void
cfline(line, f, prog)
	char *line;
	struct filed *f;
	char *prog;
{
	int i, pri;
	char *bp, *p, *q;
	char buf[MAXLINE], ebuf[100];

	dprintf("cfline(\"%s\", f, \"%s\")\n", line, prog);

	errno = 0;	/* keep strerror() stuff out of logerror messages */

	/* clear out file entry */
	memset(f, 0, sizeof(*f));
	for (i = 0; i <= LOG_NFACILITIES; i++)
		f->f_pmask[i] = INTERNAL_NOPRI;

	/* save program name if any */
	if (!strcmp(prog, "*"))
		prog = NULL;
	else {
		f->f_program = calloc(1, strlen(prog)+1);
		if (f->f_program)
			strcpy(f->f_program, prog);
	}

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t,;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(", ;", *q))
			q++;

		/* decode priority name */
		if (*buf == '*')
			pri = LOG_PRIMASK + 1;
		else {
			/* ignore trailing spaces */
			int i;
			for (i=strlen(buf)-1; i >= 0 && buf[i] == ' '; i--) {
				buf[i]='\0';
			}

			pri = decode(buf, prioritynames);
			if (pri < 0) {
				(void)snprintf(ebuf, sizeof ebuf,
				    "unknown priority name \"%s\"", buf);
				logerror(ebuf);
				return;
			}
		}

		/* scan facilities */
		while (*p && !strchr("\t.;", *p)) {
			for (bp = buf; *p && !strchr("\t,;.", *p); )
				*bp++ = *p++;
			*bp = '\0';
			if (*buf == '*')
				for (i = 0; i < LOG_NFACILITIES; i++)
					f->f_pmask[i] = pri;
			else {
				i = decode(buf, facilitynames);
				if (i < 0) {
					(void)snprintf(ebuf, sizeof(ebuf),
					    "unknown facility name \"%s\"",
					    buf);
					logerror(ebuf);
					return;
				}
				f->f_pmask[i >> 3] = pri;
			}
			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t')
		p++;

	if (omodule_create(p, f, NULL) == -1) {
		dprintf("Error initializing modules!\n");
	}
}


/*
 *  Decode a symbolic name to a numeric value
 */
int
decode(name, codetab)
	const char *name;
	CODE *codetab;
{
	CODE *c;
	char *p, buf[40];

	if (isdigit(*name))
		return (atoi(name));

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper(*name))
			*p = tolower(*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}
