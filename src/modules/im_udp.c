/*	$CoreSDI: im_udp.c,v 1.41 2000/08/25 22:37:51 alejo Exp $	*/

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
 * im_udp -- input from INET using UDP
 *      
 * Author: Alejo Sanchez for Core SDI S.A.
 *         from syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../modules.h"
#include "../syslogd.h"

/* standard input module header variables in context */
struct im_udp_ctx {
	short	flags;
#define M_FLAG_INITIALIZED 0x1
#define M_FLAG_ERROR 0x2
	int	size;
	int	fd;
};

/*
 * get messge
 *
 */

int
im_udp_getLog(struct imodule *im, struct im_msg *ret) {
	struct sockaddr_in frominet;
	struct hostent *hent;
	int slen;

	if (ret == NULL) {
		dprintf("im_udp: arg is null\n");
		return (-1);
	}

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(frominet);
	ret->im_len = recvfrom(finet, ret->im_msg, MAXLINE, 0,
		(struct sockaddr *)&frominet, (socklen_t *)&slen);
	if (ret->im_len > 0) {
		ret->im_msg[ret->im_len] = '\0';
		hent = gethostbyaddr((char *) &frominet.sin_addr,
		    sizeof(frominet.sin_addr), frominet.sin_family);
		if (hent)
			strncpy(ret->im_host, hent->h_name,
			    sizeof(ret->im_host));
		else
			strncpy(ret->im_host, inet_ntoa(frominet.sin_addr),
			    sizeof(ret->im_host));
	} else if (ret->im_len < 0 && errno != EINTR)
		logerror("recvfrom inet");

	return(1);
}

/*
 * initialize udp input
 *
 */

int
im_udp_init(struct i_module *I, char **argv, int argc) {
	struct sockaddr_in sin;
	struct servent *sp;

        if (argc == 2 && (argv == NULL || argv[1] == NULL)) {
        	dprintf("im_udp: error on params!\n");
        	return(-1);
        }

        if (finet > -1) {
		dprintf("im_udp_init: already opened!\n");
		return(-1);
        }
        finet = socket(AF_INET, SOCK_DGRAM, 0);

	sp = getservbyname("syslog", "udp");
	if (sp == NULL) {
		errno = 0;
		logerror("syslog/udp: unknown service");
		die(0);
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (argc == 2  && argv[1])
		sin.sin_port = htons(atoi(argv[1]));
	else
		sin.sin_port = LogPort = sp->s_port;

	if (bind(finet, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		logerror("bind");
		if (!Debug)
		die(0);
	} else {
		InetInuse = 1;
	}

        I->im_path = NULL;
        I->im_fd   = finet;
        return(1);
}
