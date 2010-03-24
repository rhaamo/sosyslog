/*	$CoreSDI: syslogd.c,v 1.229 2002/03/01 08:30:47 alejo Exp $	*/

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
/*static char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";*/
static char rcsid[] = "$CoreSDI: syslogd.c,v 1.229 2002/03/01 08:30:47 alejo Exp $";
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
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#if HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif
#ifndef WEXITSTATUS
# define WEXITSTATUS(stat_val) ((unsigned)(stat_val) >> 8)
#endif
#ifndef WIFEXITED
# define WIFEXITED(stat_val) (((stat_val) & 255) == 0)
#endif
#include <sys/un.h>
#include <sys/utsname.h>

#ifdef TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/resource.h>
#ifdef HAVE_SYSCTL_H
# include <sys/sysctl.h>
#endif
#include <limits.h>
#if defined(SIGALTSTACK_WITH_STACK_T) && defined(HAVE_SYS_CONTEXT_H)
# include <sys/context.h>
#endif

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <dlfcn.h>
#include <stdarg.h>

#define SYSLOG_NAMES
#include <syslog.h>
#include "modules.h"
#include "syslogd.h"

#ifndef _PATH_CONSOLE
#define _PATH_CONSOLE	"/dev/console"
/* #warning Using "/dev/console" for _PATH_CONSOLE */
#endif /* _PATH_CONSOLE */

/* if _PATH_DEVNULL isn't defined, define it here... */
#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL	"/dev/null"
#endif

#ifndef NAME_MAX
#ifdef MAXNAMLEN
#define NAME_MAX MAXNAMLEN
/* #warning using MAXNAMLEN for NAME_MAX */
#else
#define NAME_MAX 255
/* #warning using 255 for NAME_MAX */
#endif /* MAXNAMLEN */
#endif /* NAME_MAX */

#ifndef HAVE_SOCKLEN_T
# define socklen_t int
#endif

/*
 * Intervals at which we flush out "message repeated" messages,
 * in seconds after previous message is logged.  After each flush,
 * we move to the next interval until we reach the largest.
 */
int repeatinterval[] = { 30, 120, 600 };	/* # of secs before flush */


struct filed *Files;
struct filed  consfile;

int	 Initialized = 0;	 /* set when we have initialized ourselves */
int	 MarkInterval = 20 * 60; /* interval between marks in seconds */
int	 MarkSeq = 0;		 /* mark sequence number */
int	 WantDie = 0;
char	*ConfFile = _PATH_SOSYSLOG_CONF; /* configuration file */

#ifdef _PATH_LOGPID
char	 *pidfile = _PATH_LOGPID;
#else
char	 *pidfile = PID_DIR "/" PID_FILE;
#endif

FILE	*pidf;

#define MAX_PIDFILE_LOCK_TRIES 5

char    *ctty = "%classic -t CONSOLE " _PATH_CONSOLE;	/* console path */
int	UseConsole = 1;
char     LocalHostName[MAXHOSTNAMELEN];  /* our hostname */
int      Debug = 0;				/* debug flag */
int	 DaemonFlags = 0;		/* running daemon flags */
#define SYSLOGD_LOCKED_PIDFILE  0x01    /* pidfile is locked */
#define SYSLOGD_MARK		0x02    /* call domark() */
#define SYSLOGD_DIE		0x04    /* call die() */
#define USE_LOCALDOMAIN		0x08    /* use hostname with local domain */
#define DONT_COLLAPSE		0x10

char	*libdir = NULL;

RETSIGTYPE domark(int);
RETSIGTYPE reapchild(int);
RETSIGTYPE init(int);
RETSIGTYPE signal_handler (int);
RETSIGTYPE dodie(int);
void	die(int);
int	cfline(char *, struct filed *, char *);
int	decode(const char *, CODE *);
void	markit(void);
void	doLog(struct filed *, int, char *, int, int);
void	printline(char *, char *, size_t, int);
void	usage(void);
int	imodule_create(struct i_module *, char *);
int	omodule_create(char *, struct filed *, char *);
int	omodules_destroy(struct omodule *);
int	imodules_destroy(struct imodule *);
void	logerror(char *);
void	logmsg(int, char *, char *, int);
int	getmsgbufsize(void);
void	*main_lib = NULL;

extern struct	omodule *omodules;
extern struct	imodule *imodules;
struct i_module	Inputs;

struct pollfd	 *fd_inputs = NULL;
int		  fd_in_count = 0;
struct i_module	**fd_inputs_mod = NULL;

