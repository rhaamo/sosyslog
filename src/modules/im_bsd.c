/*	$CoreSDI: im_bsd.c,v 1.47 2000/06/02 22:59:35 fgsch Exp $	*/

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
 * im_bsd -- classic behaviour module for BSD like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         from syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

#include "config.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "syslogd.h"

/*
 * initialize BSD input
 *
 */

int
im_bsd_init(I, argv, argc)
	struct i_module *I;
	char	**argv;
	int	argc;
{
	if ((I->im_fd = open(_PATH_KLOG, O_RDONLY, 0)) < 0) {
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
	}
	
	I->im_type = IM_BSD;
	I->im_name = strdup("bsd");
	I->im_path = strdup(_PATH_KLOG);
	I->im_flags |= IMODULE_FLAG_KERN;
	return(I->im_fd);
}


/*
 * get messge
 *
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */

int
im_bsd_getLog(im, ret)
	struct i_module *im;
	struct im_msg  *ret;
{
	char *p, *q, *lp;
	int i, c;

	(void)strncpy(ret->im_msg, _PATH_UNIX, sizeof(ret->im_msg) - 3);
	(void)strncat(ret->im_msg, ": ", 2);
	lp = ret->im_msg + strlen(ret->im_msg);

	i = read(im->im_fd, im->im_buf, sizeof(im->im_buf) - 1);
	if (i > 0) {
		(im->im_buf)[i] = '\0';
		for (p = im->im_buf; *p != '\0'; ) {
			/* fsync file after write */
			ret->im_flags = SYNC_FILE | ADDDATE;
			ret->im_pri = DEFSPRI;
			if (*p == '<') {
				ret->im_pri = 0;
				while (isdigit(*++p))
					ret->im_pri = 10 * ret->im_pri +
					    (*p - '0');
				if (*p == '>')
					++p;
			} else {
				/* kernel printf's come out on console */
				ret->im_flags |= IGN_CONS;
			}
			if (ret->im_pri &~ (LOG_FACMASK|LOG_PRIMASK))
				ret->im_pri = DEFSPRI;
			q = lp;
			while (*p != '\0' && (c = *p++) != '\n' &&
			    q < (ret->im_msg+sizeof ret->im_msg))
				*q++ = c;
			*q = '\0';
			strncat(ret->im_host, LocalHostName,
			    sizeof(ret->im_host) - 1);
			ret->im_len = strlen(ret->im_msg);
			logmsg(ret->im_pri, ret->im_msg, ret->im_host,
			    ret->im_flags);
		}
	} else if (i < 0 && errno != EINTR) {
		logerror("im_bsd_getLog");   
		im->im_fd = -1;
	}

	/* if ok return (2) wich means already logged */
	return(im->im_fd == -1 ? -1: 2);
}

int
im_bsd_close(im)
	struct i_module *im;
{
	if (im->im_path)
		free(im->im_path);

	if (im->im_name)
		free(im->im_name);

	return(close(im->im_fd));
}
