/*	$CoreSDI: im_udp.c,v 1.52 2000/12/14 00:16:44 alejo Exp $	*/

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

#include "../../config.h"

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

#include "../modules.h"
#include "../syslogd.h"

/* recvfrom() and others like soclen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
# warning using socklen_t as int
#endif


/*
 * get messge
 *
 */

int
im_udp_read(struct imodule *im, struct im_msg *ret)
{
	struct sockaddr_in frominet;
	struct hostent *hent;
	int slen;

	if (ret == NULL) {
		dprintf(DPRINTF_SERIOUS)("im_udp: arg is null\n");
		return (-1);
	}

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(frominet);
	ret->im_len = recvfrom(finet, ret->im_msg, ret->im_mlen, 0,
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

	return (1);
}

/*
 * initialize udp input
 *
 */

int
im_udp_init(struct i_module *I, char **argv, int argc)
{
	struct sockaddr_in sin;
	struct servent *sp;

        if ((argc != 1 && argc != 2) || (argc == 2 &&
	    (argv == NULL || argv[1] == NULL))) {
        	dprintf(DPRINTF_SERIOUS)("im_udp: error on params!\n");
        	return (-1);
        }

        I->im_fd = socket(AF_INET, SOCK_DGRAM, 0);

	sp = getservbyname("syslog", "udp");
	if (sp == NULL) {
		errno = 0;
		logerror("syslog/udp: unknown service");
		return (-1);
	}
	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (argc == 2  && argv[1])
		sin.sin_port = htons(atoi(argv[1]));
	else
		sin.sin_port = LogPort = sp->s_port;

	if (bind(I->im_fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		logerror("im_udp_init: bind");
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

        dprintf(DPRINTF_INFORMATIVE)("im_udp: running\n");
        return (1);
}

int
im_udp_close(struct i_module *im)
{
        if (finet == im->im_fd) {
        	close(im->im_fd);
        	finet = im->im_fd;
		DaemonFlags &= ~SYSLOGD_INET_IN_USE;
		DaemonFlags &= ~SYSLOGD_INET_READ;
        } else {
        	close(im->im_fd);
        }
 
        return (0);
}