int
main(int argc, char **argv)
{
	struct im_msg	   log;
#ifndef SIGALTSTACK_WITH_STACK_T
	struct sigaltstack alt_stack;
#else
	stack_t alt_stack;
#endif
	struct sigaction   sa;
	int		   ch;
	int		   argcnt;
	int		default_inputs = 1; /* start default modules? */
	int		(*resolv_domain)(char *, int, char *);

	Inputs.im_next = NULL;
	Inputs.im_fd = -1;

	/* init module list */
	imodules = NULL;
	omodules = NULL;

	setlinebuf(stdout);

#ifdef	NEEDS_DLOPEN_NULL
	if ( dlopen(NULL, RTLD_LAZY | RTLD_GLOBAL) == NULL)
		printf("syslogd: error exporting%s\n", dlerror());
#endif

	if ( (main_lib = dlopen(INSTALL_LIBDIR "/" MLIBNAME_STR, DLOPEN_FLAGS))
	    == NULL && Debug)
		main_lib = dlopen("./" MLIBNAME_STR, DLOPEN_FLAGS);

	if (main_lib == NULL) {
		printf("Error opening main library, [%s] file [%s]\n",
		    dlerror(), INSTALL_LIBDIR "/" MLIBNAME_STR);
		return(-1);
	}

	/*
	 * Daemon options
	 *
	 * -d -debug		<level>
	 * -i -input		[input module name and args]
	 * -f -conf		[configuration file]
	 * -P -pidfile		[pid file]
	 * -m -markinterval	[mark interval seconds]
	 * -c -noconsole
	 * -A -uselocaldomain
	 * -n -nodefault
	 * -C -nocollapse
	 *
	 * legacy options
	 * -u -unix
	 * -p -path -a 		[additional unix input module in /dev/log]
	 */

	argcnt = 1;	/* skip argv[0] getxopt() */

	while ((ch = getxopt(argc, argv, "d!debug: i!input: f!conf:"
	    " m!markinterval: P!pidfile: c!console A!localdomain"
	    " n!nodefault C!nocollapse h!help", &argcnt)) != -1) {
		char buf[512];

		switch (ch) {
		case 'd':	/* debug */
			if (argcnt >= argc || *argv[argcnt] == '-'
			    || !isdigit((int) *argv[argcnt])) {
				/* missing level */
				Debug = 20;
				argcnt--;	/* no arg specified */
			} else if (isdigit((int) *argv[argcnt])) {
				Debug = strtol(argv[argcnt], NULL, 10);
			}
			break;
		case 'f':	/* configuration file */
			ConfFile = argv[argcnt];
			break;
		case 'm':	/* mark interval */
			MarkInterval = strtol(argv[argcnt], NULL, 10) * 60;
			break;
		case 'u':	/* allow udp input port */
			if (imodule_create(&Inputs, "udp") < 0) {
				fprintf(stderr, "syslogd: WARNING error on "
				    "udp input module\n");
			}
			break;
		case 'i':	/* inputs */
			if (imodule_create(&Inputs, argv[argcnt]) < 0) {
				fprintf(stderr, "syslogd: WARNING error on "
				    "input module, ignoring %s\n", argv[argcnt]);
			}
			break;
		case 'p':	/* path */
		case 'a':	/* additional im_unix socket */
			snprintf(buf, sizeof(buf), "unix %s", argv[argcnt]);
			if (imodule_create(&Inputs, buf) < 0) {
				fprintf(stderr, "syslogd: WARNING out of "
				    "descriptors, %s ignored\n", argv[argcnt]);
			}
			break;
		case 'c':	/* don't use console */
			UseConsole = 0;
			break;
		case 'n':	/* don't start default modules */
			default_inputs = 0;
			break;
		case 'P':	/* alternate pidfile */
			pidfile = argv[argcnt];
			break;
		case 'A':	/* use local domain name too */
			DaemonFlags |= USE_LOCALDOMAIN;
			break;
		case 'C':
			DaemonFlags |= DONT_COLLAPSE;
			break;
		case 'h':
		default:
			usage();
		}
		argcnt++;
	}

	if ( default_inputs && Inputs.im_fd < 0 && Inputs.im_next == NULL ) {
#ifdef	HAVE_LINUX_IMODULE
		if (imodule_create(&Inputs, "linux") < 0) {
			fprintf(stderr, "syslogd: WARNING error on "
			    "linux input module\n");
		}
#elif	HAVE_BSD_IMODULE
		if (imodule_create(&Inputs, "bsd") < 0) {
			fprintf(stderr, "syslogd: WARNING error on "
			    "bsd input module\n");
		}
#elif	HAVE_STREAMS_IMODULE
		if (imodule_create(&Inputs, "streams") < 0) {
			fprintf(stderr, "syslogd: WARNING error on "
			    "streams input module\n");
		}
		if (imodule_create(&Inputs, "doors") < 0) {
			fprintf(stderr, "syslogd: WARNING error on "
			    "doors input module\n");
		}
#endif

#ifndef	HAVE_STREAMS_IMODULE
# ifdef	HAVE_UNIX_IMODULE
		if (imodule_create(&Inputs, "unix") < 0) {
			fprintf(stderr, "syslogd: WARNING error on "
			    "unix input module\n");
		}
#endif /* ifdef HAVE_UNIX_IMODULE */
#endif /* ifndef HAVE_STREAMS_IMODULE */
	}

	if ( Inputs.im_fd < 0 && Inputs.im_next == NULL ) {
		m_dprintf(MSYSLOG_SERIOUS, "syslogd: no inputs active\n");
		usage();
	}

	if (argc != argcnt)
		m_dprintf(MSYSLOG_SERIOUS, "syslogd: remaining command"
		    " line not parsed!\n");

	if (!Debug) {
		struct rlimit r;

		/* no core dumping */
		r.rlim_cur = 0;
		r.rlim_max = 0;
		if (setrlimit(RLIMIT_CORE, &r)) {
			logerror("ERROR setting limits for coredump");
		}

	}

	if (!Debug) {
		int fd;

		/* go daemon and mimic daemon() */
		switch (fork()) {
			case -1:
				perror("fork");
				exit(-1);
				break;
			case 0:
				break;
			default:
				exit(0);
		}

		/* child */
		if (setsid() == -1)
			return (-1);

		chdir("/");
		if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
			dup2(fd, STDIN_FILENO);
			dup2(fd, STDOUT_FILENO);
			dup2(fd, STDERR_FILENO);
			if (fd > 2)
				CLOSE_FD(fd);
		}
	}

	gethostname(LocalHostName, sizeof(LocalHostName));
	if ((DaemonFlags & USE_LOCALDOMAIN) &&
	    (resolv_domain = dlsym(main_lib, SYMBOL_PREFIX "resolv_domain"))
	    != NULL && resolv_domain(LocalHostName, sizeof(LocalHostName) - 1,
	    LocalHostName) == -1)
		gethostname(LocalHostName, sizeof(LocalHostName)); /* again */

	/* Set signal handlers */
	/* XXX: use one signal handler for all signals other than HUP */
	/*      use sigaction and sigaltstack */
	place_signal(SIGTERM, dodie);
	place_signal(SIGINT, Debug ? dodie : SIG_IGN);
	place_signal(SIGQUIT, Debug ? dodie : SIG_IGN);
	place_signal(SIGCHLD, reapchild);
	place_signal(SIGALRM, domark);
	place_signal(SIGPIPE, SIG_IGN);

	if ( (alt_stack.ss_sp = malloc(SIGSTKSZ)) == NULL) {
		m_dprintf(MSYSLOG_CRITICAL, "malloc altstack struct");
		exit(-1);
	}
