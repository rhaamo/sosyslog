/*	$CoreSDI: om_tcp.c,v 1.58 2000/12/14 00:16:44 alejo Exp $	*/
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
	struct om_tcp_ctx *c;

	if (argv == NULL || argc != 3)
		return (-1);

	dprintf(DPRINTF_INFORMATIVE)("om_tcp init: Entering\n");

	if ((*ctx = (void *) calloc(1, sizeof(struct om_tcp_ctx))) == NULL)
		return (-1);
	c = (struct om_tcp_ctx *) *ctx;

	strncpy(c->host, argv[1], sizeof(c->host));
	c->host[sizeof(c->host) - 1] = '\0';
	strncpy(c->port, argv[2], sizeof(c->port));
	c->port[sizeof(c->port) - 1] = '\0';

	if ( (f->f_file = connect_tcp(c->host, c->port)) < 0) {
		dprintf(DPRINTF_INFORMATIVE)("om_tcp_init: error connecting "
		    "to remote host %s, %s\n", c->host, c->port);
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
	struct iovec iov[6], *v;
	struct om_tcp_ctx *c;
	char time_buf[16];


	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_tcp_write: no message!");
		return (-1);
	}

	c = (struct om_tcp_ctx *) ctx;

	/* prepare buffers for writing */
	v = iov;
	strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S", &f->f_tm);
	v->iov_base = time_buf;
	v->iov_len = 15;
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;
	v->iov_base = f->f_prevhost;
	v->iov_len = strlen(v->iov_base);
	v++;
	v->iov_base = " ";
	v->iov_len = 1;
	v++;

	v->iov_base = msg;
	v->iov_len = strlen(msg);
	v++;

	v->iov_base = "\r\n";
	v->iov_len = 2;

	if (writev(f->f_file, iov, 6) < 0) {
		char errbuf[90];

		snprintf(errbuf, sizeof(errbuf) - 1, "om_tcp_write: error: "
		    "writing to %s:%s%s\n", c->host, c->port, strerror(errno));

		logerror(errbuf);

		if ( (f->f_file = connect_tcp(c->host, c->port)) < 0) {
			f->f_type = F_UNUSED;
			f->f_file = -1;
			dprintf(DPRINTF_CRITICAL)("om_tcp_init: "
			    "error connecting to remote host %s, "
			    "%s\n", c->host, c->port);
			return (-1);
		}

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

	if ( (n = getaddrinfo(host, port, &hints, &res)) != 0) {

		dprintf(DPRINTF_INFORMATIVE)("om_tcp_init: error on address "
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
		dprintf(DPRINTF_INFORMATIVE)("om_tcp_init: error connecting "
		    "to remote host %s, %s\n", host, port);
		freeaddrinfo(ressave);
		return (-1);
	}

	freeaddrinfo(ressave);

	return (fd);

}

