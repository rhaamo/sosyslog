/*	$CoreSDI: om_regex.c,v 1.34 2001/03/23 00:12:30 alejo Exp $	*/

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
 * om_regex -- some explanation for this baby
 *
 * Author: Alejo Sanchez for Core-SDI SA
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

#include "../modules.h"
#include "../syslogd.h"

/* current time from syslogd */

struct om_regex_ctx {
	short		flags;
	int		size;
	int		filters;
#define	OM_FILTER_MESSAGE	0x01
#define	OM_FILTER_HOST		0x02
#define	OM_FILTER_DATE		0x04
#define	OM_FILTER_TIME		0x08
#define	OM_FILTER_INVERSE	0x10
	regex_t		msg_exp;
	regex_t		host_exp;
	regex_t		date_exp;
	regex_t		time_exp;
};


/*
 *  INIT -- Initialize om_regex
 *
 */
int
om_regex_init(int argc, char **argv, struct filed *f, char *prog, void **ctx,
    char **status)
{
	struct	om_regex_ctx *c;
	regex_t	*creg;
	int	ch, statbuf_len;
	char	statbuf[1048], *p;

	creg = NULL;
	p = NULL;

	/* for debugging purposes */
	dprintf(MSYSLOG_INFORMATIVE, "om_regex init\n");

	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		dprintf(MSYSLOG_SERIOUS, "om_regex: error on "
		    "initialization\n");
		return (-1);
	}

	if ((*ctx = (void *) calloc(1, sizeof(struct om_regex_ctx))) == NULL)
		return (-1);

	c = (struct om_regex_ctx *) *ctx;
	c->size = sizeof(struct om_regex_ctx);

	statbuf_len = snprintf(statbuf, sizeof(statbuf),
	    "om_regex: filtering");

	/*
	 * Parse options with getopt(3)
	 *
	 * we give an example for a -s argument
	 * -v flag means INVERSE matching
	 * -m flag match message
	 * -h flag match host
	 * -d flag match date
	 * -t flag match time
	 *
	 */
	p = NULL;
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "vm:h:d:t:")) != -1) {
		switch (ch) {
		case 'v':
			c->filters |= OM_FILTER_INVERSE;
			statbuf_len += snprintf(statbuf + statbuf_len,
			    sizeof(statbuf) - statbuf_len, ", inverse");
			continue;

		case 'm':
			c->filters |= OM_FILTER_MESSAGE;
			creg = &c->msg_exp;
			p = ", message";
			break;

		case 'h':
			c->filters |= OM_FILTER_HOST;
			creg = &c->host_exp;
			p = ", host";
			break;

		case 'd':
			c->filters |= OM_FILTER_DATE;
			creg = &c->date_exp;
			p = ", date";
			break;

		case 't':
			c->filters |= OM_FILTER_TIME;
			creg = &c->time_exp;
			p = ", time";
			break;

		default:
			dprintf(MSYSLOG_SERIOUS, "om_regex: unknown parameter"
			    " [%c]\n", ch);
			free(*ctx);
			return (-1);
		}

		if (regcomp(creg, optarg, REG_EXTENDED | REG_NOSUB) != 0) {
			dprintf(MSYSLOG_SERIOUS, "om_regex: error compiling "
			    "regular expression [%s] for message\n", optarg);
			free(*ctx);
			return (-1);
		}

		if (p)
			statbuf_len += snprintf(statbuf + statbuf_len,
			    sizeof(statbuf) - statbuf_len, " %s [%s]", p,
			    optarg);
	}

	*status = strdup(statbuf);

	return (1);
}

/* return:
	 -1  error
	  1  match  -> successfull
	  0  nomatch -> stop logging it
 */

int
om_regex_write(struct filed *f, int flags, char *msg, void *ctx)
{
	struct om_regex_ctx *c;
	regex_t *creg;
	char *str, time_buf[16];
	int i, iflag;

	creg = NULL;
	str = NULL;

	c = (struct om_regex_ctx *) ctx;

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_regex_write: no message!");
		return (-1);
	}

	/* Split date and time if filters are present. */
	if ((c->filters & OM_FILTER_DATE) || (c->filters & OM_FILTER_TIME)) {
		strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S",
		    &f->f_tm);   

		time_buf[6] = 0;
		time_buf[15] = 0;
	}

	iflag = ((c->filters & OM_FILTER_INVERSE) != 0);

	for (i = 1; i < OM_FILTER_INVERSE; i <<= 1) {
		if ((c->filters & i) == 0)
			continue;

		switch (i) {
		case OM_FILTER_MESSAGE:
			creg = &c->msg_exp;
			str = msg;
			break;

		case OM_FILTER_HOST:
			creg = &c->host_exp;
			str = f->f_prevhost;
			break;

		case OM_FILTER_DATE:
			creg = &c->date_exp;
			str = time_buf;
			break;

		case OM_FILTER_TIME:
			creg = &c->time_exp;
			str = time_buf + 7;
			break;
		}

		if ((regexec(creg, str, 0, NULL, 0) != 0) ^ iflag)
				return (0);
	}

	return (1);

}