#if 0 /* should we do this on some OSs (ie. Aix)? */
	/* adjust ss_sp to point to base of stack */
	sigstk.ss_sp += SIGSTKSZ - 1;
#endif
	alt_stack.ss_size = SIGSTKSZ;
	alt_stack.ss_flags = 0;
	if (alt_stack.ss_sp == NULL) {
		perror(strerror(errno));
		exit(-1);
	}
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGTSTP);
	sigaddset(&sa.sa_mask, SIGALRM);
	sigaddset(&sa.sa_mask, SIGHUP);
	sa.sa_handler = signal_handler;
	sa.sa_flags = SA_NOCLDSTOP | SA_RESTART | SA_ONSTACK;
	if (sigaltstack(&alt_stack, NULL) < 0 ||
	    sigaction(SIGTSTP, &sa, NULL) < 0 ) {
		FREE_PTR(alt_stack.ss_sp);
		perror(strerror(errno));
		exit(-1);
	}
	alarm(TIMERINTVL);

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
			for (tries = 0; tries < MAX_PIDFILE_LOCK_TRIES;
			    tries++) {
				errno = 0;
				status = fcntl(lfd, F_SETLK, &fl);
				if (status == -1) {
					if (errno == EACCES ||
					    errno == EAGAIN) {
						sleep(1);
						continue;
					} else {
						snprintf(buf, sizeof(buf),
						    "fcntl lock error status %d"
						    " on %s %d %s", status,
						    pidfile, lfd,
						    strerror(errno));
						logerror(buf);
						die(0);
					}
				}
				/* successful lock */
				break;
			}

			if (status == -1) {
				snprintf(buf, sizeof(buf), "Cannot lock %s fd "
				    "%d in %d tries %s", pidfile, lfd,
				    tries + 1, strerror(errno));
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
					snprintf(buf, sizeof(buf), "Cannot "
					    "determine %s lockholder status = "
					    "%d type=%d", pidfile, status,
					    fl.l_type);
					logerror(buf);
					return (0);
				}

				snprintf(buf, sizeof(buf), "Lock on %s is "
				    "being held by sys = %u pid = %d", pidfile,
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
			if ( ftruncate( lfd, (off_t) 0) < 0) {
				snprintf(buf, sizeof(buf), "Error truncating pidfile, %s",
						strerror(errno));
				logerror(buf);
				die(0);
			}

			fprintf(pidf, "%d\n", (int) getpid());
			(void) fflush(pidf);
		}
	}

	m_dprintf(MSYSLOG_INFORMATIVE, "off & running....\n");

	init(0);
	place_signal(SIGHUP, init);

	for (;;) {
		int count, i, done;

		if (DaemonFlags & SYSLOGD_MARK)
			markit();
		if (WantDie)
			die(WantDie);

		if (fd_inputs == NULL) {
			m_dprintf(MSYSLOG_CRITICAL, "no input struct");
			exit(-1);
		}

		/* count will always be less than fd_in_count */
		switch (count = poll(fd_inputs, fd_in_count, -1)) {
		case 0:
			m_dprintf(MSYSLOG_INFORMATIVE, "main: poll returned 0\n");
			continue;
		case -1:
			m_dprintf(MSYSLOG_INFORMATIVE, "main: poll returned "
			    "-1\n");
			if (errno != EINTR)
				logerror("poll");
			continue;
		}

		for (i = 0, done = 0; done < count; i++) {
			if (fd_inputs[i].revents & POLLIN) {
				char	*mname;
				int	fd;
				int	val = -1;

				log.im_pid = 0;
				log.im_pri = 0;
				log.im_flags = 0;

				mname = fd_inputs_mod[i]->im_func->im_name;
				fd = fd_inputs[i].fd;

				if (!fd_inputs_mod[i] ||
				    !fd_inputs_mod[i]->im_func ||
				    !fd_inputs_mod[i]->im_func->im_read ||
				    (val = (*fd_inputs_mod[i]->im_func->im_read)
				    (fd_inputs_mod[i], fd_inputs[i].fd,
				    &log)) < 0) {
					m_dprintf(MSYSLOG_SERIOUS, "syslogd: "
					    "Error calling input module %s, "
					    "for fd %i\n", mname, fd);

				} else if (val == 1)    /* log it */
					printline(log.im_host, log.im_msg,
					    log.im_len,
					    fd_inputs_mod[i]->im_flags);
				/* return of 0 skips it */

				done++; /* one less */
			}

		}
	}

	/* NOT REACHED */
	return(1);

}

