/*	$CoreSDI: im_udp.c,v 1.62 2001/06/13 22:32:00 alejo Exp $	*/

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
 *         from syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/param.h>

#include <netinet/in.h>
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

/*
 * get messge
 *
 */

int
im_udp_read(struct i_module *im, int infd, struct im_msg *ret)
{
	struct im_udp_ctx	*c;
	struct sockaddr_in frominet;
	int slen;

	if (ret == NULL) {
		dprintf(MSYSLOG_SERIOUS, "im_udp: arg is null\n");
		return (-1);
	}

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(frominet);
	if ((ret->im_len = recvfrom(im->im_fd, ret->im_msg, ret->im_mlen - 1, 0,
	    (struct sockaddr *)&frominet, (socklen_t *)&slen)) < 1) {
		if (ret->im_len < 0 && errno != EINTR)
			logerror("recvfrom inet");
		return (1);
	}

	ret->im_msg[ret->im_len] = '\0';

	c = (struct im_udp_ctx *) im->im_ctx;

	if (c->flags & M_USEMSGHOST) {
		int     n1, n2;       

		/* extract hostname from message */
#if SIZEOF_MAXHOSTNAMELEN < 89
#error  Change here buffer reads to match HOSTSIZE
#endif
		if (sscanf(ret->im_msg, "<%*d>%*15c %n%90s %n", &n1,
		    ret->im_host, &n2) != 3)	      
			return (0);

		/* remove host from message */
		while (ret->im_msg[n2] != '\0')
			       ret->im_msg[n1++] = ret->im_msg[n2++];
	} else {
		struct hostent *hent;

		hent = gethostbyaddr((char *) &frominet.sin_addr,
		    sizeof(frominet.sin_addr), frominet.sin_family);
		if (hent) {
			strncpy(ret->im_host, hent->h_name,
			    sizeof(ret->im_host));
		} else {
			strncpy(ret->im_host, inet_ntoa(frominet.sin_addr),
			    sizeof(ret->im_host));
		}
	}

	ret->im_host[sizeof(ret->im_host) - 1] = '\0';

	return (1);
}

/*
 * initialize udp input
 *
 */

int
im_udp_init(struct i_module *I, char **argv, int argc)
{
	struct sockaddr_in	sin;
	struct im_udp_ctx	*c;
	struct servent		*sp;
	char			*port;
	int			ch;

	if ( (I->im_ctx = calloc(1, sizeof(struct im_udp_ctx))) == NULL)
		return (-1);

	c = (struct im_udp_ctx *) I->im_ctx;

	port = NULL;

	/* parse command line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "h:p:a")) != -1) {
		switch (ch) {
		case 'h':
			/* get addr to bind */
			break;
		case 'p':
			/* get remote host port */
			port = optarg;
			break;
		case 'a':
			c->flags |= M_USEMSGHOST;
			break;
		default:
			dprintf(MSYSLOG_SERIOUS, "om_udp_init: parsing error"
			    " [%c]\n", ch);
			goto init_error;
		}
	}

        I->im_fd = socket(AF_INET, SOCK_DGRAM, 0);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	if (port != NULL) {
		sin.sin_port = htons(atoi(argv[1]));
	} else {
		if ((sp = getservbyname("syslog", "udp")) == NULL) {
			errno = 0;
			logerror("syslog/udp: unknown service");
			goto init_error;
		}
		sin.sin_port = sp->s_port;
	}

	LogPort = sin.sin_port;

	if (bind(I->im_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		logerror("im_udp_init: bind");
		goto init_error;
	}

        I->im_path = NULL;
        if (finet < 0) {
		/* finet not in use */
        	finet = I->im_fd;
		DaemonFlags |= SYSLOGD_INET_IN_USE;
		DaemonFlags |= SYSLOGD_INET_READ;
        }

	add_fd_input(I->im_fd , I);

        dprintf(MSYSLOG_INFORMATIVE, "im_udp: running\n");
        return (1);

init_error:
	free(c);
	I->im_ctx = NULL;
	return (-1);
}

int
im_udp_close(struct i_module *im)
{
        if (finet == im->im_fd) {
        	finet = -1;
		DaemonFlags &= ~SYSLOGD_INET_IN_USE;
		DaemonFlags &= ~SYSLOGD_INET_READ;
        }

       	close(im->im_fd);
 
        return (0);
}

