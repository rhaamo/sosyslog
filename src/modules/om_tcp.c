/*	$CoreSDI: om_tcp.c,v 1.11 2001/03/07 21:35:15 alejo Exp $	*/
/*
     Copyright (c) 2001, Core SDI S.A., Argentina
     All rights reserved
   
     Redistribution and use in source and binary forms, with or without
     modification, are permitted provided that the following conditions
     are met:
   
     1. Redistributions of source code must retain the above copyright
        notice, this list of conditions and the following disclaimer.
     2. Redistributions in binary form must reproduce the above copyright
        notice, this list of conditions and the following disclaimer in the
        documentation and/or other materials provided with the distribution.
     3. Neither name of the Core SDI S.A. nor the names of its contributors
        may be used to endorse or promote products derived from this software
        without specific prior written permission.
   
     THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
     IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
     OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
     IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
     INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
     NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
     DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
     THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
     (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
     THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  om_tcp -- TCP output module
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <syslog.h>

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

#include "../modules.h"
#include "../syslogd.h"

struct om_tcp_ctx {
	int	fd;
	char	*host;
	char	*port;  /* either 'syslog' or number up to XXXXX */
};

int connect_tcp(const char *, const char *);
int om_tcp_close(struct filed *, void *);

/*
 *  INIT -- Initialize om_tcp
 *
 *  we get remote host and port
 *
 *  usage: -r tries to reconnect always (optional)
 *         -h host (required)
 *         -p port (required)
 *
 * we try to make it the most IPv6 compatible as we can
 * for future porting
 */

int
om_tcp_init(int argc, char **argv, struct filed *f, char *prog, void **ctx,
    char **status)
{
	struct	om_tcp_ctx *c;
	int	ch, retry;
	char	statbuf[1024];

	if (argv == NULL || argc != 5) {
		dprintf(MSYSLOG_INFORMATIVE, "om_tcp_init: wrong param count"
		    " %d, should be 5\n", argc);
		return (-1);
	}

	dprintf(MSYSLOG_INFORMATIVE, "om_tcp init: Entering\n");

	if ((*ctx = (void *) calloc(1, sizeof(struct om_tcp_ctx))) == NULL) {
		dprintf(MSYSLOG_INFORMATIVE, "om_tcp_init: couldn't allocate"
		    " context\n");
		return (-1);
	}
	c = (struct om_tcp_ctx *) *ctx;

	retry = 0;

	/* parse line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "rh:p:")) != -1) {
		switch (ch) {
		case 'r':
			retry++;
			break;
		case 'h':
			/* get remote host name/addr */
			c->host = strdup(optarg);
			break;
		case 'p':
			/* get remote host port */
			c->port = strdup(optarg);
			break;
		default:
			dprintf(MSYSLOG_SERIOUS, "om_tcp_init: parsing error"
			    " [%c]\n", ch);
			if (c->host)
				free(c->host);
			if (c->port)
				free(c->port);
			free(*ctx);
			return (-1);
		}
	}

	if ( !c->host || !c->port ) {
		dprintf(MSYSLOG_SERIOUS, "om_tcp_init: parsing\n");
		om_tcp_close(NULL, c);
		return (-1);
	}

	/* If -r was specified, om_tcp will try to reconnect later
	 * (beware, on every log!)
	 */
	if ( (c->fd = connect_tcp(c->host, c->port)) < 0) {

		dprintf(MSYSLOG_SERIOUS, "om_tcp_init: error connecting "
		    "to remote host %s, %s\n", c->host, c->port);

		if (retry == 0) {
			om_tcp_close(NULL, c);
			return (-1);
		}
	}

	snprintf(statbuf, sizeof(statbuf), "om_tcp: forwarding "
	    "messages through TCP to host %s, port %s", c->host,
	    c->port);
	*status = strdup(statbuf);

	return (1);
}


/*
 *  WRITE -- Initialize om_tcp
 *
 */

int
om_tcp_write(struct filed *f, int flags, char *msg, void *ctx)
{
	struct om_tcp_ctx *c;
	RETSIGTYPE (*sigsave)(int);
	char time_buf[16];
	char line[MAXLINE + 1];
	int l, i;

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_tcp_write: no message!");
		return (-1);
	}

	c = (struct om_tcp_ctx *) ctx;

	strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S", &f->f_tm);

	/* we give a newline termination to difference lines, unlike UDP */
	l = snprintf(line, sizeof(line), "<%d>%.15s %s\n", f->f_prevpri,
	    time_buf, msg);

	dprintf(MSYSLOG_INFORMATIVE, "om_tcp_write: sending to %s, %s",
	    c->host, line);

	/* Ignore sigpipes so broken connections won't bother */
	sigsave = place_signal(SIGPIPE, SIG_IGN);
  
	/* If down or couldn't write, reconnect  */
	for (i = 0 ; (c->fd < 0) || (write(c->fd, line, l) != l)
	    || i > 3 ; i++) {

		dprintf(MSYSLOG_SERIOUS, "om_tcp_write: broken connection "
		    "to remote host %s, port %s\n", c->host, c->port);

		/* just in case */
		if (c->fd > -1);
			close(c->fd);
		if ( (c->fd = connect_tcp(c->host, c->port)) < 0) {
	
			place_signal(SIGPIPE, sigsave);
  
			dprintf(MSYSLOG_CRITICAL, "om_tcp_write: "
			    "error re-connecting to remote host %s, "
			    "port %s\n", c->host, c->port);

			return (-1);
		}
	}

	place_signal(SIGPIPE, sigsave);

	if (i > 3)
		return (-1);
  
	f->f_prevcount = 0;
	return (1);
}


/*
 *  CLOSE -- close om_tcp
 *
 */

int
om_tcp_close(struct filed *f, void *ctx)
{
	struct om_tcp_ctx *c;

	c = (struct om_tcp_ctx *) ctx;
	if (c->host)
		free(c->host);
	if (c->port)
		free(c->port);

	if (c->fd);
		close (c->fd);

	return (1);
}
