/*	$CoreSDI: ip_misc.c,v 1.15 2001/06/14 01:24:09 alejo Exp $	*/

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
 * ip_misc -- basic TCP/UDP/IP functions
 *      
 * Author: Alejo Sanchez for Core SDI S.A.
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
#include <sys/uio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <inttypes.h>
#include <syslog.h>

#include "../modules.h"
#include "../syslogd.h"

/* recvfrom() and others like socklen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
#endif


#define TCP_KEEPALIVE 30	/* seconds to probe TCP connection */
#define MSYSLOG_MAX_TCP_CLIENTS 100
#define LISTENQ 35

/*
 * resolv_addr: get a  host name from a generic sockaddr structure
 *
 */

int
resolv_addr(struct sockaddr *addr, socklen_t addrlen, char *host, int hlen,
    char *port, int plen)
{
#ifdef HAVE_GETNAMEINFO

	if (getnameinfo((struct sockaddr *) addr, addrlen,
	    host, hlen - 1, port, plen, 0) == 0)
		return (1);

#else /* no HAVE_GETNAMEINFO, old socket API */
	struct hostent	*hp;
	struct sockaddr_in *sin4;

	sin4 = (struct sockaddr_in *) addr;

	hp = gethostbyaddr((char *) &sin4->sin_addr,
	    sizeof(sin4->sin_addr), sin4->sin_family);

	if (hp) {
		strncpy(host, hp->h_name, (unsigned) hlen - 1);
		host[hlen] = '\0';
		if (port)
			snprintf(port, (unsigned) plen, "%u", sin4->sin_port);
		return (1);
	}

#endif /* HAVE_GETNAMEINFO */

	dprintf(MSYSLOG_INFORMATIVE, "ip_misc: error resolving "
	     "remote host name!\n");
	if (host)
		host[0] = '\0';
	if (port)
		port[0] = '\0';

	return (-1);
}


/*
 * resolv_name: get a  host name from a generic sockaddr structure
 *
 * NOTE: you must free the struct returned!
 */

struct sockaddr *
resolv_name(const char *host, const char *port, socklen_t *salen)
{
	struct sockaddr *sa;
#ifdef HAVE_GETADDRINFO
	struct addrinfo hints, *res;
	int i;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ( (i = getaddrinfo(host, port, &hints, &res)) != 0) {

		dprintf(MSYSLOG_INFORMATIVE, "resolv_name: error on address "
		    "to listen %s, %s: %s\n", host, port, gai_strerror(i));

		return (NULL);
	}

	sa = (struct sockaddr *) malloc(res->ai_addrlen);
	memcpy(sa, res->ai_addr, res->ai_addrlen);
	*salen = res->ai_addrlen;
	freeaddrinfo(res);

#else	/* we are on an extremely outdated and ugly api */
	struct hostent *hp;
	struct servent *se;
	short portnum;

	if (port)
		portnum = strtol(port, NULL, 10);
	else if ((se = getservbyname(port, "tcp")) != NULL)
		portnum = se->s_port;
	else
		portnum = 0;

#ifndef	WORDS_BIGENDIAN
	portnum = htons(portnum);
#endif

	if (host == NULL) {
		struct sockaddr_in *sin;

		sin = (struct sockaddr_in *) malloc(sizeof(*sin));
#ifdef HAVE_SOCKADDR_SA_LEN
		sin->sin_len = sizeof(*sin);
#endif
		sin->sin_family = AF_INET;
		sin->sin_port = portnum;
		memset(&sin->sin_addr, 0, sizeof(sin->sin_addr));
		return ((struct sockaddr *) sin);
	} else if ((hp = gethostbyname(host)) == NULL) {
		dprintf(MSYSLOG_SERIOUS, "resolv_name: error "
		    "resolving host address %s, %s\n", host, port);
		return (NULL);
	}

	if (hp->h_addrtype == AF_INET) {
		struct sockaddr_in *sin4;

		sin4 = (struct sockaddr_in *)
		    malloc(sizeof(struct sockaddr_in));
		*salen = sizeof(struct sockaddr_in);
#ifdef HAVE_SOCKADDR_SA_LEN
		sin4->sin_len = sizeof(struct sockaddr_in);
#endif
		sin4->sin_port = portnum;
		sin4->sin_family = AF_INET;
		memcpy(&sin4->sin_addr, *hp->h_addr_list,
		    sizeof(struct in_addr));
		sa = (struct sockaddr *) sin4;
	}
#ifdef AF_INET6
	else if (hp->h_addrtype == AF_INET) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)
		    malloc(sizeof(*sin6));
		*salen = sizeof(struct sockaddr_in);
# ifdef HAVE_SOCKADDR_SA_LEN
		sin6->sin6_len = sizeof(struct sockaddr_in6);
# endif
		sin6->sin6_port = portnum;
		sin6->sin6_family = AF_INET6;
		sin6->sin6_port = 0; /* this should be specified later */
		memcpy(&sin6->sin6_addr, *hp->h_addr_list,
		    sizeof(struct in6_addr));
		sa = (struct sockaddr *) sin6;
	}
#endif /* AF_INET6 */
	else  /* no match ?!? */
		sa = NULL;

#endif /* HAVE_GETADDRINFO */

	return (sa);
}


/*
 * connect_tcp: connect to a remote host/port
 *              return the file descriptor
 */

int
connect_tcp(const char *host, const char *port) {
	int fd, n;
	struct sockaddr *sa;
	socklen_t salen;

	if ( (sa = resolv_name(host, port, &salen)) == NULL)
		return (-1);

	n = TCP_KEEPALIVE;

	if (sa->sa_family != AF_INET
#ifdef AF_INET6
	    && sa->sa_family != AF_INET6
#endif
	    ) {
		free(sa);
		return (-1);
	}

	if ( (fd = socket(sa->sa_family, SOCK_STREAM, 0)) > -1 )

		if ( (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &n,
		    sizeof(n)) != 0) || (connect(fd, sa, salen) != 0) ) {
			close(fd); /* couldn't set option or connect */
			fd = -1;
		}

	free(sa);
	return (fd);
}


/*
 * listen_tcp: listen on a host/port
 *             return the file descriptor
 */

int
listen_tcp(const char *host, const char *port, socklen_t *addrlenp) {
	int fd, n, r;
	struct sockaddr *sa;

	if ( (sa = resolv_name(host, port, addrlenp)) == NULL)
		return (-1);

	n = TCP_KEEPALIVE;
	r = 1;

	if ( (fd = socket(sa->sa_family, SOCK_STREAM, 0)) > -1 )

		if ( (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &n,
		    sizeof(n)) != 0) || (setsockopt(fd, SOL_SOCKET,
		    SO_REUSEADDR, &r, sizeof(r)) != 0) ||
		    (bind(fd, sa, *addrlenp) != 0) ||
		    (listen(fd, LISTENQ) != 0) ) {
			close(fd); /* couldn't set option or connect */
			fd = -1;
		}

	free(sa);

	return (fd);
}



/*
 * accept_tcp: accept a listen file descriptor
 *             return the connected file descriptor
 */

int
accept_tcp(int fd, socklen_t addrlen, char *host, int hlen, char *port,
    int plen)
{
	struct sockaddr *connsa;
	int connfd;

	if (addrlen < 1 || (connsa = (struct sockaddr *)
	    calloc(1, addrlen)) == NULL )
		return (-1);

	if ((connfd = accept(fd, connsa, (socklen_t *) &addrlen)) != -1)
		(void) resolv_addr(connsa, addrlen, host, hlen, port, plen);

	free(connsa);

	return (connfd);
}
