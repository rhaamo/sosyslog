/*	$CoreSDI: syslogd.c,v 1.138 2000/09/27 22:10:28 alejo Exp $	*/

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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "@(#)syslogd.c	8.3 (Core-SDI) 7/7/00";*/
static char rcsid[] = "$CoreSDI: syslogd.c,v 1.138 2000/09/27 22:10:28 alejo Exp $";
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
 * extensive changes by Alejo Sanchez for Core-SDI
 *
 */

#include "config.h"

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
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SYSLOG_NAMES
#include <syslog.h>
#include "syslogd.h"
#include "config.h"

#ifdef PATH_MAX
#define PID_PATH_MAX PATH_MAX
#else
#include <limits.h>
#ifdef _POSIX_PATH_MAX
#define PID_PATH_MAX _POSIX_PATH_MAX
#warning Using _POSIX_PATH_MAX as maximum pidfile name length
#else
#error No max path defined
#endif /* +POSIX_PATH_MAX */
#endif /* PATH_MAX */

#ifndef _PATH_CONSOLE
#define _PATH_CONSOLE "/dev/console"
#warning Using _PATH_CONSOLE as "/dev/console"
#endif /* _PATH_CONSOLE */
  
#ifndef NAME_MAX
#define NAME_MAX 255
#warning Using NAME_MAX as 255
#endif /* NAME_MAX */
  
#ifndef INTERNAL_NOPRI
#define INTERNAL_NOPRI 0x10
#warning Using INTERNAL_NOPRI as 0x10
#endif /* INTERNAL_NOPRI */

#ifndef HAVE_CODE_TYPEDEF
typedef struct _code {
        char    *c_name;
        int     c_val;
} CODE;
#endif
 
#ifdef NEED_PRIORITYNAMES
CODE prioritynames[] = {
        { "alert",      LOG_ALERT },
        { "crit",       LOG_CRIT },
        { "debug",      LOG_DEBUG },
        { "emerg",      LOG_EMERG },
        { "err",        LOG_ERR },
        { "error",      LOG_ERR },              /* DEPRECATED */
        { "info",       LOG_INFO },
        { "none",       INTERNAL_NOPRI },       /* INTERNAL */
        { "notice",     LOG_NOTICE },
        { "panic",      LOG_EMERG },            /* DEPRECATED */
        { "warn",       LOG_WARNING },          /* DEPRECATED */
        { "warning",    LOG_WARNING },
        { NULL,         -1 },
};
#warning defined prioritynames
#endif /* prioritynames */


/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int	repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */


struct	filed *Files;
struct	filed consfile;
struct	log_buf *log_queue = NULL;
int		log_queue_in = 0;
#define LOG_QUEUE_MAX	100;

int	Initialized = 0;	/* set when we have initialized ourselves */
int	MarkInterval = 20 * 60;	/* interval between marks in seconds */
int	MarkSeq = 0;		/* mark sequence number */
char	*ConfFile = _PATH_LOGCONF;	/* configuration file */
#define MAX_PIDFILE_LOCK_TRIES 5
char pidfile[PATH_MAX];
FILE *pidf = NULL;

char *ctty = _PATH_CONSOLE;
char *TypeNames[] = { "UNUSED", "FILE", "TTY", "CONSOLE", "FORW",
	"USERS", "WALL", "MODULE", NULL };
char  LocalHostName[MAXHOSTNAMELEN];  /* our hostname */
char *LocalDomain = NULL;
int finet = -1;
int LogPort = -1;
int Debug = 0;
int DaemonFlags = 0; /* running daemon flags */

char *libdir = NULL;

void    cfline(char *, struct filed *, char *);
int     decode(const char *, CODE *);
void    domark(int);
void    doLog(struct filed *, int, char *);
void    init(int);
void    printline(char *, char *, int);
void    reapchild(int);
void    usage(void);
int     modules_load(void);
int     imodule_init(struct i_module *, char *);
int     omodule_create(char *, struct filed *, char *);
void    logerror(char *);
void    logmsg(int, char *, char *, int);
void    die(int);
char   *ttymsg (struct iovec *, int, char *, int);


