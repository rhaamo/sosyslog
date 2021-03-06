/*	$CoreSDI: om_tcp.c,v 1.27 2002/03/01 07:31:03 alejo Exp $	*/
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

#define OM_TCP_MAX_RETRY_SLEEP_SECONDS 60

struct om_tcp_ctx {
	int	fd;
	char	*host;
	char	*port;  /* either 'syslog' or number up to XXXXX */
	char	*saved;
	int	savesize;
	int	savelen;
	unsigned int	msec;	/* maximum seconds to wait until connection retry */
	unsigned int	inc;	/* increase save */
	time_t	savet;		/* saved time of last failed reconnect */
	int	flags;
};

#define M_ADDHOST	0x01

int connect_tcp(char *, char *);
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
 *
 *  NOTE: connection will be established on first om_tcp_write !!
 */

int
om_tcp_init(int argc, char **argv, struct filed *f, char *prog, void **ctx,
    char **status)
{
	char			statbuf[1024];
	struct om_tcp_ctx	*c;
	int			ch;
	int			argcnt;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_tcp init: Entering\n");

	if ((*ctx = (void *) calloc(1, sizeof(struct om_tcp_ctx))) == NULL) {
		m_dprintf(MSYSLOG_CRITICAL, "om_tcp_init: couldn't allocate"
		    " context\n");
		return (-1);
	}
	c = (struct om_tcp_ctx *) *ctx;

	argcnt = 1;

	while ((ch = getxopt(argc, argv, "h!host: p!port: m!maxretrywait:"
	    " s!savesize: a!addhost", &argcnt)) != -1) {

		switch (ch) {
		case 'h':
			/* get remote host name/addr */
			c->host = strdup(argv[argcnt]);
			break;
		case 'p':
			/* get remote host port */
			c->port = strdup(argv[argcnt]);
			break;
		case 'm':
			/* get maximum seconds to wait on connect retry */
			c->msec = (unsigned int) strtol(argv[argcnt], NULL, 10);
			break;
		case 's':
			/* set saved buffer size */
			c->savesize = strtol(argv[argcnt], NULL, 10);
			c->saved = (char *) malloc(c->savesize);
			break;
		case 'a':
			c->flags |= M_ADDHOST;
			break;
		default:
			m_dprintf(MSYSLOG_SERIOUS, "om_tcp_init: parsing error"
			    " [%c]\n", ch);
			FREE_PTR(c->host);
			FREE_PTR(c->port);
			FREE_PTR(*ctx);
			return (-1);
		}
		argcnt++;
	}

	if ( !c->host || !c->port ) {
		m_dprintf(MSYSLOG_SERIOUS, "om_tcp_init: parsing\n");
		om_tcp_close(NULL, c);
		return (-1);
	}

	if (c->msec == 0)
		c->msec = OM_TCP_MAX_RETRY_SLEEP_SECONDS;
	c->inc = 2;
	c->savet = 0;
	c->fd = -1;

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
om_tcp_write(struct filed *f, int flags, struct m_msg *m, void *ctx)
{
	struct om_tcp_ctx *c;
	RETSIGTYPE (*sigsave)(int);
	char time_buf[16];
	char line[MAXLINE + 1];
	int l;

	if (m->msg == NULL || !strcmp(m->msg, "")) {
		logerror("om_tcp_write: no message!");
		return (-1);
	}

	c = (struct om_tcp_ctx *) ctx;

	strftime(time_buf, sizeof(time_buf), "%b %e %H:%M:%S", &f->f_tm);

	/* we give a newline termination to difference lines, unlike UDP */
	if (c->flags & M_ADDHOST) {
		l = snprintf(line, sizeof(line), "<%d>%.15s %s %s\n",
		    f->f_prevpri, time_buf, f->f_prevhost, m->msg);
	} else {
		l = snprintf(line, sizeof(line), "<%d>%.15s %s\n",
		    f->f_prevpri, time_buf, m->msg);
	}

	m_dprintf(MSYSLOG_INFORMATIVE, "om_tcp_write: sending to %s, %s",
	    c->host, line);

	/* Ignore sigpipes so broken connections won't bother */
	sigsave = place_signal(SIGPIPE, SIG_IGN);
  
	/*
	 * reconnect using (max_seconds - (max_seconds/n))
	 */

	/* If down or couldn't write, reconnect  */
	 if ( c->fd < 0 || (c->savelen && (write(c->fd, c->saved, c->savelen)
	    != c->savelen)) || (write(c->fd, line, l) != l) ) {
		time_t t;

		t = time(NULL);

		if (c->savet == 0) {
			c->savet = t;
		} else {
			register unsigned int n, s;

			n = (unsigned int) t;
			s = c->msec - (c->msec / c->inc);
			n -= s;

			m_dprintf(MSYSLOG_INFORMATIVE, "om_tcp_write: should "
			    "I retry? (now %u, lasttime %u, sleep %u,"
			    " next %u)...", (unsigned int) t,
			    (unsigned int) c->savet, s, n);

			if (n < c->savet) {
				m_dprintf(MSYSLOG_INFORMATIVE, "no!\n");
				if (c->saved && l < (c->savesize - c->savelen
				    - 1)) {
					strncat(c->saved, line, c->savesize
				   	    - 1 - c->savelen);
					c->savelen = strlen(c->saved);
				}
				return(0);
			}

			m_dprintf(MSYSLOG_INFORMATIVE, "yes!\n");

		}
			
		m_dprintf(MSYSLOG_SERIOUS, "om_tcp_write: no connection "
		    "to remote host %s, port %s. retry %i...  ", c->host,
		    c->port, c->inc - 1);

		/* just in case */
		CLOSE_FD(c->fd);
		if ( ((c->fd = connect_tcp(c->host, c->port)) < 0) ||
	 	    (c->savelen && (write(c->fd, c->saved, c->savelen)
		    != c->savelen)) || (write(c->fd, line, l) != l) ) {

			m_dprintf(MSYSLOG_SERIOUS, "still down! next retry "
			    "in %i seconds\n", c->msec - (c->msec / c->inc));

			c->inc++;
			c->savet = t;
			CLOSE_FD(c->fd);
			c->fd = -1;

			place_signal(SIGPIPE, sigsave);

			/* save this line too if posible */
			if (c->saved && l < (c->savesize - c->savelen - 1)) {
				strncat(c->saved, line, c->savesize - 1 -
				    c->savelen);
				c->savelen = strlen(c->saved);
			}

			return(0);

		} else {
			m_dprintf(MSYSLOG_SERIOUS, "reconnected!\n");
			c->inc = 2;
			c->savet = 0;
			if (c->savelen) {
				c->savelen = 0;
				c->saved[0] = '\0';
			}
		}
	} else if (c->savelen) {  /* yes this code is repeated, but CLEAR */
		c->savelen = 0;
		c->saved[0] = '\0';
	}
			

	place_signal(SIGPIPE, sigsave);

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
	FREE_PTR(c->host);
	FREE_PTR(c->port);
	CLOSE_FD(c->fd);
	FREE_PTR(c->saved);

	return (1);
}
