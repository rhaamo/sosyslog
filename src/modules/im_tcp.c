/*	$CoreSDI: im_tcp.c,v 1.18 2001/06/14 01:24:09 alejo Exp $	*/

/*
 * Copyright (c) 2001, Core SDI S.A., Argentina
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

#include "config.h"


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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include "../modules.h"
#include "../syslogd.h"

/* recvfrom() and others like socklen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
#endif

struct tcp_conn {
	struct tcp_conn *next;
	int		 fd;
	char		 name[SIZEOF_MAXHOSTNAMELEN + 1];
	char		 port[20];
};

struct im_tcp_ctx {
	socklen_t	 addrlen;
	struct tcp_conn	*first;
	struct tcp_conn	*last;
};

void printline(char *, char *, size_t, int);
char * resolve_addr(struct sockaddr *addr, socklen_t addrlen);
struct sockaddr * resolv_name(const char *host, const char *port);
int listen_tcp(const char *host, const char *port, socklen_t *);
int accept_tcp(int, socklen_t, char *, int, char *, int);


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
	struct im_tcp_ctx	*c;
	char			*host, *port;

        dprintf(MSYSLOG_INFORMATIVE, "im_tcp_init: entering\n");
        if (argc == 2) {
			host = NULL;
			port = argv[1];
        } else if (argc == 3) {
			host = argv[1];
			port = argv[2];
        } else {
        	dprintf(MSYSLOG_SERIOUS, "im_tcp: error on params! %d, should "
		    "be 3\n", argc);
        	return (-1);
	}

	if ( (I->im_ctx = calloc(1, sizeof(struct im_tcp_ctx))) == NULL)
        	return (-1);

	c = (struct im_tcp_ctx *) I->im_ctx;

	if ( (I->im_fd = listen_tcp(host, port, &c->addrlen)) < 0) {
        	dprintf(MSYSLOG_SERIOUS, "im_tcp: error with listen_tcp()\n");
		return (-1);
	}

        I->im_path = NULL;

	add_fd_input(I->im_fd , I);

        dprintf(MSYSLOG_INFORMATIVE, "im_tcp: running\n");

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

	if (im == NULL || ret == NULL) {
		dprintf(MSYSLOG_SERIOUS, "im_tcp_read: arg %s%s is null\n",
		    ret? "ret":"", im? "im" : "");
		return (-1);
	}

	if ((c = (struct im_tcp_ctx *) im->im_ctx) == NULL) {
		dprintf(MSYSLOG_SERIOUS, "im_tcp_read: null context\n");
		return (-1);
	}

	if (infd == im->im_fd) {

		/* create a new connection */
		if ((con = (struct tcp_conn *) calloc(1, sizeof(*con)))
		    == NULL) {
       			dprintf(MSYSLOG_SERIOUS, "im_tcp: "
			    "error allocating conn struct\n");
			return (-1);
		}

		/* accept it and add to queue */
		if ((con->fd = accept_tcp(infd, c->addrlen, con->name,
		    sizeof(con->name), con->port, sizeof(con->port))) < 0) {
			dprintf(MSYSLOG_SERIOUS, "im_tcp: couldn't accept\n");
			free (con);
			return (-1);
		}

		/* add to queue */
		if (c->last == NULL) {
			c->first = con;
		} else {
			c->last->next = con;
		}
		c->last = con;


		dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: new conection from"
		    " %s with fd %d\n", con->name, con->fd);

		/* add to inputs list */
		add_fd_input(con->fd , im);

		return (0); /* 0 because there is no line to log */

	}

	/* read connected socket */

	dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: reading connection fd %d\n",
	    infd);

	/* find connection */
	for (con = c->first; con && con->fd != infd; con = con->next);

	if (con == NULL || con->fd != infd) {
		dprintf(MSYSLOG_SERIOUS, "im_tcp_read: no such connection "
		    "fd %d !\n", infd);
		return (-1);
	}

	n = read(con->fd, im->im_buf, sizeof(im->im_buf) - 1);
	if (n == 0) {
		struct tcp_conn *prev;

		dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: conetion from %s"
		    " closed\n", con->name);

		remove_fd_input(con->fd);

		/* connection closed, remove its tcp_con struct */
		close (con->fd);

		for(prev = c->first; prev && prev != con && prev->next
		    && prev->next != con; prev = prev->next);

		if (prev == c->first && prev == con) {
			/* c->cons and prev point to con now */
			c->first = con->next;
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
		return (0); /* nothing to log */
 	} else if (n < 0 && errno != EINTR) {
		logerror("im_tcp_read");
		con->fd = -1;
        } else {
		char *p, *q, *lp;
		int ch;

		(im->im_buf)[n] = '\0';

		lp = ret->im_msg;

		for (p = im->im_buf; *p != '\0'; ) {

			q = lp;
			ch = '\0';

			/* copy line */
			while (*p != '\0' && (ch = *p++) != '\r' &&
			    ch != '\n' && q < (ret->im_msg + ret->im_mlen))
	 			*q++ = ch;
			*q = '\0';

			/* get rid of \r\n too */
			if (ch == '\r' && *p == '\n')
				p++;

			strncpy(ret->im_host, con->name,
			    sizeof(ret->im_host) - 1);
			ret->im_host[sizeof(ret->im_host) - 1] = '\0';

			ret->im_len = strlen(ret->im_msg);

			printline(ret->im_host, ret->im_msg,
			    (unsigned) ret->im_len, ret->im_flags);
		}

	}

	return (0); /* we already logged the lines */
}

int
im_tcp_close(struct i_module *im)
{
	struct im_tcp_ctx *c;
	struct tcp_conn *con, *cnext;

        c = (struct im_tcp_ctx *) im->im_ctx;

	/* close all connections */
	for (con = c->first; con; con = cnext) {
		if (con->fd > -1)
			close(con->fd);
		cnext = con->next;
		free(con);
	}

	im->im_ctx = NULL;

        /* close listening socket */
        return (close(im->im_fd));
}
