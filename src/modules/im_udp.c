/*	$CoreSDI: im_udp.c,v 1.67 2001/09/20 01:11:42 alejo Exp $	*/

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

/* prototypes */
struct sockaddr *resolv_name(char *, char *, char *, socklen_t *);


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
		m_dprintf(MSYSLOG_SERIOUS, "im_udp: arg is null\n");
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
		char	host[90];
		int	n1, n2;

		n1 = 0;
		n2 = 0;
		/* extract hostname from message */
		if ((sscanf(ret->im_msg, "<%*d>%*3s %*i %*i:%*i:%*i %n%89s "
		    "%n%*s", &n1, host, &n2) != 1 &&
		    sscanf(ret->im_msg, "%*3s %*i %*i:%*i:%*i %n%89s %n%*s",
		    &n1, host, &n2) != 1 &&
		    sscanf(ret->im_msg, "%n%89s %n%*s", &n1, host,
		    &n2) != 1) ||
		    ret->im_msg[n2] == '\0') {
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

		strncpy(ret->im_host, host, sizeof(ret->im_host) - 1);
		ret->im_host[sizeof(ret->im_host) - 1] = '\0';

	} else {
		struct hostent *hent;

		hent = gethostbyaddr((char *) &frominet.sin_addr,
		    sizeof(frominet.sin_addr), frominet.sin_family);
		if (hent) {
			strncpy(ret->im_host, hent->h_name,
			    sizeof(ret->im_host) - 1);
		} else {
			strncpy(ret->im_host, inet_ntoa(frominet.sin_addr),
			    sizeof(ret->im_host) - 1);
		}
	}

	ret->im_host[sizeof(ret->im_host) - 1] = '\0';

	if (c->flags & M_NOTFQDN) {
		char     *dot;

		if ((dot = strchr(ret->im_host, '.')) != NULL)
			*dot = '\0';
	}

	return (1);
}

/*
 * initialize udp input
 *
 */

int
im_udp_init(struct i_module *I, char **argv, int argc)
{
	struct sockaddr		*sa;
	struct im_udp_ctx	*c;
	char			*host, *port;
	int			ch;
	socklen_t		salen;

	if ( (I->im_ctx = calloc(1, sizeof(struct im_udp_ctx))) == NULL)
		return (-1);

	c = (struct im_udp_ctx *) I->im_ctx;

	port = "syslog";
	host = "0.0.0.0";

	/* parse command line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "h:p:aq")) != -1) {
		switch (ch) {
		case 'h':
			/* get addr to bind */
			host = optarg;
			break;
		case 'p':
			/* get remote host port */
			port = optarg;
			break;
		case 'a':
			c->flags |= M_USEMSGHOST;
			break;
		case 'q':
			/* dont use domain in hostname (FQDN) */
			c->flags |= M_NOTFQDN;
			break;
		default:
			m_dprintf(MSYSLOG_SERIOUS, "om_udp_init: parsing error"
			    " [%c]\n", ch);
			free(c);
			return (-1);
		}
	}

	I->im_fd = socket(AF_INET, SOCK_DGRAM, 0);

	if ((sa = resolv_name(host, port, "udp", &salen)) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_udp_init: error resolving host"
		    "[%s] and port [%s]", host, port);
		free(c);
		return (-1);
	}

	if (bind(I->im_fd, sa, salen) < 0) {
		m_dprintf(MSYSLOG_SERIOUS, "om_udp_init: error binding to host"
		    "[%s] and port [%s]", host, port);
		free(c);
		return (-1);
	}

	I->im_path = NULL;
	if (finet < 0) {
		/* finet not in use */
		finet = I->im_fd;
		DaemonFlags |= SYSLOGD_INET_IN_USE;
		DaemonFlags |= SYSLOGD_INET_READ;
	}

	add_fd_input(I->im_fd , I);

	m_dprintf(MSYSLOG_INFORMATIVE, "im_udp: running\n");
	return (1);
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

