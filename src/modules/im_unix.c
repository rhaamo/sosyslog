/*	$Id: im_unix.c,v 1.56 2002/09/17 05:20:28 alejo Exp $	*/

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
 * im_unix -- classic behaviour module for BSD like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         from syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

#include "config.h"

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
#include <time.h>

#include "../modules.h"
#include "../syslogd.h"

/* recvfrom() and others like socklen_t, Irix doesn't provide it */   
#ifndef HAVE_SOCKLEN_T
  typedef int socklen_t;
#endif

#define DEFAULT_LOGGER "/dev/log"

/*
 * initialize input
 */

int
im_unix_init(struct i_module *I, char **argv, int argc)
{
	struct sockaddr_un sunx;
	char *logger;

	m_dprintf(MSYSLOG_INFORMATIVE, "im_unix_init: Entering\n");

	if (I == NULL || argv == NULL || (argc != 2 && argc != 1)) 
return (-1);

	logger = (argc == 2) ? argv[1] : DEFAULT_LOGGER;

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	(void) unlink(logger);

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void)strncpy(sunx.sun_path, logger, sizeof(sunx.sun_path));
	I->im_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (I->im_fd < 0 ||
	    bind(I->im_fd, (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(logger, 0666) < 0) {
		(void) snprintf(I->im_buf, sizeof(I->im_buf),
		    "cannot create %s", logger);
		logerror(I->im_buf);
		m_dprintf(MSYSLOG_SERIOUS, "cannot create %s (%d)\n",
		    logger, errno);
return (-1);
	}

	watch_fd_input('p', I->im_fd , I);
return (1);
}


/*
 * read message
 */

int
im_unix_read(struct i_module *im, int infd, struct im_msg  *ret)
{
	struct sockaddr_un fromunix;
	int slen;

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(fromunix);

	ret->im_len = recvfrom(im->im_fd, ret->im_msg,
	    sizeof(ret->im_msg) - 1, 0, (struct sockaddr *)&fromunix,
	    (socklen_t *)&slen);

	if (ret->im_len > 0) {
		ret->im_msg[ret->im_len] = '\0';
		ret->im_host[0] = '\0';
	}
  else if (ret->im_len < 0 && errno != EINTR) {
		logerror("recvfrom unix");
		ret->im_msg[0] = '\0';
		ret->im_len = 0;
		ret->im_host[0] = '\0';
return (-1);
	}

return (1);
}

/*
 * close input
 */

int
im_unix_close( struct i_module *im)
{
	close(im->im_fd);
return (0);
}


