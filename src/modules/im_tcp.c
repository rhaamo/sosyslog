/*	$CoreSDI: im_tcp.c,v 1.52 2000/12/14 00:16:44 alejo Exp $	*/

/*
 * Copyright (c) 2000, Core SDI S.A., Argentina
 * All rights reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither name of the Core SDI S.A. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * im_tcp -- input from INET using TCP
 *      
 * Author: Alejo Sanchez for Core SDI S.A.
 * 
 * This input module is a bit tricky, because of the nature of TCP
 * connections, and the use of poll() for I/O on syslogd
 * 
 * The main idea is that first a im_tcp module will be called
 * and it will bind to a port and wait for connections to it.
 * 
 * Whenever a conection is established it will add it to an
 * array of file descriptors of connections.
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

/* recvfrom() and others like soclen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
# warning using socklen_t as int
#endif

struct tcp_conn {
	int		 fd;
	int		 index;
	struct tcp_conn *next;
	struct sockaddr *cliaddr;
	socklen_t	 addrlen;
};

struct im_tcp_ctx {
	socklen_t	addrlen;
	struct tcp_conn	conns;
};

#define MSYSLOG_MAX_TCP_CLIENTS 100
#define LISTENQ 100

int listen_tcp(const char *, const char *, socklen_t *);

/*
 * initialize tcp input
 *
 * this module takes a host argument (ie. 0.0.0.0, 0::0, server.example.com)
 * and a port/service ('syslog' or numerical)
 *
 */

int
im_tcp_init(struct i_module *I, char **argv, int argc)
{
	struct im_tcp_ctx *c;

        if (argc != 2) {
        	dprintf(DPRINTF_SERIOUS)("im_tcp: error on params!\n");
        	return (-1);
        }

	if ( (I->im_ctx = calloc(1, sizeof(struct im_tcp_ctx))) == NULL)
        	return (-1);

	c = (struct im_tcp_ctx *) I->im_ctx;

	if (listen_tcp(argv[1], argv[2], &c->addrlen) < 0)
		return (-1);

	/* init connections to empty */
	c->conns.fd = -1;
	c->conns.index = 0;
	c->conns.next = NULL;

        I->im_path = NULL;

	add_fd_input(I->im_fd , I, 0);

        dprintf(DPRINTF_INFORMATIVE)("im_tcp: running\n");

        return (1);
}


/*
 * im_tcp_read: accept a connection and add it to the queue
 *
 */

int
im_tcp_read(struct i_module *im, int index, struct im_msg *ret)
{
	struct im_tcp_ctx *c;

	if (im == NULL) {
		dprintf(DPRINTF_SERIOUS)("im_tcp: arg is null\n");
		return (-1);
	}

	c = (struct im_tcp_ctx *) im->im_ctx;

	if (index == 0) {
		struct tcp_conn *con;
		struct sockaddr *cliaddrp;
		socklen_t slen;
		int fd, count;

		cliaddrp = malloc(c->addrlen);
		slen = c->addrlen;

		/* accept it and add to queue */
		if ( (fd = accept(im->im_fd, cliaddrp,  &slen)) < 0) {
			free(cliaddrp);
			dprintf(DPRINTF_SERIOUS)("im_tcp: couldn't accept\n");
			return (-1);
		}

		/* create a new connection */
		for (con = &c->conns, count = 0; con->next; con = con->next)
			if (con->index > count)
				count = con->index;

		if (con->fd > -1) {
			con->next = (struct tcp_conn *)	
			    calloc(1, sizeof(struct tcp_conn));
			con = con->next;
		}

		con->fd  = fd;
		con->cliaddr  = cliaddrp;
		con->addrlen  = slen;

		/* add to inputs list */
		add_fd_input(con->fd , im, ++count);

		return (0); /* 0 because there is no line to log */

	}

	/* read connected socket */



	return (1);

}

struct tcp_conn {
	int		 fd;
	int		 index;
	struct tcp_conn *next;
	struct sockaddr *cliaddr;
	socklen_t	 addrlen;
};

struct im_tcp_ctx {
	socklen_t	addrlen;
	struct tcp_conn	conns;
};
int
im_tcp_close(struct i_module *im)
{
	struct im_tcp_ctx *c;
	struct tcp_conn *con;

        c = (struct im_tcp_ctx *) im->im_ctx;
	/* close all connections */
	for (con = &c->conns; con; con = con->next)
		if (con->fd > -1)
			close(con->fd);

        return (close(im->im_fd));
}



/*
 * listen_tcp: listen on local host/port
 *              return the file descriptor
 */

int
listen_tcp(const char *host, const char *port, socklen_t *addrlenp) {
	struct addrinfo hints, *res, *ressave;
	int fd, i;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (i = getaddrinfo(host, port, &hints, &res)) != 0) {

		dprintf(DPRINTF_INFORMATIVE)("im_tcp_init: error on address "
		    "to listen %s, %s: %s\n", host, port,
		    gai_strerror(i));

		return (-1);

	}

	i = 1;

	for (ressave = res; res != NULL; res = res->ai_next) {

		if ( (fd = socket(res->ai_family, res->ai_socktype,
		    res->ai_protocol)) < 0)
			continue; /* ignore */

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i,
		    sizeof(i)) != 0) {
			freeaddrinfo(ressave);
			return (-1);
		}

		if (bind(fd, res->ai_addr, res->ai_addrlen) == 0)
			break; /* ok! */

		close(fd); /* couldn't connect */

	};

	if (res == NULL) {
		dprintf(DPRINTF_INFORMATIVE)("im_tcp_init: error binding "
		    "to host address %s, %s\n", host, port);
		freeaddrinfo(ressave);
		return (-1);
	}

	freeaddrinfo(ressave);

	if (listen(fd, LISTENQ) != 0) {
		dprintf(DPRINTF_INFORMATIVE)("im_tcp_init: error listening "
		    "on host address %s, %s\n", host, port);
		return (-1);
	}

	if (addrlenp)
		*addrlenp = res->ai_addrlen;

	return (fd);

}

