/*	$CoreSDI: ip_misc.c,v 1.2 2001/03/06 18:21:05 alejo Exp $	*/

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
 * ip_misc -- basic TCP/UDP/IP functions
 *      
 * Author: Alejo Sanchez for Core SDI S.A.
 * 
 */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <syslog.h>

#include "../modules.h"
#include "../syslogd.h"

/* recvfrom() and others like soclen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
# warning using socklen_t as int
#endif


#define TCP_KEEPALIVE 30	/* seconds to probe TCP connection */
#define MSYSLOG_MAX_TCP_CLIENTS 100
#define LISTENQ 35

/*
 * resolv_addr: get a  host name from a generic sockaddr structure
 *
 * NOTE: you must free the string returned!
 */

char *
resolv_addr(struct sockaddr *addr, socklen_t addrlen)
{
	struct hostent	*hp;
	struct sockaddr_in *sin4;
	char host[512];
	int n;

	host[0] = '\0';

#ifdef HAVE_GETNAMEINFO

	n = getnameinfo((struct sockaddr *) addr, addrlen,
	    host, sizeof(host) - 1, NULL, 0, 0);
#else /* old socket API */

	hp = NULL;
	if (addr->sa_family == AF_INET) {
		sin4 = (struct sockaddr_in *) addr;
		hp = gethostbyaddr((char *) &sin4->sin_addr,
		    sizeof(sin4->sin_addr), sin4->sin_family);
	}

	if (hp == NULL) {
		n = -1; /* error */
	} else {
		strncpy(host, hp->h_name, sizeof(host) - 1);
		host[sizeof(host) - 1] = '\0';
		n = 0;
	}
#endif /* HAVE_GETNAMEINFO */

	if (n != 0) {
	       	dprintf(DPRINTF_INFORMATIVE)("im_tcp: error resolving "
		    "remote host name! [%d]\n", n);
		return (NULL);
	}

	return (strdup(host));
}


/*
 * resolv_name: get a  host name from a generic sockaddr structure
 *
 * NOTE: you must free the struct returned!
 */

struct sockaddr *
resolv_name(const char *host, const char *port, int *salen)
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

		dprintf(DPRINTF_INFORMATIVE)("resolv_name: error on address "
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

	if ( port && ((se = getservbyname(port, "tcp")) != NULL) )
		portnum = se->s_port;
	else
		portnum = 0;

	if ( (hp = gethostbyname(host)) == NULL ) {
		dprintf(DPRINTF_SERIOUS)("resolv_name: error resolving "
		    "host address %s, %s\n", host, port);
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
		sin4->sin_port = 0; /* this should be specified later */
		memcpy(&sin4->sin_addr, *hp->h_addr_list,
		    sizeof(struct in_addr));
		sa = (struct sockaddr *) sin4;
	}
#ifdef AF_INET6
	else if (hp->h_addrtype == AF_INET) {
		struct sockaddr_in6 *sin6;

		sin6 = (struct sockaddr_in6 *)
		    malloc(sizeof(struct sockaddr_in6));
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
	int fd, n;
	struct sockaddr *sa;

	if ( (sa = resolv_name(host, port, addrlenp)) == NULL)
		return (-1);

	n = TCP_KEEPALIVE;

	if ( (fd = socket(sa->sa_family, SOCK_STREAM, 0)) > -1 )

		if ( (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &n,
		    sizeof(n)) != 0) || (bind(fd, sa, *addrlenp) != 0)
		    || (listen(fd, LISTENQ) != 0) ) {
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
accept_tcp(int fd, const socklen_t addrlen, char **name) {
	struct sockaddr *connsa;
	socklen_t connsalen;
	int connfd;

	if ( addrlen < 1 || (connsa = (struct sockaddr *)
	    calloc(1, addrlen)) == NULL )
		return (-1);

	connsalen = addrlen;

	*name = NULL;
	if ( (connfd = accept(fd, connsa, &connsalen)) != -1)
		*name = resolv_addr(connsa, connsalen);

	free(connsa);

	return (connfd);
}