void
usage(void)
{
	fprintf(stderr,
	    "Saucisse log version " SOSYSLOG_VERSION_STR "\n\n"
	    "usage: syslogd [-d <debug_level>] [-u] [-f conffile] "
	    "[-P pidfile] [-n] [-m markinterval] \\\n [-p logpath] "
	    "[-a logpath] [-C] -i input1 [-i input2] [-i inputn]\n %s\n"
	    "%s\n\n", copyright, rcsid);
	exit(1);
}

/*
 * Take a raw input line, decode the message, and print the message
 * on the appropriate log files.
 */
void
printline(char *hname, char *msg, size_t len, int flags)
{
	register char *p, *q;
	register unsigned char c;
	char line[MAXLINE + 2];
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

	while ( (c = *p++) && q < &line[sizeof(line) - 1]) {
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
		} else
			*q++ = c;
	}

	*q = '\0';

	logmsg(pri, line, hname, 0);
}

/*
 * Log a message to the appropriate log files, users, etc. based on
 * the priority.
 */
void
logmsg(int pri, char *msg, char *from, int flags)
{
	register struct filed *f;
	int fac, msglen, prilev, i;
	sigset_t mask, omask;
 	char prog[NAME_MAX+1];
	time_t now;
	struct tm timestamp;

	if (from == NULL || *from == '\0')
		from = LocalHostName;

	m_dprintf(MSYSLOG_INFORMATIVE2, "logmsg: pri 0%o, flags 0x%x, from %s,"
	    " msg %s\n", pri, flags, from, msg);

	sigemptyset(&mask);
	sigaddset(&mask, SIGALRM);
	sigaddset(&mask, SIGHUP);
	sigaddset(&mask, SIGTSTP);
	sigprocmask(SIG_BLOCK, &mask, &omask);

	/*
	 * Process date and time as needed
	 *
	 * ctime   gives    "Thu Nov 24 18:22:48 1986\n"
	 * msg may give         "Nov 24 18:22:48"
	 *                   0123456789012345678901234
	 */

	msglen = strlen(msg);
	if (!(flags & ADDDATE) && (msglen < 16 || msg[3] != ' ' ||
	    msg[6] != ' ' || msg[9] != ':' || msg[12] != ':' ||
	    msg[15] != ' '))
		flags |= ADDDATE;

	time(&now);
	localtime_r(&now, &timestamp);

	if (!(flags & ADDDATE)) {
		char	*p;
		/* First, attempt to parse the date as if it contained
		 * the year, which isn't part of the standard syslog
		 * date format. */

		p = strptime(msg, "%b %d %H:%M:%S %Y", &timestamp);
		if (!p) {
			time_t	tstamp;
			/* The year of the message is unknown at this point.
			 * If the resulting date is in the future, assume
			 * it actually is from last year. */

			p = strptime(msg, "%b %d %H:%M:%S", &timestamp);
			tstamp = mktime(&timestamp);

		}

		if (p) { /* If this fails we're probably in trouble. */
		    	p++; /* Point to the beginning of the message. */

			msglen -= (unsigned int) (p - msg);
			msg = p;
		}

	}

	/* extract facility and priority level */
	if (flags & MARK)
		fac = LOG_NFACILITIES;
	else
		fac = LOG_FAC(pri);
	prilev = LOG_PRI(pri);

	/* extract program name */
	for (i = 0; i < NAME_MAX; i++) {
		if (!isalnum((int)msg[i]))
			break;
		prog[i] = msg[i];
	}
	prog[i] = 0;

	/* log the message to the particular outputs */
	if (!Initialized) {
		if (UseConsole && omodule_create(ctty, &consfile,
		    NULL) != -1) {
			doLog(&consfile, flags, msg, prilev, fac);
			if (consfile.f_omod && consfile.f_omod->om_func
			    && consfile.f_omod->om_func->om_close != NULL)
				(*consfile.f_omod->om_func->om_close)
				    (&consfile, consfile.f_omod->ctx);
			if (consfile.f_omod) {
				FREE_PTR(consfile.f_omod->ctx);
				FREE_PTR(consfile.f_omod->status);
				FREE_PTR(consfile.f_omod);
				consfile.f_omod = NULL;
			}
		}
		sigprocmask(SIG_SETMASK, &omask, NULL);
		return;
	}

	for (f = Files; f; f = f->f_next) {
		/* skip messages that are incorrect priority */
		/* XXX */
		if (f->f_pmask[fac] == TABLE_NOPRI ||
		    (f->f_pmask[fac] & (1<<prilev)) == 0 ||
		    f->f_pmask[fac] == INTERNAL_NOPRI )
			continue;

		if (f->f_program)
			if (strcmp(prog, f->f_program) != 0)
				continue;

		if (UseConsole && (flags & IGN_CONS))
			continue;

		/* don't output marks to recently written files */
		time(&now);
		if ((flags & MARK) && (now - f->f_time) < MarkInterval / 2)
			continue;

		/*
		 * suppress duplicate lines to this file
		 */
		if (!(DaemonFlags & DONT_COLLAPSE) &&
		    !(flags & MARK) && msglen == f->f_prevlen &&
		    !strcmp(msg, f->f_prevline) &&
		    !strcmp(from, f->f_prevhost)) {

			memcpy(&f->f_tm, &timestamp, sizeof(f->f_tm));
			f->f_prevcount++;
			m_dprintf(MSYSLOG_INFORMATIVE, "msg repeated %d times,"
			    " %ld sec of %d\n", f->f_prevcount,
			    (long)(now - f->f_time),
			    repeatinterval[f->f_repeatcount]);
			/*
			 * If domark would have logged this by now,
			 * flush it now (so we don't hold isolated messages),
			 * but back off so we'll flush less often
			 * in the future.
			 */
			if (now > REPEATTIME(f)) {
				doLog(f, flags, NULL, prilev, fac);
				BACKOFF(f);
			}
		} else {
			/* new line, save it */

			/* flush previous line */
			if (f->f_prevcount)
				doLog(f, 0, NULL, prilev, fac);

			/*
			 * Start counting again, save host data etc.
			 */
			f->f_prevcount = 0;
			f->f_repeatcount = 0;
			f->f_prevpri = pri;
			memcpy(&f->f_tm, &timestamp, sizeof(f->f_tm));
			strncpy(f->f_prevhost, from,
			    sizeof(f->f_prevhost) - 1);
			f->f_prevhost[sizeof(f->f_prevhost) - 1] = '\0';
			if (msglen < MAXSVLINE) {
				f->f_prevlen = msglen;
				strncpy(f->f_prevline, msg,
				    sizeof(f->f_prevline) - 1);
				f->f_prevline[sizeof(f->f_prevline) - 1] = '\0';
				doLog(f, flags, NULL, prilev, fac);
			} else {
				f->f_prevlen = 0;
				f->f_prevline[0] = 0;
				doLog(f, flags, msg, prilev, fac);
			}
		}
	}
	(void)sigprocmask(SIG_SETMASK, &omask, NULL);
}

