/*	$CoreSDI: im_unix.c,v 1.35 2000/10/31 19:42:14 alejo Exp $	*/

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
 * im_unix -- classic behaviour module for BSD like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         from syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include "../modules.h"
#include "../syslogd.h"

/*
 * get message
 *
 */

int
im_unix_getLog(struct i_module *im, struct im_msg  *ret)
{
	struct sockaddr_un fromunix;
	int slen;

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(fromunix);
	ret->im_len = recvfrom(im->im_fd, ret->im_msg, MAXLINE, 0,
	    (struct sockaddr *)&fromunix, (socklen_t *)&slen);
	if (ret->im_len > 0) {
		ret->im_msg[ret->im_len] = '\0';
		strncpy(ret->im_host, LocalHostName, sizeof(ret->im_host));
	} else if (ret->im_len < 0 && errno != EINTR) {
		logerror("recvfrom unix");
		ret->im_msg[0] = '\0';
		ret->im_len = 0;
		ret->im_host[0] = '\0';
		return (-1);
	}

	return (1);
}

/*
 * initialize unix input
 *
 */

int
im_unix_init(struct i_module *I, char **argv, int argc)
{
	struct sockaddr_un sunx;

	dprintf ("\nim_unix_init...\n");

	if (I == NULL || argv == NULL || argc != 2)
		return (-1);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	(void) unlink(argv[1]);

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void)strncpy(sunx.sun_path, argv[1], sizeof(sunx.sun_path));
	I->im_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (I->im_fd < 0 ||
	    bind(I->im_fd, (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(argv[1], 0666) < 0) {
		(void) snprintf(I->im_buf, sizeof(I->im_buf),
		    "cannot create %s", argv[1]);
		logerror(I->im_buf);
		dprintf("cannot create %s (%d)\n", argv[1], errno);
		return (-1);
	}
	I->im_path = argv[1];
	return (1);
}

int
im_unix_close( struct i_module *im)
{
	close(im->im_fd);

	if (im->im_path)
		unlink(im->im_path);

	return (0);
}


#if 0
/*
 * get user credentials
 */

ssize_t
getcred(fd, fcred)
	int	fd;
	struct f_cred *fcred;
{
	struct msg_hdr	msg;
	struct iovec	iov[1];
	ssize_t		n;
	union {
	    struct cmsghdr cm;
	    char   control[CMSGSPACE(sizeof(struct fcred))]
	} control_un;
	struct cmsghdr  *cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	msg.msg_name = NULL;
	msg.msg_nmelen = 0;

	iov[0].io_base = NULL;
	iov[0].io_len  = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if ((n = recvmsg(fd, &msg, 0)) < 0)k
	
}

#endif