extern	struct  omodule *omodules;
extern	struct  imodule *imodules;
struct	i_module Inputs;

int
main(int argc, char **argv) {
	int ch;
	char *p;
	struct timeval timeout, tnow, nextcall;

	memset(&Inputs, 0, sizeof(Inputs));

	Inputs.im_fd = -1;

	timeout.tv_sec = SYSLOG_TIMEOUT_SEC;
	timeout.tv_usec = SYSLOG_TIMEOUT_USEC;

	/* assign functions and init input */
	if ((ch = modules_load()) < 0) {
		dprintf("Error loading static modules [%d]\n", ch);
		exit(-1);
	}

	while ((ch = getopt(argc, argv, "dubSf:m:p:a:i:l:h")) != -1)
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
		case 'u':		/* allow udp input port */
			if (imodule_init(&Inputs, "udp") < 0) {
				fprintf(stderr, "syslogd: error on udp input module\n");
				exit(-1);
			}
			break;
		case 'i':		/* inputs */
			if (imodule_init(&Inputs, optarg) < 0) {
				fprintf(stderr, "syslogd: error on input module, ignoring"
						"%s\n", optarg);
				exit(-1);
			}
			break;
		case 'p':		/* path */
		case 'a':		/* additional AF_UNIX socket name */
			if (optarg != NULL) {
			    char buf[512];

			    snprintf(buf, 512, "unix %s", optarg); 
			    if (imodule_init(&Inputs, buf) < 0) {
				fprintf(stderr,
					"syslogd: out of descriptors, ignoring %s\n",
					optarg);
			        exit(-1);
			    }
			}
			break;
		case 'l':
			libdir = optarg;
			break;
		case '?':
		case 'h':
		default:
			usage();
		}
	if (((argc -= optind) != 0) || Inputs.im_fd < 0)
		usage();

	if (!Debug)
		(void)daemon(0, 0);
	else
		setlinebuf(stdout);

	if (!(DaemonFlags & SYSLOGD_CONSOLE_ACTIVE)) {
		/* this should get into Files and be way nicer */
		if (omodule_create(ctty, &consfile, NULL) == -1) {
			dprintf("Error initializing console output!\n");
		} else
			DaemonFlags |= SYSLOGD_CONSOLE_ACTIVE;
	}

	/* get our hostname */
	(void)gethostname(LocalHostName, sizeof(LocalHostName));
	if ((p = strchr(LocalHostName, '.')) != NULL) {
		*p++ = '\0';
		LocalDomain = p;
	} else
		LocalDomain = "";

	/* define signal actions */
	(void)signal(SIGTERM, die);
	(void)signal(SIGINT, Debug ? die : SIG_IGN);
	(void)signal(SIGQUIT, Debug ? die : SIG_IGN);
	(void)signal(SIGCHLD, reapchild);
	(void)signal(SIGALRM, domark);
	(void)alarm(TIMERINTVL);

	snprintf(pidfile, PATH_MAX, "%s/syslog.pid", PID_DIR);

	/* took my process id away */
	if (!Debug) {
		struct flock fl;
		int lfd, tries, status;
		char buf[1024];

		fl.l_type   = F_WRLCK;
		fl.l_whence = SEEK_SET; /* relative to bof */
		fl.l_start  = 0L; /* from offset zero */
		fl.l_len    = 0L; /* lock to eof */

		/* no truncating before lock checking */
		pidf = fopen(pidfile, "a+");
		if (pidf != NULL) {
			lfd = fileno(pidf);
			for (tries = 0; tries < MAX_PIDFILE_LOCK_TRIES; tries++) {
				errno = 0;
				status = fcntl(lfd, F_SETLK, &fl);
				if (status == -1) {
					if (errno == EACCES || errno == EAGAIN) {
						sleep(1);
						continue;
					} else {
						snprintf(buf, sizeof(buf), "fcntl lock error "
								"status %d on %s %d %s", status,
								pidfile, lfd, sys_errlist[errno]);
						logerror(buf);
						die(0);
					}
				}
				/* successful lock */
				break;
			}

			if (errno != 0) {
				snprintf(buf, sizeof(buf), "Cannot lock %s fd %d "
						"in %d tries %s", pidfile, lfd,
						tries+1, sys_errlist[errno]);
				logerror(buf);

				/* who is hogging this lock */
				fl.l_type   = F_WRLCK;
				fl.l_whence = SEEK_SET; /* relative to bof */
				fl.l_start  = 0L; /* from offset zero */
				fl.l_len    = 0L; /* lock to eof */
#ifdef HAS_FLOCK_SYSID
				fl.l_sysid  = 0L;
#endif
				fl.l_pid    = 0;

				status = fcntl(lfd, F_GETLK, &fl);
				if ((status == -1) || (fl.l_type == F_UNLCK)) {
					snprintf(buf, sizeof(buf), "Cannot determine "
							"%s lockholder status=%d type=%d",
							pidfile, status, fl.l_type);
					logerror(buf);
					return(0);
				}

				snprintf(buf, sizeof(buf), "ERROR: another syslog"
						" daemon is running, with lock on %s being"
						"held by it sys=%u pid=%d", pidfile,
#ifdef HAS_FLOCK_SYSID
						fl.l_sysid,
#else
						-1,
#endif
						fl.l_pid);
				logerror(buf);
				die(0);
			}

			DaemonFlags |= SYSLOGD_LOCKED_PIDFILE;
			if (ftruncate(lfd,0) < 0) {
				snprintf(buf, sizeof(buf), "Error truncating pidfile, %s",
						strerror(errno));
				logerror(buf);
				die(0);
			}

			fprintf(pidf, "%d\n", getpid());
			(void) fflush(pidf);
		}
	}

	dprintf("off & running....\n");

	init(0);
	(void)signal(SIGHUP, init);

	nextcall.tv_sec = 0;
	nextcall.tv_usec = 0;
	for (;;) {
		fd_set readfds;
		int nfds = 0;
		struct i_module *im;
		struct im_msg log;

		FD_ZERO(&readfds);

		for (im = &Inputs; im ; im = im->im_next) {
			if (im->im_fd != -1) {
				FD_SET(im->im_fd, &readfds);
				if (im->im_fd > nfds)
					nfds = im->im_fd;
			}
		}

		/* get current time */
		gettimeofday(&tnow, NULL);
		if (finet > 0 && !(DaemonFlags & SYSLOGD_FINET_READ))
			FD_SET(finet, &readfds);

		if (nextcall.tv_sec == tnow.tv_sec &&
				nextcall.tv_usec > tnow.tv_usec) {
			timeout.tv_sec = 0L;
			timeout.tv_usec = nextcall.tv_usec - tnow.tv_usec;
			if (timeout.tv_usec < SYSLOG_TIMEOUT_MINUSEC)
				timeout.tv_usec = SYSLOG_TIMEOUT_MINUSEC;
		} else if (nextcall.tv_sec > tnow.tv_sec) {
			timeout.tv_sec = nextcall.tv_sec - tnow.tv_sec;
			if (timeout.tv_sec > SYSLOG_TIMEOUT_MAXSEC)
				timeout.tv_sec = SYSLOG_TIMEOUT_MAXSEC;
			if (nextcall.tv_usec > tnow.tv_usec) {
				timeout.tv_usec = nextcall.tv_usec - tnow.tv_usec;
			} else {
				timeout.tv_usec = 0L;
			}
		} else {
			timeout.tv_sec = SYSLOG_TIMEOUT_SEC;
			timeout.tv_usec = SYSLOG_TIMEOUT_USEC;
		}

		/* dprintf("readfds = %#x\n", readfds); */
		dprintf("Entering select and waiting %li secs and %li usecs\n",
				timeout.tv_sec, timeout.tv_usec);
		nfds = select(nfds+1, &readfds, (fd_set *)NULL,
			(fd_set *)NULL, &timeout);

		if (nfds < 0) {
			if (errno != EINTR)
				logerror("select");
			continue;
		}

		gettimeofday(&nextcall, NULL);
		nextcall.tv_sec += SYSLOG_TIMEOUT_MAXSEC;

		/* dprintf("got a message (%d, %#x)\n", nfds, readfds); */
		/* call requested im_timer()  and read inputs */
		for (im = &Inputs; im ; im = im->im_next) {

			/* get current time each pass */
			gettimeofday(&tnow, NULL);

			if (im->im_func->im_timer &&
					im->im_nextcall.tv_sec < tnow.tv_sec &&
					im->im_nextcall.tv_usec < tnow.tv_usec) {

				/* call this input module timer */
				if ((*im->im_func->im_timer)(im, &log) < 0) {
					dprintf("Error calling timer for [%s]\n", im->im_name);
				}

				if ((nextcall.tv_sec = im->im_nextcall.tv_sec) &&
						(nextcall.tv_usec > im->im_nextcall.tv_usec)) {
					nextcall.tv_usec = im->im_nextcall.tv_usec;
				} else if (nextcall.tv_sec > im->im_nextcall.tv_sec) {
					nextcall.tv_sec = im->im_nextcall.tv_sec;
					nextcall.tv_usec = im->im_nextcall.tv_usec;
				}
			}

			if (im->im_fd != -1 && FD_ISSET(im->im_fd, &readfds)) {
				int i = 0;

				memset(&log, 0,sizeof(struct im_msg));

				if ( !(im->im_func->im_getLog) || (i =
						(*im->im_func->im_getLog)(im, &log)) < 0) {
					dprintf("Syslogd: Error calling input module"
		       				" %s, for fd %d\n", im->im_name, im->im_fd);
				}

				if (i > -1) {
					printline(log.im_host, log.im_msg, im->im_flags);
				}
			}
			/* silently DROP what comes */
			if ((finet > 0) && !(DaemonFlags & SYSLOGD_FINET_READ) &&
					(FD_ISSET(finet, &readfds))) {

				struct sockaddr_in frominet;
				int len = sizeof(frominet);
				char line[MAXLINE];

				recvfrom(finet, line, MAXLINE, 0,
						(struct sockaddr *)&frominet, &len);
			}


		}
	}

}