void
doLog(struct filed *f, int flags, char *message, int prilev, int fac)
{
	struct o_module *om;
	char	repbuf[80];
	struct m_msg m;
	int	ret;

	m.pri = prilev;
	m.fac = fac;
	if (message) {
		m.msg = message;
	} else if (f->f_prevcount > 1) {
		m.msg = repbuf;
		snprintf(repbuf, sizeof(repbuf), "last message repeated %d"
		    " times", f->f_prevcount);
	} else {
		m.msg = f->f_prevline;
	}

	time(&f->f_time);
	for (om = f->f_omod; om; om = om->om_next) {
		if (!om->om_func || !om->om_func->om_write) {
			m_dprintf(MSYSLOG_SERIOUS, "doLog: error, no write "
			    "function in output module [%s], message [%s]\n",
			    om->om_func->om_name, m.msg);
			continue;
		}

		/* call this module write */
		ret = (*(om->om_func->om_write))(f, flags, &m, om->ctx);
		if (ret < 0) {
			m_dprintf(MSYSLOG_SERIOUS, "doLog: error with output "
			    "module [%s] for message [%s]\n",
			    om->om_func->om_name, m.msg);
		} else if (ret == 0)
			/* stop going on */
			break;
	}
}


RETSIGTYPE
reapchild(int signo)
{
	int status;
	int save_errno = errno;

	while (waitpid(-1, &status, WNOHANG) > 0)
		;
	errno = save_errno;
}

RETSIGTYPE
domark(int signo)
{
	DaemonFlags |= SYSLOGD_MARK;
}

RETSIGTYPE
dodie(int signo)
{
	WantDie = 1;
}

void
markit(void)
{
	struct filed *f;
	time_t now;

	now = time((time_t *) NULL);

	MarkSeq += TIMERINTVL;

	if (MarkSeq >= MarkInterval || DaemonFlags & SYSLOGD_MARK) {
		logmsg(LOG_INFO, "-- MARK --", LocalHostName, ADDDATE|MARK);
		MarkSeq = 0;
	}

	for (f = Files; f; f = f->f_next) {
		if (f->f_prevcount && now >= REPEATTIME(f)) {
			/* we should report this based on module */
			m_dprintf(MSYSLOG_INFORMATIVE, "flush: repeated %d "
			    "times, %d sec.\n", f->f_prevcount,
			    repeatinterval[f->f_repeatcount]);
			doLog(f, 0, NULL, 0, 0);
			BACKOFF(f);
		}
	}

	DaemonFlags &= ~SYSLOGD_MARK;

	place_signal(SIGALRM, domark);

	alarm(TIMERINTVL);
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
	m_dprintf(MSYSLOG_INFORMATIVE, "%s\n", buf);
	logmsg(LOG_SYSLOG|LOG_ERR, buf, LocalHostName, ADDDATE);
}

void
die(int signo) {
	struct filed *f;
	int was_initialized = Initialized;
	char buf[100];
	struct i_module	*im;

	Initialized = 0;		/* Don't log SIGCHLDs */

	alarm(0);

	for (f = Files; f != NULL; f = f->f_next) {
		/* flush any pending output */
		if (f->f_prevcount)
			doLog(f, 0, NULL, 0, 0);
	}

	Initialized = was_initialized;

	if (signo) {
		m_dprintf(MSYSLOG_SERIOUS, "syslogd: exiting on signal %d\n",
		    signo);
		(void)sprintf(buf, "exiting on signal %d", signo);
		errno = 0;
		logerror(buf);
	}

	for (im = &Inputs; im; im = im->im_next)
		if (im->im_func && im->im_func->im_close)
			(*im->im_func->im_close)(im);
		else
			CLOSE_FD(im->im_fd);

	if (!Debug && (DaemonFlags == SYSLOGD_LOCKED_PIDFILE)) {
		struct flock fl;
		int lfd;

		lfd = fileno(pidf);
		fl.l_type   = F_UNLCK;
		fl.l_whence = SEEK_SET; /* relative to bof */
		fl.l_start  = 0L; /* from offset zero */
		fl.l_len    = 0L; /* lock to eof */

		fcntl(lfd, F_SETLK, &fl);

		(void) fclose(pidf);
		if (unlink(pidfile) < 0)
			logerror("error deleting pidfile");
	}

	exit(0);
}

