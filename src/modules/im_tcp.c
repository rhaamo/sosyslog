/*	$CoreSDI: im_tcp.c,v 1.8 2001/02/26 22:37:25 alejo Exp $	*/

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
#include <ctype.h>
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

/* recvfrom() and others like soclen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
# warning using socklen_t as int
#endif

struct tcp_conn {
	int		 fd;
	struct tcp_conn *next;
	struct sockaddr *cliaddr;
	socklen_t	 addrlen;
	char		 name[SIZEOF_MAXHOSTNAMELEN + 1];
};

struct im_tcp_ctx {
	socklen_t	 addrlen;
	struct tcp_conn	*conns;
};

#define MSYSLOG_MAX_TCP_CLIENTS 100
#define LISTENQ 100

int  listen_tcp(const char *, const char *, socklen_t *);
void printline (char *, char *, size_t, int);

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

        if (argc != 3) {
        	dprintf(DPRINTF_SERIOUS)("im_tcp: error on params! %d, should "
		    "be 3\n", argc);
        	return (-1);
        }

	if ( (I->im_ctx = calloc(1, sizeof(struct im_tcp_ctx))) == NULL)
        	return (-1);

	c = (struct im_tcp_ctx *) I->im_ctx;

	if ( (I->im_fd = listen_tcp(argv[1], argv[2], &c->addrlen)) < 0) {
        	dprintf(DPRINTF_SERIOUS)("im_tcp: error with listen_tcp()\n");
		return (-1);
	}

	/* no connections yet established */
	c->conns = NULL;

        I->im_path = NULL;

	add_fd_input(I->im_fd , I);

        dprintf(DPRINTF_INFORMATIVE)("im_tcp: running\n");

        return (1);
}


/*
 * im_tcp_read: accept a connection and add it to the queue
 *
 */

