/*	$CoreSDI: im_tcp.c,v 1.35 2002/03/01 07:31:02 alejo Exp $	*/

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
#include <ctype.h>
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
	struct tcp_conn *next;           /* next item in the singlely linked list */
	int   fd;                        /* the socket (file) descriptor */
	char  name[MAXHOSTNAMELEN + 1];  /* then hostname being listened for */
	char  port[20];                  /* the port being listened on */
  /* unprocessed bytes from socket, partial lines only */
	char  saveline[MAXLINE + 3];    /* [maxline + cr + lf + null] */
};

struct im_tcp_ctx {
	socklen_t	 addrlen;
	struct tcp_conn	*first;
	struct tcp_conn	*last;
	int		flags;
};

#define	M_USEMSGHOST	0x01
#define	M_NOTFQDN	0x02


void printline(char *, char *, size_t, int);
int listen_tcp(char *host, char *port, socklen_t *);
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
	int			ch, argcnt;

	m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_init: entering\n");

	if ( (I->im_ctx = calloc(1, sizeof(struct im_tcp_ctx))) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_tcp_init: cant alloc memory");
return (-1);
	}

	c = (struct im_tcp_ctx *) I->im_ctx;

	host = "0.0.0.0";
	port = "syslog";

	argcnt = 1; /* skip module name */

	while ((ch = getxopt(argc, argv, "h!host: p!port: a!addhost q!nofqdn",
	    &argcnt)) != -1) {

		switch (ch) {
		case 'h':
			/* get addr to bind */
			host = argv[argcnt];
			break;
		case 'p':
			/* get remote host port */
			port = argv[argcnt];
			break;
		case 'a':
			c->flags |= M_USEMSGHOST;
			break;
		case 'q':
			/* don't use domain in hostname (FQDN) */
			c->flags |= M_NOTFQDN;
			break;
		default:
			m_dprintf(MSYSLOG_SERIOUS, "om_tcp_init: parsing error"
			    " [%c]\n", ch);
			free(c);
return (-1);
		}
		argcnt++;
	}

	if ( (I->im_fd = listen_tcp(host, port, &c->addrlen)) < 0) {
		m_dprintf(MSYSLOG_SERIOUS, "im_tcp_init: error with listen_tcp() %s\n",
		    strerror(errno));
		free(c);
return (-1);
	}

	I->im_path = NULL;

	add_fd_input(I->im_fd , I);

	m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_init: running\n");

return (1);
}


/*
 * im_tcp_read: accept a connection and add it to the queue
 * connections and modules are read in a round-robin so partial lines 
 * must persist across calls to the im_read functions for the
 * various modules.
 */

int
im_tcp_read(struct i_module *im, int infd, struct im_msg *ret)
{
	struct im_tcp_ctx *c;
	struct tcp_conn *con;
	int n;

	m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: entering...\n");

	if (im == NULL || ret == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "im_tcp_read: arg %s%s is null\n",
		    ret? "ret":"", im? "im" : "");
return (-1);
	}

	if ((c = (struct im_tcp_ctx *) im->im_ctx) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "im_tcp_read: null context\n");
return (-1);
	}

	if (infd == im->im_fd) {

		m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: new connection\n");

		/* create a new connection */
		if ((con = (struct tcp_conn *) calloc(1, sizeof(*con)))
		    == NULL) {
			m_dprintf(MSYSLOG_SERIOUS, "im_tcp_read: "
			    "error allocating conn struct\n");
return (-1);
		}

		/* accept it and add to queue (intializing the 'fd', 'name' and 'port' */
		if ((con->fd = accept_tcp(infd, c->addrlen, con->name,
		    sizeof(con->name), con->port, sizeof(con->port))) < 0) {
			m_dprintf(MSYSLOG_SERIOUS, "im_tcp_read: couldn't accept\n");
			free (con);
return (-1);
		}

    /* initialize saveline */
		con->saveline[0] = '\0';

		/* add to queue */
		if (c->last == NULL) {
			c->first = con;
		} else {
			c->last->next = con;
		}
		c->last = con;

		m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: new conection from"
		    " %s with fd %d\n", con->name, con->fd);

		/* add to inputs list */
		add_fd_input(con->fd , im);