/*
 *  INIT -- Initialize syslogd from configuration table
 */
RETSIGTYPE
init(int signo)
{
	int i;
	FILE *cf;
	struct filed *f, *next, **nextp;
	char *p;
	char cline[LINE_MAX];
 	char prog[NAME_MAX+1];
	struct o_module *om, *om_next;

	m_dprintf(MSYSLOG_INFORMATIVE, "init\n");

	/*
	 *  Close all open log files.
	 */

	Initialized = 0;

	alarm(0);

	for (f = Files; f != NULL; f = next) {

		/* flush any pending output */
		if (f->f_prevcount)
			doLog(f, 0, NULL, 0, 0);

		for (om = f->f_omod; om; om = om_next) {
			/* flush any pending output */
			if (f->f_prevcount && om->om_func &&
			    om->om_func->om_flush != NULL) {
				(*om->om_func->om_flush) (f,om->ctx);
			}

			if (om->om_func && om->om_func->om_close != NULL) {
				(*om->om_func->om_close) (f,om->ctx);
			}

			/* free om_ctx om_func and stuff */
			om_next = om->om_next;

			FREE_PTR(om->ctx);
			FREE_PTR(om->status);
			FREE_PTR(om);
		}

		next = f->f_next;

		FREE_PTR(f->f_program);

		FREE_PTR(f);
	}

#ifdef REOPEN_MAIN_LIBRARY_ON_HUP
	if (main_lib) {
		dlclose(main_lib);
		main_lib = NULL;
	}

	/* Load main modules library */
	if ( (main_lib = dlopen(INSTALL_LIBDIR "/" MLIBNAME_STR, DLOPEN_FLAGS))
	    == NULL && (Debug && (main_lib = dlopen("./" MLIBNAME_STR,
	    DLOPEN_FLAGS)) == NULL) ) {
	        m_dprintf(MSYSLOG_CRITICAL, "init: Error opening main library, [%s] "
	            "file [%s]\n", dlerror(), INSTALL_LIBDIR "/" MLIBNAME_STR);
	        exit(-1);
	}
#endif

	/* list of filed is now empty */
	Files = NULL;
	nextp = &Files;

	/* free all modules and their dynamic libs */
	if (signo == SIGHUP) {
		if (omodules_destroy(omodules) == 0)
			omodules = NULL;
#if DESTROY_INPUTS_ON_HUP
		if (imodules_destroy(imodules) == 0)
			imodules = NULL;
#endif
	}

	/* open the configuration file */
	if ((cf = fopen(ConfFile, "r")) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "cannot open %s\n", ConfFile);
		if ( (*nextp = (struct filed *) calloc(1, sizeof(*f)))
		    == NULL) {
			m_dprintf(MSYSLOG_CRITICAL, "calloc struct filed");
			exit(-1);
		}
		if (cfline("*.ERR\t/dev/console", *nextp, "*") == -1) {
			m_dprintf(MSYSLOG_CRITICAL, "can't write to console");
			exit(-1);
		}
		if ( ((*nextp)->f_next = (struct filed *) calloc(1, sizeof(*f)))
		    == NULL) {
			m_dprintf(MSYSLOG_CRITICAL, "calloc struct filed");
			exit(-1);
		}
		if (cfline("*.PANIC\t*", (*nextp)->f_next, "*") == -1) {
			m_dprintf(MSYSLOG_CRITICAL, "can't write to console");
			exit(-1);
		}
		Initialized = 1;
		return;
	}

	/*
	 *  Foreach line in the conf table, open that file.
	 */
	f = NULL;
	strncpy(prog, "*", sizeof (prog) - 1);
	prog[sizeof (prog) - 1] = '\0';

	while (fgets(cline, sizeof(cline), cf) != NULL) {
		int	clen;

		/*
		 * check for end-of-section, comments, strip off trailing
		 * spaces and newline character. #!prog  and !prog are treated
		 * specially: the following lines apply only to that program.
		 */
		for (p = cline; isspace((int)*p); ++p)
			continue;
		if (*p == '\0')
			continue;
		/* line is splitted, merge with the next */
		clen = strlen(cline);
		if (cline[clen - 1] == '\n' && cline[clen - 2] == '\\') {
			if (fgets(&cline[clen - 2], sizeof(cline) - clen, cf)
			    == NULL) {
				cline[clen - 2] = '\0';
				m_dprintf(MSYSLOG_INFORMATIVE, "syslogd: error "
				    "merging line [%s]\n", cline);
				break;
			}
		}
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
				strncpy(prog, "*", sizeof (prog) - 1);
				prog[sizeof (prog) - 1] = '\0';
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
		if ( (f = (struct filed *)calloc(1, sizeof(*f))) == NULL) {
			m_dprintf(MSYSLOG_CRITICAL, "calloc struct filed");
			exit(-1);
		}
		if (cfline(cline, f, prog) == 1) {
			*nextp = f;
			nextp = &f->f_next;
		} else {
			FREE_PTR(f);
		}
	}

	/* close the configuration file */
	fclose(cf);

	if (Files == NULL) {
		m_dprintf(MSYSLOG_CRITICAL, "syslogd: WARNING NO OUTPUT MODULES"
		    " ACTIVE, GIVING UP!\n");
		exit(-1);
	}

	Initialized = 1;

	if (Debug >= MSYSLOG_SERIOUS) {
		for (f = Files; f; f = f->f_next) {
			for (i = 0; i <= LOG_NFACILITIES; i++)
				if (f->f_pmask[i] == INTERNAL_NOPRI)
					printf("X ");
				else
					printf("%d ", f->f_pmask[i]);
			printf("\n");
			for (om = f->f_omod; om; om = om->om_next) {
				if (om->status) {
					printf("%s\n", om->status);
				} else {
					if (om->om_func &&
					    om->om_func->om_name)
						printf("** No status info for "
						    "module %s! **\n",
						    om->om_func->om_name);
				}
			}
		}
	}

	logmsg(LOG_SYSLOG|LOG_INFO, "syslogd: restart", LocalHostName, ADDDATE);
	m_dprintf(MSYSLOG_INFORMATIVE, "syslogd: restarted\n");
}