int
im_tcp_read(struct i_module *im, int infd, struct im_msg *ret)
{
	struct im_tcp_ctx *c;
	struct tcp_conn *con;
	int n;
#ifndef HAVE_GETNAMEINFO
	struct hostent *hp;
	struct sockaddr_in *sin4;
#endif

	if (im == NULL || ret == NULL) {
		dprintf(DPRINTF_SERIOUS)("im_tcp_read: arg %s%s is null\n",
		    ret? "ret":"", im? "im" : "");
		return (-1);
	}

	c = (struct im_tcp_ctx *) im->im_ctx;

	if (infd == im->im_fd) {
		struct sockaddr *cliaddrp;
		socklen_t slen;
		int fd;

		if ( (cliaddrp = (struct sockaddr *)
			calloc(1, c->addrlen)) == NULL)
			return (-1);
		slen = c->addrlen;

		/* accept it and add to queue */
		if ( (fd = accept(im->im_fd, cliaddrp,  &slen)) < 0) {
			free(cliaddrp);
			dprintf(DPRINTF_SERIOUS)("im_tcp: couldn't accept\n");
			return (-1);
		}

		/* create a new connection */
		if (c->conns == NULL) {
			if ( (con = (struct tcp_conn *)	
			    calloc(1, sizeof(struct tcp_conn))) == NULL) {
        			dprintf(DPRINTF_SERIOUS)("im_tcp: "
				    "error allocating conn struct\n");
				free(cliaddrp);
				return (-1);
			}
			c->conns = con;
		} else {
			for (con = c->conns; con->next; con = con->next);

			if (con->fd > -1) {
				if ( (con->next = (struct tcp_conn *)	
				    calloc(1, sizeof(struct tcp_conn)))
				    == NULL) {
        				dprintf(DPRINTF_SERIOUS)("im_tcp: "
					    "error allocating conn struct\n");
					free(cliaddrp);
					return (-1);
				}
				con = con->next;
			}
		}

		con->fd  = fd;
		con->cliaddr  = cliaddrp;
		con->addrlen  = slen;

#ifdef HAVE_GETNAMEINFO
		n = getnameinfo((struct sockaddr *) con->cliaddr, con->addrlen,
		    con->name, sizeof(con->name) - 1, NULL, 0, 0);
#else
		hp = NULL;
		if (con->cliaddr->sa_family == AF_INET) {
			sin4 = (struct sockaddr_in *) con->cliaddr;
			hp = gethostbyaddr((char *) &sin4->sin_addr,
			    sizeof(sin4->sin_addr), sin4->sin_family);
		}

		if (hp == NULL) {
			n = -1;
		} else {
			strncpy(con->name, hp->h_name, sizeof(con->name) - 1);
			con->name[sizeof(con->name) - 1] = '\0';
			n = 0;
		}
#endif


		if (n != 0) {
			
        		dprintf(DPRINTF_INFORMATIVE)("im_tcp: error resolving "
			    "remote host name! [%d]\n", n);

#ifdef HAVE_INET_NTOP
			inet_ntop(con->cliaddr->sa_family, &con->cliaddr,
			    con->name, sizeof(con->name) - 1);
#endif
		}	

		dprintf(DPRINTF_INFORMATIVE)("im_tcp_read: new conection from"
		    " %s with fd %d\n", con->name, con->fd);

		/* add to inputs list */
		add_fd_input(con->fd , im);

		return (0); /* 0 because there is no line to log */

	}

	/* read connected socket */

	dprintf(DPRINTF_INFORMATIVE)("im_tcp_read: reading connection fd %d\n",
	    infd);

	/* find connection */
	for (con = c->conns; con && con->fd != infd; con = con->next);

	if (con == NULL || con->fd != infd) {
		dprintf(DPRINTF_SERIOUS)("im_tcp_read: no such connection "
		    "fd %d !\n", infd);
		return (-1);
	}

	n = read(con->fd, im->im_buf, sizeof(im->im_buf) - 1);
	if (n == 0) {
		struct tcp_conn *prev;

		dprintf(DPRINTF_INFORMATIVE)("im_tcp_read: conetion from %s"
		    " closed\n", con->name);

		remove_fd_input(con->fd);

		/* connection closed, remove its tcp_con struct */
		if (con->cliaddr)
			free(con->cliaddr);
		close (con->fd);

		for(prev = c->conns; prev && prev != con && prev->next
		    && prev->next != con; prev = prev->next);

		if (prev == c->conns && prev == con) {
			/* c->cons and prev point to con now */
			c->conns = con->next;
		} else if (prev->next == con) {
			prev->next = con->next;
		}

		free(con);
		return (0);
	}

	/* Remove trailing newlines. Newlines in the middle are ok
	   as line separators */
	if (n == 1) {
		if (im->im_buf[0] == '\r' || im->im_buf[0] == '\n')
			return (0); /* nothing to log */
	} else if (n > 1) {
		if (im->im_buf[n - 2] == '\r' && im->im_buf[n - 1] == '\n') {
			im->im_buf[n - 2] = '\0';
			n -= 2;
		} else if (im->im_buf[n - 1] == '\n') {
			im->im_buf[n - 1] = '\0';
			n -= 1;
		}
	}

	if (n == 0) {
		dprintf(DPRINTF_INFORMATIVE)("im_tcp_read: empty message\n");
		return (0); /* nothing to log */
 	} else if (n < 0 && errno != EINTR) {

		dprintf(DPRINTF_SERIOUS)("im_tcp_read: error reading (and it"
		    " was not because of an interruption)\n");
		logerror("im_tcp_read");
		con->fd = -1;

        } else {
		char *p, *q, *lp;
		int c;

		(im->im_buf)[n] = '\0';

		lp = ret->im_msg;

		for (p = im->im_buf; *p != '\0'; ) {

			q = lp;
			c = '\0';

			/* copy line */
			while (*p != '\0' && (c = *p++) != '\r' &&
			    c != '\n' && q < (ret->im_msg + ret->im_mlen))
	 			*q++ = c;
			*q = '\0';

			/* get rid of \r\n too */
			if (c == '\r' && *p == '\n')
				p++;

			strncpy(ret->im_host, con->name,
			    sizeof(ret->im_host) - 1);
			ret->im_host[sizeof(ret->im_host) - 1] = '\0';

			ret->im_len = strlen(ret->im_msg);

			printline(ret->im_host, ret->im_msg, ret->im_len,
			    ret->im_flags);
		}

	}

	return (0); /* we already logged the lines */

}

