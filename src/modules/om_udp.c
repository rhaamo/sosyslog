/*	$CoreSDI$	*/
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
 *  om_udp -- UDP output module
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <errno.h>
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

struct om_udp_ctx {
	int	fd;
	char	*host;
	char	*port;
	void	*addr;	/* remote address */
	int	addrlen;
	int	flags;
};

#define M_ADDHOST	0x01

int sock_udp(char *, char *, void **, int *);
int udp_send(int, char *, int, void *, int);

/*
 *  INIT -- Initialize om_udp
 *
 *  we get remote host and port
 *
 *  usage: -h host (required)
 *         -p port (required)
 *         -a	   (add hostname to message)
 *
 * we try to make it the most IPv6 compatible as we can
 * for future porting
 *
 */

int
om_udp_init(int argc, char **argv, struct filed *f, char *prog, void **ctx,
    char **status)
{
	struct	om_udp_ctx	*c;
	int			ch;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_udp init: Entering\n");

	if ((*ctx = (void *) calloc(1, sizeof(struct om_udp_ctx))) == NULL) {
		m_dprintf(MSYSLOG_INFORMATIVE, "om_udp_init: couldn't allocate"
		    " context\n");
		return (-1);
	}
	c = (struct om_udp_ctx *) *ctx;

	/* parse line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "h:p:m:s:a")) != -1) {
		switch (ch) {
		case 'h':
			/* get remote host name/addr */
			c->host = strdup(optarg);
			break;
		case 'p':
			/* get remote host port */
			c->port = strdup(optarg);
			break;
		case 'a':
			c->flags |= M_ADDHOST;
			break;
		default:
			m_dprintf(MSYSLOG_SERIOUS, "om_udp_init: parsing error"
			    " [%c]\n", ch);
			if (c->host)
				free(c->host);
			if (c->port)
				free(c->port);
			free(*ctx);
			return (-1);
		}
	}

	if ( c->host == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_udp_init: host unspecified\n");
		return (-1);
	}

	errno = 0;
	if ((c->fd = sock_udp(c->host, c->port == NULL? "syslog" : c->port,
	    &c->addr, &c->addrlen)) == -1) {
		m_dprintf(MSYSLOG_SERIOUS, "om_udp_init: error creating generic"
		    " outgoing UDP socket [%s]", strerror(errno));
		return (-1);
	}

	return (1);
}


/*
 *  WRITE -- Send a message
 *
 */

int
om_udp_write(struct filed *f, int flags, char *msg, void *ctx)
{
	struct om_udp_ctx *c;
	char time_buf[16];
	char line[MAXLINE + 1];
	int l;

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_udp_write: no message!");
		return (-1);
	}

	c = (struct om_udp_ctx *) ctx;

	strftime(time_buf, sizeof(time_buf), "%b %e %H:%M:%S", &f->f_tm);

	/* we give a newline termination to difference lines, unlike UDP */
	if (c->flags & M_ADDHOST) {
		l = snprintf(line, sizeof(line), "<%d>%.15s %s %s\n",
		    f->f_prevpri, time_buf, f->f_prevhost, msg);
	} else {
		l = snprintf(line, sizeof(line), "<%d>%.15s %s\n",
		    f->f_prevpri, time_buf, msg);
	}

	m_dprintf(MSYSLOG_INFORMATIVE, "om_udp_write: sending to %s:%s, %s",
	    c->host, c->port, line);

	if (udp_send(c->fd, line, l, c->addr, c->addrlen) == -1) {

		m_dprintf(MSYSLOG_SERIOUS, "om_udp_write: error sending "
		    "to remote host [%s]", strerror(errno));
	}
			
	return (1);
}


/*
 *  CLOSE -- close om_udp
 *
 */

int
om_udp_close(struct filed *f, void *ctx)
{
	struct om_udp_ctx *c;

	c = (struct om_udp_ctx *) ctx;

	if (c->host)
		free(c->host);
	if (c->port)
		free(c->port);
	if (c->addr)
		free(c->addr);

	close(c->fd);

	return (1);
}