/*
 * Crack a configuration file line
 */
int
cfline(char *line, struct filed *f, char *prog) {
	register int i, j;
	int pri, singlpri, ignorepri;
	register char *p, *q;
	char *bp;
	char buf[MAXLINE], ebuf[240];

	m_dprintf(MSYSLOG_INFORMATIVE, "cfline(\"%s\", f, \"%s\")\n", line,
	    prog);

	errno = 0;	/* keep strerror() stuff out of logerror messages */
	ignorepri = 0;
	singlpri = 0;

	/* clear out file entry */
	memset(f->f_pmask, TABLE_NOPRI, sizeof(f->f_pmask));

	/* save program name if any */
	if (!strcmp(prog, "*"))
		prog = NULL;
	else
		f->f_program = strdup(prog);

	/* scan through the list of selectors */
	for (p = line; *p && *p != '\t' && *p != ' ';) {

		/* find the end of this facility name list */
		for (q = p; *q && *q != '\t' && *q++ != '.'; )
			continue;

		if (*p == '/' || *p == '%' || *p == '|' || *p == '-')
			break;

		pri = -1;

		/* collect priority name */
		for (bp = buf; *q && !strchr("\t, ;", *q); )
			*bp++ = *q++;
		*bp = '\0';

		/* skip cruft */
		while (strchr(", ;", *q))
			q++;

		if (*buf == '!') {
			ignorepri++;
			for (bp = buf; *(bp + 1); bp++)
				*bp = *(bp + 1); /* move back one */
			*bp = '\0';
		} else
			ignorepri = 0;

		if (*buf == '=') {
			singlpri++;
			pri = decode(&buf[1], prioritynames);
			for (bp = buf; *(bp + 1); bp++)
				*bp = *(bp + 1); /* move back one */
			*bp = '\0';
		} else {
			singlpri = 0;
			pri = decode(buf, prioritynames);
		}

		if (pri < 0) {
			snprintf(ebuf, sizeof ebuf, "unknown priority"
			    " name \"%s\" on line [%s]", buf, line);
			logerror(ebuf);
			return (-1);
		}

		/*
		 * Heavily modified to fit sysklogd
		 * This should be done with lex/yacc
		 */
	        /* scan facilities */
		while (*p && !strchr("\t .;", *p)) {
			for (bp = buf; *p && !strchr("\t ,;.", *p); )
			    *bp++ = *p++;

			*bp = '\0';

			if (*buf == '*') {
				for (i = 0; i <= LOG_NFACILITIES; i++) {
					if (pri == INTERNAL_NOPRI) {
						if (ignorepri)
							f->f_pmask[i] =
							    TABLE_ALLPRI;
						else
							f->f_pmask[i] =
							    TABLE_NOPRI;
					} else if (singlpri) {
						if (ignorepri)
							f->f_pmask[i] &=
							    ~(1<<pri);
						else
							f->f_pmask[i] |=
							    (1<<pri);
					} else {
						if (pri == TABLE_ALLPRI) {
							if (ignorepri)
								f->f_pmask[i] =
								    TABLE_NOPRI;
							else
								f->f_pmask[i] =
								    TABLE_ALLPRI;
						} else {
							if (ignorepri)
								for (j = 0; j <= pri; ++j)
									f->f_pmask[i] &= ~(1<<j);
							else
								for (j= 0; j <= pri; ++j)
									f->f_pmask[i] |= (1<<j);
						}
					}
				}
			} else {
				i = decode(buf, facilitynames);

				if (i < 0) {
					snprintf(ebuf, sizeof(ebuf), "unknown"
					    " facility name \"%s\"", buf);
					logerror(ebuf);
					return (-1);
				}

				if (pri == INTERNAL_NOPRI) {
					if (ignorepri)
						f->f_pmask[i >> 3] =
						    TABLE_ALLPRI;
					else
						f->f_pmask[i >> 3] =
						    TABLE_NOPRI;
				} else if (singlpri) {
					if (ignorepri)
						f->f_pmask[i >> 3] &=
						    ~(1<<pri);
					else
						f->f_pmask[i >> 3] |=
						    (1<<pri);
				} else {
					if (pri == TABLE_ALLPRI) {
						if (ignorepri)
							f->f_pmask[i >> 3] =
							    TABLE_NOPRI;
						else
							f->f_pmask[i >> 3] =
							    TABLE_ALLPRI;
					} else {
						if (ignorepri)
							for (j = 0; j <= pri;
							    ++j)
								f->f_pmask[i>>3]
								    &= ~(1<<j);
						else
							for (j= 0; j <= pri;
							    ++j)
								f->f_pmask[i>>3]
								    |= (1<<j);
					}
				}
			}

			while (*p == ',' || *p == ' ')
				p++;
		}

		p = q;
	}

	/* skip to action part */
	while (*p == '\t' || *p == ' ')
		p++;

	if (omodule_create(p, f, NULL) == -1) {
		m_dprintf(MSYSLOG_SERIOUS, "cfline: error initializing modules!\n");
		return (-1);
	}

	m_dprintf(MSYSLOG_INFORMATIVE, "cfline: all ok\n");

	return (1);
}

