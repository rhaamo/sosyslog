/*	$Id: ip_misc.c,v 1.26 2002/09/25 22:50:16 alejo Exp $	*/

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
#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_MACHINE_ENDIAN_H
# include <machine/endian.h>
#endif
#include <netdb.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "../modules.h"
#include "../syslogd.h"

/* recvfrom() and others like socklen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
#endif


#define TCP_KEEPALIVE 30	/* seconds to probe TCP connection */
#define MSYSLOG_MAX_TCP_CLIENTS 100
#define LISTENQ 35
#define M_NODNS		0x01

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
			snprintf(port, (unsigned) plen, "%u", ntohs(sin4->sin_port));
		return (1);
	}

#endif /* HAVE_GETNAMEINFO */

#ifdef HAVE_INET_NTOP
	switch (addr->sa_family) {
	case AF_INET: {
		struct sockaddr_in *caddr;

		caddr = (struct sockaddr_in *) addr;
		if (inet_ntop(AF_INET, &caddr->sin_addr, host, hlen) != NULL) {
# ifdef HAVE_INET_NTOHS
			if (ntohs(caddr->sin_port)) != 0)
				snprintf(port, (unsigned) plen, "%u",
				    ntohs(caddr->sin_port));
# endif /* HAVE_INET_NTOHS */
			return (1);
		}
		}
	case AF_INET6: {
		struct sockaddr_in6 *caddr;

		caddr = (struct sockaddr_in6 *) addr;
		if (inet_ntop(AF_INET6, &caddr->sin6_addr, host, hlen) != NULL) {
# ifdef HAVE_INET_NTOHS
			if (ntohs(caddr->sin6_port)) != 0)
				snprintf(port, (unsigned) plen, "%u",
				    ntohs(caddr->sin6_port));
# endif /* HAVE_INET_NTOHS */
			return (1);
		}
		}
	default:
		return (-1);
	}
#endif /* HAVE_INET_NTOP */

	m_dprintf(MSYSLOG_SERIOUS, "resolv_addr: error resolving "
	     "remote host name!\n");
	if (host)
		host[0] = '\0';
	if (port)
		port[0] = '\0';

	return (-1);
}

/*
 * resolv_addr_nodns: get a host name from a generic sockaddr struct
 *                    without resolving
 */

int
resolv_addr_nodns(struct sockaddr *addr, socklen_t addrlen,
    char *host, int hlen, char *port, int plen)
{
	struct sockaddr_in	*sin4;
#ifdef AF_INET6
	struct sockaddr_in6	*sin6;
#endif

	switch (addr->sa_family) {
	case AF_INET:
		inet_ntop(AF_INET, addr, host, hlen);
		sin4 = (struct sockaddr_in *) addr;
		snprintf(port, (unsigned) plen, "%u",
		    ntohs(sin4->sin_port));
		break;
#ifdef AF_INET6
	case AF_INET6:
		inet_ntop(AF_INET6, addr, host, hlen);
		sin6 = (struct sockaddr_in6 *) addr;
		snprintf(port, (unsigned) plen, "%u",
		    ntohs(sin6->sin6_port));
		break;
#endif
	default:
		return (-1);
	}

	host[hlen - 1] = '\0';
	port[plen - 1] = '\0';

	return (1);
}


/*
 * resolv_name: get a sockaddr address from host and port string
 *
 * NOTE: you must free the struct returned!
 */