return (0); /* 0 because there is no line to log */
	}

	/* read connected socket */

	m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: reading connection fd %d\n",
	    infd);

	/* find connection */
	for (con = c->first; con && con->fd != infd; con = con->next);

	if (con == NULL || con->fd != infd) {
		m_dprintf(MSYSLOG_SERIOUS, "im_tcp_read: no such connection "
		    "fd %d !\n", infd);
		remove_fd_input(infd);
return (-1);
	}


	n = read(con->fd, im->im_buf, sizeof(im->im_buf) - 1);
	if (n == 0) {  /* no data read from this connection */
		struct tcp_conn *prev;

		m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: conetion from %s"
		    " closed\n", con->name);

		remove_fd_input(con->fd);

		/* connection closed, remove its tcp_con struct */
		close (con->fd);

		if (con == c->first) {
			c->first = con->next;
			if (con == c->last)
				c->last = NULL;
		} else {
			for(prev = c->first; prev->next != con;
			    prev = prev->next);
			prev->next = con->next;
		}

		if (con->saveline[0] != '\0')
			printline(ret->im_host, con->saveline, strlen(con->saveline),  0);

		free(con);
	
return (0);

	} else if (n < 0 && errno != EINTR) {
		m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: connection from %s"
		    " closed with error [%s]\n", con->name, strerror(errno));
		logerror("im_tcp_read");
		con->fd = -1;
		remove_fd_input(con->fd);
return (0);
	}
 
  /* 'n' bytes of data were successfully read from socket */
  {
    char *endmark;
    char *thisline;
    char *nextline;

		m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_read: read: %s [%s]",
		    con->name, im->im_buf);

		/* mark the end of the data received */
		endmark = im->im_buf + n;

    for( thisline = im->im_buf; ; thisline = nextline ) {

      /* seek the end of the current line */
      for( nextline = thisline; ; nextline++ ) {
  			/* a new line indicates a complete message */
  		  if (*nextline == '\n')  {
          *nextline = '\0';
          nextline++;
        if (nextline >= endmark) {
return (0);
        }
      break;
        }

				/* Data not ending with a new line is a partial message. 
         * Meaning, not all data has yet been received, save it for later.
         */
        if (nextline >= endmark) {
          int position;

          for( position = 0; (thisline + position) >= endmark; position++ ) {
            con->saveline[position] = thisline[position];
            if (position >= sizeof(con->saveline)) {
					    m_dprintf(MSYSLOG_SERIOUS,
					      "im_tcp_read: partial message too long [%d] [%s]\n",
					      "message having no embedded host name [%s]\n",
                sizeof(con->saveline), thisline);
            }
          }
				  con->saveline[sizeof(con->saveline) - 1] = '\0';
return (0);
        }

  			/* a carriage return indicates then end of a message */
	  	  if (*nextline == '\r')  {
          *nextline = '\0';
          nextline++;
      break;
        }
        /* change non printable chars to 'X', as you go */
			  if (! isprint((unsigned int) *nextline)) {
          *nextline = 'X';
      continue;
        }
      }

      /* append the current line with any prior partial line */
  		if (con->saveline[0] != '\0') {
  			strncat(con->saveline, thisline, sizeof(con->saveline) - 1);
  			con->saveline[sizeof(con->saveline) - 1] = '\0';
  			thisline = con->saveline;
  		}

      /* skip empty lines */
      if (*thisline == '\0') {
    continue;
      }

      /* process the message */

			if (c->flags & M_USEMSGHOST) {
        /* recover the hostname from the message */
				char host[90];
				int	n1, n2;

				/* extract hostname from the message line */
				if ((sscanf(thisline, "<%*d>%*3s %*i %*i:%*i:%*i %n%89s %n",
                    &n1, host, &n2) != 1 &&
				     sscanf(thisline, "%*3s %*i %*i:%*i:%*i %n%89s %n",
                    &n1, host, &n2) != 1 &&
				     sscanf(thisline, "%n%89s %n",
                    &n1, host, &n2) != 1)
				    || im->im_buf[n2] == '\0')
        {
					m_dprintf(MSYSLOG_INFORMATIVE,
					    "im_tcp_read: ignoring invalid "
					    "message having no embedded host name [%s]\n", thisline);
   continue;
				}

				/* remove host from message */
				while (im->im_buf[n2] != '\0')
					im->im_buf[n1++] = im->im_buf[n2++];
				im->im_buf[n1] = '\0';

				strncpy(ret->im_host, host, sizeof(ret->im_host) - 1);
				ret->im_host[sizeof(ret->im_host) - 1] = '\0';

			} else {
				/* get hostname from originating addr */

				strncpy(ret->im_host, con->name,
				    sizeof(ret->im_host) - 1);
				ret->im_host[sizeof(ret->im_host) - 1] = '\0';
			}

			if (c->flags & M_NOTFQDN) {
				char	*dot;

				if ((dot = strchr(ret->im_host, '.')) != NULL)
					*dot = '\0';
			}

      /* invoke rules on this message */
			printline(ret->im_host, thisline, strlen(thisline), im->im_flags);

      /* release prior partial line */
  		con->saveline[0] = '\0';
		}
	}
}

/*
 * im_tcp_close: close the connection
 *
 */

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
