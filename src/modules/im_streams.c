/*      $CoreSDI: im_streams.c,v 1.17 2001/11/21 05:15:25 alejo Exp $   */

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
 * im_streams -- gather logging information from streams device (for sysv)
 *      
 * Author: ari edelkind (10/31/2000)
 *    
 */

#include "config.h"

#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#include <stropts.h>
#include <sys/strlog.h>
#include <sys/types.h>
#include <sys/conf.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include "../modules.h"
#include "../syslogd.h"

struct im_streams_ctx {
  char*   path;
};

/* global variables and definitions */
#define DEFAULT_LOGGER "/dev/log"

/*
 * initialize streams input
 *
 */

int
im_streams_init (struct i_module *I, char **argv, int argc)
{
	m_dprintf(MSYSLOG_INFORMATIVE, "im_streams_init: Entering\n");

	if (I == NULL || argv == NULL || argc < 1 || argc > 2) {
		m_dprintf(MSYSLOG_SERIOUS, "usage: -i streams[:path]\n\n");
		return(-1);
	}

  if ( (I->im_ctx = calloc(1, sizeof(struct im_streams_ctx))) == NULL) {
    m_dprintf(MSYSLOG_SERIOUS, "om_streams_init: cannot alloc memory");
return (-1);
  }
  c = (struct im_streams_ctx *) I->im_ctx;

	if (argc == 2) {
		c->path = strdup(argv[1]);
	}
  else {
		c->path = strdup(DEFAULT_LOGGER);
	}
	m_dprintf(MSYSLOG_INFORMATIVE, "streams_logpath = %s\n", c->path);
	I->im_fd = open (c->path, O_RDONLY|O_NOCTTY|O_NONBLOCK);

	if (I->im_fd == -1) {
		m_dprintf(MSYSLOG_SERIOUS, "couldn't open %s: %s\n", c->path,
		    strerror (errno));
return (-1);
	}

  {
		struct strioctl ioctbuf;

		memset (&ioctbuf, 0, sizeof(ioctbuf));

		ioctbuf.ic_cmd = I_CONSLOG; /* why I_CONSLOG? */
		if (ioctl (I->im_fd, I_STR, &ioctbuf) == -1) {
			m_dprintf(MSYSLOG_SERIOUS, "ioctl(%s): %s\n", c->path, strerror (errno));
			close (I->im_fd);
			return (-1);
		}
		add_fd_input(I->im_fd , I);
	}
return (1);
}


/*
 * get message
 *
 */

int
im_streams_read (struct i_module *im, int infd, struct im_msg *ret)
{
	struct strbuf ctl, dat;
	struct log_ctl     lc;
	char   msgbuf[MAXLINE];
	int    r, flags = 0;

	ctl.maxlen = sizeof(lc);
	dat.maxlen = sizeof(msgbuf);
	ctl.buf    = (char *)&lc;
	dat.buf    = (char *)msgbuf;
	ctl.len    = ctl.maxlen;
	dat.len    = 0;

	if (im->im_fd < 0)
		return (-1);

	r = getmsg (im->im_fd, &ctl, &dat, &flags);

	if (r & MORECTL) {
		m_dprintf(MSYSLOG_SERIOUS, "im_streams_read: getmsg() "
		    "returned too much control information\n");
		logerror("im_streams_read: getmsg() returned too much"
		    " control information");
		return (-1);
	}

	do {
		if (r & MOREDATA) {
			/* message is too long for im_msg */
			m_dprintf(MSYSLOG_INFORMATIVE, "im_streams_read: "
			    "STREAMS device offered too much data (remainder "
			    "to come) ...\n");
		}


    /* ensure the data buffer is in the proper format */
    {
    	register char *dataptr;
    	register char c;
    	register int i;

    	dataptr = dat.buf;
    	i       = dat.len;

    	/* this is necessary on some platforms (i.e. irix), but
    	   not others (i.e. solaris). */
    	if (*dataptr == '<') {
    		for (;;) {
    			c = *(++dataptr); --i;
    			if (c == '>') { ++dataptr; --i; break; }
    			if (c >= '0' && c <= '9') continue;
    			break; /* not a digit, not an end-bracket */
    		}
    	}
    	dat.buf = dataptr;
    	dat.len = i;
    }

		/* msgbuf still points to the old data */

		if (dat.len) {
			ret->im_msg[dat.len] = '\0';
			memmove (ret->im_msg, dat.buf, dat.len);
			ret->im_len = dat.len;
			ret->im_pri = lc.pri;

			ret->im_host[0] = '\0';
			logmsg(ret->im_pri, ret->im_msg, ret->im_host,
			    ret->im_flags);
		} else {
			m_dprintf(MSYSLOG_INFORMATIVE, "im_streams_read: "
			    "STREAMS device offered no data?\n");
			logerror("im_streams_read: STREAMS device offered"
			    " no data?");
		}
	} while (r & MOREDATA);

	return(0);
}

/*
 * the following function is not mandatory, you can omit it
 */
int
im_streams_close (struct i_module *im) 
{
  struct im_streams_ctx *c = (im_streams_ctx *) im->ctx;

  if (c->path != NULL) free(c->path);
  if (im->ctx != NULL) free(im->ctx);

	close (im->im_fd); 

return(1);
}


