/*	$CoreSDI: om_regex.c,v 1.13.2.10.4.8 2000/10/20 22:46:36 alejo Exp $	*/

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
 * om_regex -- some explanation for this baby
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 */

#include "../config.h"

#include <sys/time.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <regex.h>
#include "../syslogd.h"
#include "../modules.h"

/* current time from syslogd */

struct om_regex_ctx {
	short		flags;
	int		size;
	int		filters;
#define	OM_FILTER_INVERSE	0x01
#define	OM_FILTER_MESSAGE	0x02
#define	OM_FILTER_HOST		0x04
#define	OM_FILTER_DATE		0x08
#define	OM_FILTER_TIME		0x10
	regex_t	       *msg_exp;
	regex_t	       *host_exp;
	regex_t	       *date_exp;
	regex_t	       *time_exp;
};


/*
 *  INIT -- Initialize om_regex
 *
 */
int
om_regex_init(int argc, char **argv, struct filed *f, char *prog, void **ctx)
{
	struct om_regex_ctx *c;
	int ch;

	/* for debugging purposes */
	dprintf("om_regex init\n");

	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		dprintf("om_regex: error on initialization\n");
		return(-1);
	}

	if ((*ctx = (void *) calloc(1, sizeof(struct om_regex_ctx))) == NULL)
		return (-1);

	c = (struct om_regex_ctx *) *ctx;
	c->size = sizeof(struct om_regex_ctx);

	/*
	 * Parse options with getopt(3)
	 *
	 * we give an example for a -s argument
	 * -v flag means INVERSE matching
	 * -m flag match message (default)
	 * -h flag match host
	 * -d flag match date
	 * -t flag match time
	 *
	 */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "vm:h:d:t:")) != -1) {
		switch (ch) {
		case 'v':
			c->filters |= OM_FILTER_INVERSE;
			break;
		case 'm':
			c->filters |= OM_FILTER_MESSAGE;
			c->msg_exp = (regex_t *) malloc(sizeof(regex_t));
			if (regcomp(c->msg_exp, optarg, REG_EXTENDED) != 0) {
				dprintf("om_regex: error compiling regular "
				    "expression [%s] for message\n", optarg);
				free(c->msg_exp);
				goto bad;
			}
			break;
		case 'h':
			c->filters |= OM_FILTER_HOST;
			c->host_exp = (regex_t *) malloc(sizeof(regex_t));
			if (regcomp(c->host_exp, optarg, REG_EXTENDED) != 0) {
				dprintf("om_regex: error compiling regular "
				    "expression [%s] for message\n", optarg);
				free(c->host_exp);
				goto bad;
			}
			break;
		case 'd':
			c->filters |= OM_FILTER_DATE;
			c->date_exp = (regex_t *) malloc(sizeof(regex_t));
			if (regcomp(c->date_exp, optarg, REG_EXTENDED) != 0) {
				dprintf("om_regex: error compiling regular "
				    "expression [%s] for message\n", optarg);
				free(c->date_exp);
				goto bad;
			}
			break;
		case 't':
			c->filters |= OM_FILTER_TIME;
			c->time_exp = (regex_t *) malloc(sizeof(regex_t));
			if (regcomp(c->time_exp, optarg, REG_EXTENDED) != 0) {
				dprintf("om_regex: error compiling regular "
				    "expression [%s] for message\n", optarg);
				free(c->time_exp);
				goto bad;
			}
			break;
		default:
			dprintf("om_regex: unknown parameter [%c]\n", ch);
			goto bad;
		}
	}

	return (1);
bad:
	free(*ctx);
	return (-1);
}

int
om_regex_doLog(struct filed *f, int flags, char *msg, void *ctx)
{
	struct om_regex_ctx *c;
	char time_buf[16];

	c = (struct om_regex_ctx *) ctx;

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_regex_doLog: no message!");
		return(-1);
	}

	/* return:
			 1  match  -> successfull
			 0  nomatch -> stop logging it
	 */

	if ((c->filters & OM_FILTER_MESSAGE) &&
	    regexec(c->msg_exp, msg, 0, NULL, 0))
		goto nomatch;

	if ((c->filters & OM_FILTER_HOST) &&
	    regexec(c->host_exp, f->f_prevhost, 0, NULL, 0))
		goto nomatch;

	/* get message time and separate date and time */
	if ((c->filters & OM_FILTER_DATE) || (c->filters & OM_FILTER_TIME)) {

		strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S",
		    &f->f_tm);

		time_buf[6] = 0;
		time_buf[15] = 0;
	}

	if ((c->filters & OM_FILTER_DATE) &&
	    regexec(c->date_exp, time_buf, 0, NULL, 0))
		goto nomatch;

	if ((c->filters & OM_FILTER_TIME) &&
	    regexec(c->time_exp, time_buf + 7, 0, NULL, 0))
		goto nomatch;

	/* matched */
	return (c->filters & OM_FILTER_INVERSE ? 0 : 1);

nomatch:
	/* not  matched */
	return (c->filters & OM_FILTER_INVERSE ? 1 : 0);
}

int
om_regex_close(struct filed *f, void *ctx)
{
	struct om_regex_ctx *c;

	c = (struct om_regex_ctx *) ctx;

	if (c->msg_exp)
		free(c->msg_exp);
	if (c->host_exp)
		free(c->host_exp);
	if (c->date_exp)
		free(c->date_exp);
	if (c->time_exp)
		free(c->time_exp);

	return (1);
}
