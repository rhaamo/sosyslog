/*	$CoreSDI: om_filter.c,v 1.4 2000/06/05 21:43:22 fgsch Exp $	*/

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
 * om_filter -- some explanation for this baby
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 */

#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <regex.h>
#include "syslogd.h"
#include "modules.h"

/* current time from syslogd */
extern time_t now;

struct om_filter_ctx {
	short		 flags;
	int		 size;
	regex_t		*exp;
	int		 filters;
#define	OM_FILTER_INVERSE	0x01
#define	OM_FILTER_MESSAGE	0x02
#define	OM_FILTER_HOST		0x04
#define	OM_FILTER_DATE		0x08
#define	OM_FILTER_TIME		0x10
};


/*
 *  INIT -- Initialize om_filter
 *
 */
int
om_filter_init(argc, argv, f, prog, c)
	int argc;		/* argumemt count */
	char **argv;		/* argumemt array, like main() */
	struct filed *f;	/* our filed structure */
	char *prog;		/* program name doing this log */
	struct om_hdr_ctx **c; /* our context */
{
	struct om_filter_ctx *ctx;
	int ch;

	/* for debugging purposes */
	dprintf("om_filter init\n");


	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		dprintf("om_filter: error on initialization\n");
		return(-1);
	}

	if (! (*c = (struct om_hdr_ctx *)
			calloc(1, sizeof(struct om_filter_ctx))))
		return (-1);

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
	while ((ch = getopt(argc, argv, "vmhdt")) != -1) {
		switch (ch) {
			case 'v':
				ctx->filters |= OM_FILTER_INVERSE;
				break;
			case 'm':
				ctx->filters |= OM_FILTER_MESSAGE;
				break;
			case 'h':
				ctx->filters |= OM_FILTER_HOST;
				break;
			case 'd':
				ctx->filters |= OM_FILTER_DATE;
				break;
			case 't':
				ctx->filters |= OM_FILTER_TIME;
				break;
			default:
				dprintf("om_filter: unknown parameter [%c]\n", ch);
				return(-1);
				break;
		}
	}


	ctx = (struct om_filter_ctx *) *c;
	ctx->size = sizeof(struct om_filter_ctx);
	ctx->exp = (regex_t *) calloc(1, sizeof(regex_t));
	ch = REG_EXTENDED;
	if (regcomp(ctx->exp, argv[argc - 1], ch) != 0) {
		dprintf("om_filter: error compiling  regular expression [%s]\n",
				argv[argc - 1]);
		return(-1);
	}

	return(1);
}

int
om_filter_doLog(f, flags, msg, context)
	struct filed	*f;
	int		 flags;
	char		*msg;
	struct om_hdr_ctx *context; /* our context */
{
	struct om_filter_ctx *ctx;
	int ret1, ret2, ret3;
	char *host, *date, *time;

	ctx = (struct om_filter_ctx *) context;

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_filter_doLog: no message!");
		return(-1);
	}

	if (ctx->filters & OM_FILTER_MESSAGE)	
		if ((ret1 = regexec(ctx->exp, msg, 0, NULL, 0)) == 0)
			goto match;

	if (ctx->filters & OM_FILTER_HOST)	
		if (regexec(ctx->exp, host, 0, NULL, 0) == 0)
			goto match;

	if (ctx->filters & OM_FILTER_DATE)	
		if (regexec(ctx->exp, date, 0, NULL, 0) == 0)
			goto match;

	if (ctx->filters & OM_FILTER_TIME)	
		if (regexec(ctx->exp, time, 0, NULL, 0) == 0)
			goto match;

	/* return:
			 1  match  -> successfull
			 0  nomatch -> stop logging it
	 */
match:
	if (ctx->filters & OM_FILTER_INVERSE)

catched:
	if (ctx->filters & OM_FILTER_INVERSE)
		return(!ret1);
	else
		return(ret1);
}


int
om_filter_close(f, ctx)
	struct filed *f;
	struct om_hdr_ctx *ctx;
{
	struct om_filter_ctx *c;

	c = (struct om_filter_ctx *) ctx;

	if (c->exp)
		free(c->exp);

	return (1);
}

int
om_filter_flush(f, context)
	struct filed *f;
	struct om_hdr_ctx *context;
{
	return(1);

}
