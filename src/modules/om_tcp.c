/*	$CoreSDI: om_tcp.c,v 1.2 2001/02/16 00:34:52 alejo Exp $	*/
/*
     Copyright (c) 2000, Core SDI S.A., Argentina
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
 *         from syslogd.c Eric Allman  and Ralph Campbell
 *
 */

#include "../../config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netdb.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
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

#define OM_TCP_KEEPALIVE 30	/* seconds to probe TCP connection */

struct om_tcp_ctx {
	char	host[SIZEOF_MAXHOSTNAMELEN];
	char	port[7];  /* either 'syslog' or number up to XXXXX */
};

int connect_tcp(const char *host, const char *port);

/*
 *  INIT -- Initialize om_tcp
 *
 *  we get remote host and port
 *
 * we try to make it the most IPv6 compatible as we can
 * for future porting
 */

int
om_tcp_init(int argc, char **argv, struct filed *f, char *prog, void **ctx)
{
	struct	om_tcp_ctx *c;
	int	ch;

	if (argv == NULL || argc != 5) {
		dprintf(DPRINTF_INFORMATIVE)("om_tcp_init: wrong param count"
		    " %d, should be 5\n", argc);
		return (-1);
	}

	dprintf(DPRINTF_INFORMATIVE)("om_tcp init: Entering\n");

	if ((*ctx = (void *) calloc(1, sizeof(struct om_tcp_ctx))) == NULL) {
		dprintf(DPRINTF_INFORMATIVE)("om_tcp_init: couldn't allocate"
		    " context\n");
		return (-1);
	}
	c = (struct om_tcp_ctx *) *ctx;

	/* parse line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "h:p:")) != -1) {
		switch (ch) {
		case 'h':
			/* get remote host name/addr */
			strncpy(c->host, optarg, sizeof(c->host));
			c->host[sizeof(c->host) - 1] = '\0';
			break;
		case 'p':
			/* get remote host port */
			strncpy(c->port, optarg, sizeof(c->port));
			c->port[sizeof(c->port) - 1] = '\0';
			break;
		default:
			dprintf(DPRINTF_SERIOUS)("om_tcp_init: parsing error"
			    " [%c]\n", ch);
			free(*ctx);
			*ctx = NULL;
			return (-1);
		}
	}

	if ( (c->host[0] == '\0') || (c->port[0] == '\0') ) {

		dprintf(DPRINTF_SERIOUS)("om_tcp_init: parsing\n");
		free(*ctx);
		*ctx = NULL;

		return (-1);
	}

	if ( (f->f_file = connect_tcp(c->host, c->port)) < 0) {

		dprintf(DPRINTF_SERIOUS)("om_tcp_init: error connecting "
		    "to remote host %s, %s\n", c->host, c->port);
		free(*ctx);
		*ctx = NULL;

		return (-1);
	}

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
	char time_buf[16];
	char line[MAXLINE + 1];
	int l;


	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_tcp_write: no message!");
		return (-1);
	}

	c = (struct om_tcp_ctx *) ctx;

	/* if not connected, reconnect ! */
	if ( !f->f_file && (f->f_file = connect_tcp(c->host, c->port)) < 0) {
		dprintf(DPRINTF_CRITICAL)("om_tcp_init: "
		    "error connecting to remote host %s, "
		    "%s\n", c->host, c->port);
		return (-1);
	}

	strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S", &f->f_tm);
	dprintf(DPRINTF_INFORMATIVE)(" %s\n", c->host);

	/* we give a newline termination, unlike UDP, to difference lines */
	l = snprintf(line, sizeof(line), "<%d>%.15s %s\n", f->f_prevpri,
	    time_buf, msg);

	dprintf(DPRINTF_INFORMATIVE)("om_tcp_write: sending to %s, %s]",
	    c->host, line);

	if (write(f->f_file, line, l) != l) {
		logerror("om_tcp_write: write()");
	}

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

	return (close(f->f_file));
}


/*
 * connect_tcp: connect to a remote host/port
 *              return the file descriptor
 */

int
connect_tcp(const char *host, const char *port) {
	struct addrinfo hints, *res, *ressave;
	int fd, n;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	dprintf(DPRINTF_INFORMATIVE)("connect_tcp: called for host %s , "
	    "port %s\n", host, port);

	if ( (n = getaddrinfo(host, port, &hints, &res)) != 0) {

		dprintf(DPRINTF_INFORMATIVE)("connect_tcp: error on address "
		    "of remote host %s, %s: %s\n", host, port,
		    gai_strerror(n));

		return (-1);

	}

	n = OM_TCP_KEEPALIVE;

	for (ressave = res; res != NULL; res = res->ai_next) {

		if ( (fd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol)) < 0)
			continue; /* ignore */

		if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &n,
		    sizeof(n)) != 0) {
			freeaddrinfo(ressave);
			return (-1);
		}

		if (connect(fd, res->ai_addr, res->ai_addrlen) == 0)
			break; /* ok! */

		close(fd); /* couldn't connect */

	};

	if (res == NULL) {
		dprintf(DPRINTF_INFORMATIVE)("connect_tcp: error connecting "
		    "to remote host %s, %s\n", host, port);
		freeaddrinfo(ressave);
		return (-1);
	}

	freeaddrinfo(ressave);

	return (fd);

}

