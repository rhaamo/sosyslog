 /*	$Id: im_udp.c,v 1.76 2002/09/17 06:30:41 alejo Exp $	*/
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
 * im_udp -- input from INET using UDP
 *
 * Author: Alejo Sanchez for Core SDI S.A.
 *	from syslogd.c by Eric Allman and Ralph Campbell
 *
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "../modules.h"
#include "../syslogd.h"

/* recvfrom() and others like socklen_t, Irix doesn't provide it */
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
#endif

struct im_udp_ctx {
	int	flags;
};

#define M_USEMSGHOST	0x01
#define M_NOTFQDN	0x02
#define M_CACHENAMES	0x04

/* prototypes */
struct sockaddr *resolv_name(char *, char *, char *, socklen_t *);

/*
 * initialize udp input
 *
 *  this module takes a host argument (ie. 0.0.0.0, 0::0, server.example.com)
 *  and a port/service ('syslog' or numerical)
 */

int
im_udp_init(struct i_module *I, char **argv, int argc)
{
	struct im_udp_ctx	*c;
	char   *host, *port;
	int    ch, argcnt;

  m_dprintf(MSYSLOG_INFORMATIVE, "im_tcp_init: entering\n");

	if ( (I->im_ctx = calloc(1, sizeof(struct im_udp_ctx))) == NULL) {
     m_dprintf(MSYSLOG_SERIOUS, "im_udp_init: cannot alloc memory");
return (-1);
  }

	c = (struct im_udp_ctx *) I->im_ctx;

	host = "0.0.0.0";
	port = "syslog";

	/* parse args (skip module name) */
	for (argcnt = 1; (ch = getxopt(argc, argv, "h!host: p!port: "
	    "a!addhost q!nofqdn c!cachenames", &argcnt)) != -1; argcnt++) {

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
			/* dont use domain in hostname (FQDN) */
			c->flags |= M_NOTFQDN;
			break;
		case 'c':
			/* use cached hostnames */
			c->flags |= M_CACHENAMES;
			break;
		default:
			m_dprintf(MSYSLOG_SERIOUS, "im_udp_init: parsing error [%c]\n", ch);
			free(c);
return (-1);
		}
	}

  { /* get the udp socket */
	  struct sockaddr		*sa;
  	socklen_t		salen;

  	I->im_fd = socket(AF_INET, SOCK_DGRAM, 0);

  	if ((sa = resolv_name(host, port, "udp", &salen)) == NULL) {
  		m_dprintf(MSYSLOG_SERIOUS, "im_udp_init: error resolving host"
  		    "[%s] and port [%s]", host, port);
  		free(c);
return (-1);
  	}

  	if (bind(I->im_fd, sa, salen) < 0) {
  		m_dprintf(MSYSLOG_SERIOUS, "im_udp_init: error binding to host"
  		    "[%s] and port [%s]", host, port);
  		free(c);
return (-1);
  	}
 	}

	watch_fd_input('p', I->im_fd , I);
	m_dprintf(MSYSLOG_INFORMATIVE, "im_udp: running\n");
return (1);
}


/*
 * im_tcp_read: accept a connection and add it to the queue
 * connections and modules are read in a round-robin so partial lines
 * must persist across calls to the im_read functions for the
 * various modules.
 */

int
im_udp_read(struct i_module *im, int infd, struct im_msg *ret)
{
	struct im_udp_ctx	*c;
	struct sockaddr_in	 frominet;
	char   *p;
	int    slen;

  m_dprintf(MSYSLOG_INFORMATIVE, "im_udp_read: entering...\n");

	if (ret == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "im_udp: arg is null\n");
return (-1);
	}

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(frominet);
	if ((ret->im_len = recvfrom(im->im_fd, ret->im_msg,
	    sizeof(ret->im_msg) - 1, 0, (struct sockaddr *)&frominet,
	    (socklen_t *)&slen)) < 1) {
		if (ret->im_len < 0 && errno != EINTR)
			logerror("recvfrom inet");
return (1);
	}

	ret->im_msg[ret->im_len] = '\0';

	c = (struct im_udp_ctx *) im->im_ctx;

	/* change non printable chars to X, just in case */
	for(p = ret->im_msg; *p != '\0'; p++)
		if (!isprint((unsigned int) *p) && *p != '\n')
			*p = 'X';

	if (c->flags & M_USEMSGHOST) {
		/* extract hostname from message */
		char  host[90];
		int   n1 = 0;
		int   n2 = 0;

		if ((sscanf(ret->im_msg, "<%*d>%*3s %*i %*i:%*i:%*i %n%89s %n%*s",
                &n1, host, &n2) != 1 &&
		     sscanf(ret->im_msg, "%*3s %*i %*i:%*i:%*i %n%89s %n%*s",
		            &n1, host, &n2) != 1 &&
		     sscanf(ret->im_msg, "%n%89s %n%*s", &n1, host, &n2) != 1)
       ||
		    ret->im_msg[n2] == '\0')
    {
			m_dprintf(MSYSLOG_INFORMATIVE, "im_udp_read: skipped"
			    " invalid message [%s]\n", ret->im_msg);
return (0);
		}

		if (ret->im_msg[n2] == '\0')
return (0);

		/* remove host from message */
		while (ret->im_msg[n2] != '\0')
			ret->im_msg[n1++] = ret->im_msg[n2++];
		ret->im_msg[n1] = '\0';

		strncat(ret->im_host, host, sizeof(ret->im_host));

    /* strip domain from hostname */
    /* There is no assurance that the hostname is not an ip address
     * in which case stripping off the domain would be inappropriate.
     *
	  if (c->flags & M_NOTFQDN) { 
	    char   *dot;
  		if ((dot = strchr(ret->im_host, '.')) != NULL) *dot = '\0';
   	}
    */
	}
  else {
		/* extract host ip address from ip header, and attempt to look up the name */
		struct hostent *hent;

		hent = gethostbyaddr((char *) &frominet.sin_addr,
		    sizeof(frominet.sin_addr), frominet.sin_family);
		if (hent) {
			strncpy(ret->im_host, hent->h_name, sizeof(ret->im_host));
      /* strip domain from hostname */
	    if (c->flags & M_NOTFQDN) { 
		    char   *dot;
    		if ((dot = strchr(ret->im_host, '.')) != NULL) *dot = '\0';
    	}
		}
    else {
			strncpy(ret->im_host, inet_ntoa(frominet.sin_addr), sizeof(ret->im_host));
		}
	}

	ret->im_host[sizeof(ret->im_host) - 1] = '\0';

return (1);
}

int
im_udp_close(struct i_module *im)
{

	close(im->im_fd);

return (0);
}