void
usage(void) {

	(void)fprintf(stderr,
			"usage: syslogd [-d] [-u] [-f conffile] [-m markinterval] \\\n"
			" [-p logpath] [-a logpath] -i input1 [-i input2] [-i inputn] \\\n"
			" [-l libdir]");
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(char *hname, char *msg, int flags) {
	char *p, *q, line[MAXLINE + 1];
	unsigned char c;
	int pri;

	/* test for special codes */
	pri = DEFUPRI;
	p = msg;
	if (*p == '<') {
		pri = 0;
		while (isdigit((int)*++p))
			pri = 10 * pri + (*p - '0');
		if (*p == '>')
			++p;
	}
	if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
		pri = DEFUPRI;

#ifndef INSECURE_KERNEL_INPUT
	/* don't allow users to log kernel messages */
	if (LOG_FAC(pri) == LOG_KERN && !(flags & IMODULE_FLAG_KERN))
		pri = LOG_MAKEPRI(LOG_USER, LOG_PRI(pri));
#endif

	q = line;

	while ((c = *p++) && q < &line[sizeof(line) - 1]) {
		if (c == '\n')
			*q++ = ' ';
		else if (c < 040 && q < &line[sizeof(line) - 2]) {
			*q++ = '^';
			*q++ = c ^ 0100;
		} else if ((c == 0177 || (c & 0177) < 040) &&
		     q < &line[sizeof(line) - 4]) {
			*q++ = '\\';
			*q++ = '0' + ((c & 0300) >> 6);
			*q++ = '0' + ((c & 0070) >> 3);
			*q++ = '0' + (c & 0007);
		}
		else
			*q++ = c;
	}

	*q = '\0';

	logmsg(pri, line, hname, 0);
}

time_t	now;

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(int pri, char *msg, char *from, int flags) {
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
		if (!isalnum((int)msg[i]))
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

	/* log the message to the particular outputs */
	if (!Initialized) {

		if (!(DaemonFlags & SYSLOGD_CONSOLE_ACTIVE)) {
			if (omodule_create(ctty, &consfile, NULL) == -1) {
				dprintf("Error initializing console output!\n");

			} else
				DaemonFlags |= SYSLOGD_CONSOLE_ACTIVE;
		}

		if (DaemonFlags & SYSLOGD_CONSOLE_ACTIVE) {
			doLog(&consfile, flags, msg);
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
			f->f_lasttime[15] = '\0';
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
			f->f_lasttime[15] = '\0';
			(void)strncpy(f->f_prevhost, from,
					sizeof(f->f_prevhost)-1);
			f->f_prevhost[sizeof(f->f_prevhost)-1] = '\0';
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				(void)strncpy(f->f_prevline, msg, sizeof(f->f_prevline) - 1);
				f->f_prevline[sizeof(f->f_prevline) - 1] = '\0';
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
doLog(struct filed *f, int flags, char *message) {
	struct	o_module *om;
	char	repbuf[80], *msg;
	int	len, ret;

	if (message) {
		msg = message;
	        len = strlen(message);
	} else if (f->f_prevcount > 1) {
		msg = repbuf;
		len = snprintf(repbuf, 80, "last message repeated %d times",
				f->f_prevcount);
	} else {
		msg = f->f_prevline;
		len = f->f_prevlen;
	}

	f->f_time = now;

	for (om = f->f_omod; om; om = om->om_next) {
		if(!om->om_func || !om->om_func || !om->om_func->om_doLog) {
			dprintf("doLog: error, no doLog function in output "
				"module [%s], message [%s]\n", om->om_func->om_name, msg);
			continue;
		};

		/* call this module doLog */
		ret = (*(om->om_func->om_doLog))(f,flags,msg,om->ctx);
		if (ret < 0) {
			dprintf("doLog: error with output module [%s] "
				"for message [%s]\n", om->om_func->om_name, msg);
		} else if (ret == 0)
			/* stop going on */
			break;
	}
	f->f_prevcount = 0;
}


void
reapchild(int signo) {
	int status;
	int save_errno = errno;

	while (waitpid(-1, &status, WNOHANG) > 0)
		;
	errno = save_errno;
}

void
domark(int signo) {
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
logerror(char *type) {
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
die(int signo) {
	struct filed *f;
	int was_initialized = Initialized;
	char buf[100];
	struct i_module	*im;

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

	for (im = &Inputs; im; im = im->im_next)
		if (im->im_func && im->im_func->im_close)
			(*im->im_func->im_close)(im);
		else if (im->im_fd)
			close(im->im_fd);

	if (!Debug && (DaemonFlags & SYSLOGD_LOCKED_PIDFILE)) {
		struct flock fl;
		int lfd;

		lfd = fileno(pidf);
		fl.l_type   = F_UNLCK;
		fl.l_whence = SEEK_SET; /* relative to bof */
		fl.l_start  = 0L; /* from offset zero */
		fl.l_len    = 0L; /* lock to eof */

		fcntl(lfd, F_SETLK, &fl);

		if (pidf != NULL)
			(void) fclose(pidf);
		if (unlink(pidfile) < 0)
			logerror("error deleting pidfile");
	}

	exit(0);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
void
init(int signo) {
	int i;
	FILE *cf;
	struct filed *f, *next, **nextp;
	char *p;
	char cline[LINE_MAX];
 	char prog[NAME_MAX+1];
	struct o_module *om;

	dprintf("init\n");

	/*
	 *  Close all open log files.
	 */
	Initialized = 0;
	for (f = Files; f != NULL; f = next) {
		/* flush any pending output */
		if (f->f_prevcount)
			doLog(f, 0, (char *)NULL);

		for (om = f->f_omod; om; om = om->om_next) {
			/* flush any pending output */
			if (f->f_prevcount &&
			    om->om_func->om_flush != NULL) {
				(*om->om_func->om_flush) (f,om->ctx);
			}

			if (om->om_func->om_close != NULL) {
				(*om->om_func->om_close) (f,om->ctx);
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
	strncpy(prog, "*", 2);
	while (fgets(cline, sizeof(cline), cf) != NULL) {
		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. #!prog  and !prog are treated
		 * specially: the following lines apply only to that program.
		 */
		for (p = cline; isspace((int)*p); ++p)
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
			while (isspace((int)*p))
				p++;
			if (!*p) {
				strncpy(prog, "*", 2);
				continue;
			}
			for (i = 0; i < NAME_MAX; i++) {
				if (!isalnum((int)p[i]))
					break;
				prog[i] = p[i];
			}
			prog[i] = 0;
			continue;
		}
		p = cline + strlen(cline);
		while (p > cline)
			if (!isspace((int)*--p)) {
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
cfline(char *line, struct filed *f, char *prog) {
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
	if (!strcmp(prog, "*")) prog = NULL;
		else f->f_program = strdup(prog);

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
decode(const char *name, CODE *codetab) {
	CODE *c;
	char *p, buf[40];

	if (isdigit((int)*name))
		return (atoi(name));

	for (p = buf; *name && p < &buf[sizeof(buf) - 1]; p++, name++) {
		if (isupper((int)*name))
			*p = tolower((int)*name);
		else
			*p = *name;
	}
	*p = '\0';
	for (c = codetab; c->c_name; c++)
		if (!strcmp(buf, c->c_name))
			return (c->c_val);

	return (-1);
}

struct log_buf *
log_getnext(void) {

	if (log_buf_queue) {
		struct log_buf *ret, *last = log_buf_queue;

		for(ret = last; ret->l_next; last = ret, ret = ret->next);

		last->l_next = NULL;

		return(ret_l);

	} else {

		log_buf_avail = (struct logbuf *) calloc(1,sizeof(struct log_buf);

		if (!log_buf_avail) {
			dprintf("logGetNext: ERROR No memory available for a new log_buffer!\n");
			return(NULL);
		}

		return(log_buf_queue);

	}

}

int
log_deref(struct log_buf *log) {

	if (log == NULL) {
		dprintf("log_deref: WARNING we received a NULL pointer\n");
		return (-1);
	}

	if (log_queue_in > LOG_QUEUE_MAX) {
		free(log);
	} else {
		struct log_buf *l = log_queue;

		if (log_queue != NULL)
			for(; l->l_next; l = l->l_next);

		l->next = log;
		log->next = NULL; /* just in case */
	}

	return(1);
}

int
msgAdjust(struct log_buf *log) {

	/* set it first */
	time(&now);

    /* check if data has original date */
    if ( log->l_dlen < 16
			|| log->l_data[3] != ' '
			|| log->l_data[6] != ' '
			|| log->l_data[9] != ':'
			|| log->l_data[12] != ':'
			|| log->l_data[15] != ' ') {

		/* move original date to l_tm */
		if (!strptime(log->l_data, "%b %d %T", &log->l_tm) {
			dprintf("logAdjust: ERROR reading date and time!\n");
			return (-1);
		}

		/* move original data to start of l_data */
		bcopy(log->l_data + 16, log->l_data, log->l_dlen - 16);
		log->l_dlen -= 16;
        
#error finish this sucker

	} else {
		/* set l_tm to now */
		localtime_r(&now, &log->l_tm)
	}

	/* zero all unused data */
	memset(log->l_data + log->l_dlen, 0, sizeof(log->l_data) - log->l_dlen);
	memset(log->l_host + log->l_hlen, 0, sizeof(log->l_host) - log->l_hlen);

}

/*
 * Compares two log buffers
 * Returns 1 if equal, 0 if different
 *
 */

int
logCompare(struct log_buf *log1, struct log_buf *log2) {

	if (log1 == NULL || log2 == NULL)
		return(0);

	if (memcmp(log1 + LOG_BUF_IGNORE_HDR,
			log2 + LOG_BUF_IGNORE_HDR,
			sizeof(*log1) - LOG_BUF_IGNORE_HDR)
		return(1);
	else
		return(0);
	
}