int
im_tcp_close(struct i_module *im)
{
	struct im_tcp_ctx *c;
	struct tcp_conn *con;

        c = (struct im_tcp_ctx *) im->im_ctx;
	/* close all connections */
	for (con = c->conns; con; con = con->next) {
		if (con->cliaddr)
			free(con->cliaddr);
		if (con->fd > -1)
			close(con->fd);
	}

        /* close listening socket */
        return (close(im->im_fd));
}



/*
 * listen_tcp: listen on local host/port
 *              return the file descriptor
 */

int
listen_tcp(const char *host, const char *port, socklen_t *addrlenp) {
	int fd, i;
#ifdef HAVE_GETADDRINFO
	struct addrinfo hints, *res, *ressave;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (i = getaddrinfo(host, port, &hints, &res)) != 0) {

		dprintf(DPRINTF_INFORMATIVE)("listen_tcp: error on address "
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
		fd = -1;

	};

	if (res == NULL) {
		dprintf(DPRINTF_INFORMATIVE)("listen_tcp: error binding "
		    "to host address %s, %s\n", host, port);
		freeaddrinfo(ressave);
		return (-1);
	}

	freeaddrinfo(ressave);

#else	/* we are on an extremely outdated and ugly api */
	struct hostent *hp;
	struct servent *sp;
	struct sockaddr_in servaddr;
	struct in_addr **paddr;
	short portnum;

	if ( (sp = getservbyname(port, "tcp")) == NULL) {
		if ( (portnum = htons((short) atoi(port))) == 0 ) {
			dprintf(DPRINTF_SERIOUS)("tcp_listen: error resolving "
			    "port number %s, %s\n", host, port);
			return (-1);
		}
	} else
		portnum = sp->s_port;

	if ( (hp = gethostbyname(host)) == NULL ) {
		dprintf(DPRINTF_SERIOUS)("tcp_listen: error resolving "
		    "host address %s, %s\n", host, port);
		return (-1);
	}

	paddr = (struct in_addr **) hp->h_addr_list;
	fd = -1;

	for ( ; *paddr != NULL; paddr++) {

		if ( (fd = socket( AF_INET, SOCK_STREAM, 0)) < 0) {
			dprintf(DPRINTF_SERIOUS)("tcp_listen: error creating socket "
			    "for host address %s, %s\n", host, port);
			return (-1);
		}

		i = 1;

		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) != 0) {
			dprintf(DPRINTF_SERIOUS)("tcp_listen: error setting socket "
			    "options for host address %s, %s\n", host, port);
			return (-1);
		}

		memset(&servaddr, 0, sizeof(servaddr)); 
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = portnum;
		memcpy(&servaddr.sin_addr, *paddr, sizeof(servaddr.sin_addr));

		if (bind(fd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == 0)
			break; /* ok! */

		close(fd); /* couldn't bind */
		fd = -1;
	}

#endif

	if (fd == -1) {
		dprintf(DPRINTF_SERIOUS)("tcp_listen: error binding "
		    "to host address %s, %s\n", host, port);
		return (-1);
	}

	if (listen(fd, LISTENQ) != 0) {
		dprintf(DPRINTF_SERIOUS)("tcp_listen: error listening "
		    "on host address %s, %s\n", host, port);
		return (-1);
	}

	if (addrlenp)
#ifdef HAVE_GETADDRINFO
		*addrlenp = res->ai_addrlen;
#else
		*addrlenp = sizeof(servaddr)
#endif

	return (fd);

}