struct sockaddr *
resolv_name(char *host, char *port, char *proto, socklen_t *salen)
{
	struct sockaddr *sa;
#ifdef HAVE_GETADDRINFO
	struct addrinfo hints, *res;
	int i;
#if defined(HAVE_INET_ADDR) && !defined(HAVE_INET_ATON)
	in_addr_t	addr;
#endif

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags = AI_PASSIVE;
	hints.ai_family = AF_UNSPEC;
	if (proto != NULL && strcmp(proto, "udp") == 0)
		hints.ai_socktype = SOCK_DGRAM;
	else
		hints.ai_socktype = SOCK_STREAM;


	if ( (i = getaddrinfo(host, port, &hints, &res)) != 0) {

		m_dprintf(MSYSLOG_SERIOUS, "resolv_name: error on address "
		    "to listen %s, %s: %s\n", host, port, gai_strerror(i));

		return (NULL);
	}

	sa = (struct sockaddr *) malloc(res->ai_addrlen);
	memcpy(sa, res->ai_addr, res->ai_addrlen);
	*salen = res->ai_addrlen;
	freeaddrinfo(res);

#else	/* we are on an extremely outdated and ugly api */
	struct sockaddr_in	*sin;
	struct hostent *hp;
	struct servent *se;
	short portnum;

	if (port != NULL && isdigit((int) *port)) {

		portnum = strtol(port, NULL, 10);

	} else if ((se = getservbyname(port == NULL? "syslog" : port,
	    proto == NULL? "tcp" : proto)) != NULL) {

		portnum = se->s_port;

	} else
		portnum = 514;

#ifndef	WORDS_BIGENDIAN
	portnum = htons(portnum);
#endif

	sin = (struct sockaddr_in *) malloc(sizeof(*sin));
#ifdef HAVE_SOCKADDR_SA_LEN
	sin->sin_len = sizeof(*sin);
#endif
	sin->sin_family = AF_INET;
	sin->sin_port = portnum;
	memset(&sin->sin_addr, 0, sizeof(sin->sin_addr));

	if (host == NULL ||
#ifdef HAVE_INET_ATON

inet_aton(host, &sin->sin_addr) == 1

#elif defined(HAVE_INET_ADDR)

        (addr = inet_addr(host)) > 0 && 
                memcpy(&sin->sin_addr, &addr, sizeof(sin->sin_addr)) != NULL

#else
# error NEED RESOLVING FUNCTION, PLEASE REPORT
#endif
	    ) {

		return ((struct sockaddr *) sin);

	} else if ((hp = gethostbyname(host)) == NULL) {

		m_dprintf(MSYSLOG_SERIOUS, "resolv_name: error "
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
connect_tcp(char *host, char *port) {
	int fd, n;
	struct sockaddr *sa;
	socklen_t salen;

	if ( (sa = resolv_name(host, port, "tcp", &salen)) == NULL)
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
listen_tcp(char *host, char *port, socklen_t *addrlenp) {
	int fd, n, r;
	struct sockaddr *sa;

	if ( (sa = resolv_name(host, port, "tcp", addrlenp)) == NULL)
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

/*
 * sock_udp: create a generic socket for sending udp packets
 *
 * NOTE: you must free the struct returned!
 */

int
sock_udp(char *host, char *port, void **addr, int *addrlen)
{
	struct sockaddr	*sa;
	socklen_t	 salen;

	if ((sa = resolv_name(host, port, "udp", &salen)) == NULL)
		return (-1);

	/* pass struct sockaddr if requested */
	if (addrlen != NULL)
		*addrlen = salen;
	if (addr != NULL)
		*addr = sa;
	else
		free(sa);

	return (socket(sa->sa_family, SOCK_DGRAM, 0));
}

/*
 * udp_send:	send an UDP packet
 */

int
udp_send(int fd, char *msg, int mlen, void *addr, int addrlen)
{
	return (sendto(fd, msg, mlen, 0, (struct sockaddr *) addr, addrlen));
}


/*
 * udp_recv:	receive an UDP packet
 */

int
udp_recv(int fd, char *msg, int mlen, char *host, int hlen,
    char *port, int plen, int flags)
{
	struct sockaddr *sa;
	socklen_t	 salen;
	int		 rlen;

	salen = sizeof (struct
#ifdef AF_INET6
	    sockaddr_in6
#else
	    sockaddr_in
#endif
	    );

	if ((sa = (struct sockaddr *) malloc(salen)) == NULL) 
		return (-1);

	if ((rlen = recvfrom(fd, msg, mlen - 1, 0, sa, &salen)) < 1) {

		free(sa);
		return (-1);
	}

	msg[rlen] = '\0';

	if (flags & M_NODNS)
		resolv_addr_nodns(sa, salen, host, hlen, port, plen);
	else
		resolv_addr(sa, salen, host, hlen, port, plen);

	free(sa);
	return (rlen);
}

/*
 * resolv_domain: get a domain for a name, used to get local domain
 *
 */

int
resolv_domain(char *buf, int buflen, char *host)
{
	struct	sockaddr	*sa;
	socklen_t		salen;

	if ((sa = resolv_name(host, NULL, NULL, &salen)) == NULL ||
	    resolv_addr(sa, salen, buf, buflen, NULL, 0) == -1) {

		*buf = '\0';
	}

	return (1);
}

#if 0
int
resolv_addr_cached(struct name_cache *cache, char *host, char *port,
    char *proto, socklen_t *salen)
{
	/* walk through the cache */
	/* if found, return pointer to cache */
	/* else, resolv */
	/* if cache full, remove first entry(ies) (as many as needed)
	 * move forwards others (since this name could be longer than one)
	 * and add this name at the end */
	return (-1);
}
#endif