/*
 * Retrieve the size of the kernel message buffer, via sysctl.
 */
int
getmsgbufsize()
{
#ifdef HAVE_OPENBSD
	int msgbufsize, mib[2];
	size_t size;


	mib[0] = CTL_KERN;
	mib[1] = KERN_MSGBUFSIZE;
	size = sizeof msgbufsize;
	if (sysctl(mib, 2, &msgbufsize, &size, NULL, 0) == -1) {
		m_dprintf(MSYSLOG_INFORMATIVE, "couldn't get kern.msgbufsize\n");
		return (0);
	}

	return (msgbufsize);
#else
	return (MAXLINE);
#endif
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

	if (*name == '*')
		return (TABLE_ALLPRI);

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

/*
 *  decode_name a numeric value to a symbolic name
 */
char *
decode_val(int val, int codetab) {
	CODE *c;

	if (codetab == CODE_FACILITY)
		c = facilitynames;
	else if (codetab == CODE_PRIORITY)
		c = prioritynames;
	else
		return (NULL);

	for (; c->c_name; c++)
		if (val == c->c_val)
			return (c->c_name);

	return (NULL);
}

/*
 * add this fd to array
 *
 * grow by 50
 *
 * params: fd    file descriptor to watch
 *         im    module functions and more
 *
 */

int
add_fd_input(int fd, struct i_module *im)
{

	if (fd < 0 || im == NULL) {
		m_dprintf(MSYSLOG_INFORMATIVE, "add_fd_input: error on params"
		    " %d%s\n", fd, im ? "" : " null im");
		return (-1);
	}

	m_dprintf(MSYSLOG_INFORMATIVE, "add_fd_input: adding fd %d "
	    "for module %s\n", fd, im->im_func->im_name ?
	    im->im_func->im_name : "unknown");

	/* do we need bigger arrays? */
	if (!fd_inputs || fd_in_count % 50 == 0) {

		if ( (fd_inputs = (struct pollfd *) realloc(fd_inputs,
		    (size_t) (fd_in_count + 50) * sizeof(struct pollfd)))
		    == NULL) {
			m_dprintf(MSYSLOG_CRITICAL, "realloc inputs");
			exit(-1);
		}

		if ( (fd_inputs_mod = (struct i_module **)
		    realloc(fd_inputs_mod, (size_t) (fd_in_count + 50)
		    * sizeof(struct i_module *)))
		    == NULL) {
			m_dprintf(MSYSLOG_CRITICAL, "realloc inputs");
			exit(-1);
		}

	}

	fd_inputs[fd_in_count].fd = fd;
	fd_inputs[fd_in_count].events = POLLIN;
	fd_inputs_mod[fd_in_count] = im;
	fd_in_count++;

	return(1);
}

void
remove_fd_input(int fd)
{
	int i;

	m_dprintf(MSYSLOG_INFORMATIVE, "remove_fd_input: remove fd %d\n",
	    fd);

	for (i = 0; i < fd_in_count && fd_inputs[i].fd != fd; i++);

	if (i == fd_in_count || fd != fd_inputs[i].fd)
		return; /* not found */

	for (;i < fd_in_count; i++) {
		fd_inputs[i].fd = fd_inputs[i + 1].fd;
		fd_inputs[i].events = fd_inputs[i + 1].events;
		fd_inputs_mod[i] = fd_inputs_mod[i + 1];
	}

	fd_in_count--;
}


RETSIGTYPE
signal_handler(int signo)
{
	switch (signo) {
	case SIGTSTP:
		raise(SIGSTOP);
		break;
	default:;
	}
}


RETSIGTYPE (*
place_signal(int signo, RETSIGTYPE (*func)(int))) (int)
{
	struct sigaction act, oldact;

	act.sa_handler = func;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;
	if (signo == SIGALRM) {
#ifdef SA_INTERRUPT
		act.sa_flags |= SA_INTERRUPT;
#endif
	} else {
#ifdef SA_RESTART
	act.sa_flags |= SA_RESTART;
#endif
	}
	if (sigaction(signo, &act, &oldact) < 0)
		return(SIG_ERR);

	return(oldact.sa_handler);
}

/*
 * Report errors on debug active
 */

int
m_dprintf(const int level, char const *fmt, ...)
{
	int ret;
	va_list var;

	if (level >= Debug)
		return (1);

	va_start(var, fmt);
	ret = vfprintf(stderr, fmt, var);
	va_end(var);
	return(ret);
}
