/*	$CoreSDI: im_file.c,v 1.12 2002/03/01 07:31:02 alejo Exp $	*/

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
 * im_file -- read from a log file being written by other program
 *
 * Author: Alejo Sanchez for Core-SDI SA
 * http://www.corest.com/
 *
 */

#include "config.h"

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "../modules.h"
#include "../syslogd.h"

struct im_file_ctx {
	char	*timefmt;
	int	start;
};

/*
 * initialize file input
 *
 */

int
im_file_init(struct i_module *I, char **argv, int argc)
{
	char			*path;
	struct im_file_ctx	*c;
	int			ch, argcnt, start;

	dprintf(MSYSLOG_INFORMATIVE, "im_file_init: Entering\n");

	I->im_path = NULL;
	start = 0;
	path = NULL;
	c = NULL;	/* just to make compiler happy */

	/* parse command line */
	argcnt = 1;	/* skip module name */

	while ((ch = getxopt(argc, argv, "f!file: p!pipe: n!program:"
	    " t!timeformat: s!startpos:", &argcnt)) != -1) {
		switch (ch) {
		case 'f':
			/* file to read */
			path = argv[argcnt];
			break;
		case 'p':
			/* pipe to read */
			path = argv[argcnt];
			break;
		case 'n':
			I->im_path = strdup(argv[argcnt]);
			break;
		case 's':
			start = strtol(argv[argcnt], NULL, 10);
			break;
		case 't':
			if ((I->im_ctx = malloc(sizeof(struct im_file_ctx)))
			    == NULL) {
				dprintf(MSYSLOG_SERIOUS, "om_file_init: error"
				    " allocating context!\n");
				return (-1);
			}
			c = (struct im_file_ctx *) I->im_ctx;
			c->timefmt = strdup(argv[argcnt]);
			break;
		default:
			dprintf(MSYSLOG_SERIOUS, "om_file_init: command line"
			    " error, at arg %c [%s]\n", ch,
			    argv[argcnt]? argv[argcnt]: "");
			return (-1);
		}
		argcnt++;
	}

	if (I->im_ctx != NULL && c->timefmt == NULL) {
		dprintf(MSYSLOG_SERIOUS, "om_file_init: start of time string but"
		    " no string!\n");
		return (-1);
	}

	if (I->im_ctx != NULL)
		c->start = start;

	if (path == NULL) {
		dprintf(MSYSLOG_SERIOUS, "om_file_init: no file/pipe to read\n");
		return (-1);
	}

	if ((I->im_fd = open(path, O_RDONLY, 0)) < 0) {
		dprintf(MSYSLOG_SERIOUS, "im_file_init: can't open %s (%d)\n",
		    argv[1], errno);
		return (-1);
	}

	if (I->im_path == NULL)
		I->im_path = path;	/* no name specified */

	add_fd_input(I->im_fd , I);

	return (1);
}

/*
 * get message
 *
 */

int
im_file_read(struct i_module *im, int infd, struct im_msg  *ret)
{
	char *p, *q;
	int i, c;
	RETSIGTYPE (*sigsave)(int);

        /* ignore sigpipes   for mysql_ping */
        sigsave = place_signal(SIGPIPE, SIG_IGN);

	i = read(im->im_fd, im->im_buf, sizeof(im->im_buf) - 1);
	if (i > 0) {
		(im->im_buf)[i] = '\0';
		for (p = im->im_buf; *p != '\0'; ) {

			/* fsync file after write */
			ret->im_flags = ADDDATE;
			ret->im_pri = DEFSPRI;

			if (im->im_ctx != NULL) {
				struct tm		tm;
				struct im_file_ctx	*c;
				char			*start, *end;

				/* apply strftime */
				c = (struct im_file_ctx *) im->im_ctx;
				if ((end = strptime((p + c->start), c->timefmt, &tm))
				    == NULL) {

					dprintf(MSYSLOG_WARNING, "om_file_read"
					    ": error parsing time!\n");

				} else {

					for (start = p + c->start; *end != '\0';)
						*start++ = *end++;
					*start = '\0';

					if (strftime(ret->im_msg,
					    sizeof(ret->im_msg) - 1,
					    "%b %e %H:%M:%S ", &tm) == 0) {
						dprintf(MSYSLOG_WARNING,
						    "om_file_read: error "
						    "printing time!\n");
					} else {
						ret->im_flags &= !ADDDATE;
					}
				}
			} else if (*p == '<') {
				ret->im_pri = 0;
				while (isdigit((int)*++p))
					ret->im_pri = 10 * ret->im_pri +
					    (*p - '0');
				if (*p == '>')
					++p;
			}

			strncat(ret->im_msg, im->im_path, strlen(ret->im_msg)
			    - sizeof(ret->im_msg) - 1);
			strncat(ret->im_msg, ":", strlen(ret->im_msg)
			    - sizeof(ret->im_msg) - 1);
			dprintf(MSYSLOG_INFORMATIVE, "im_file_read: Entering "
			    "with header %s\n", ret->im_msg);

			if (ret->im_pri &~ (LOG_FACMASK|LOG_PRIMASK))
				ret->im_pri = DEFSPRI;
			q = ret->im_msg + strlen(ret->im_msg);
			while (*p != '\0' && (c = *p++) != '\n' &&
			    q < &ret->im_msg[sizeof(ret->im_msg) - 1]) {
				*q++ = c;
			}
			*q = '\0';
			ret->im_host[0] = '\0';
			ret->im_len = strlen(ret->im_msg);
			logmsg(ret->im_pri, ret->im_msg, ret->im_host,
			    ret->im_flags);
		}
	} else if (i < 0 && errno != EINTR) {
		logerror("im_file_read");
		im->im_fd = -1;
	}

	/* restore previous SIGPIPE handler */
	place_signal(SIGPIPE, sigsave);

	/* if ok return (2) wich means already logged */
	return (im->im_fd == -1 ? -1: 2);
}

int
im_file_close( struct i_module *im)
{
	if (im->im_ctx != NULL) {
		struct im_file_ctx	*c;

		c = (struct im_file_ctx *) im->im_ctx;

		if (c->timefmt)
			free(c->timefmt);
		free(c);
	}

	if (im->im_path != NULL)
		free(im->im_path);

	if (im->im_fd >= 0)
		close(im->im_fd);

	return (0);
}
