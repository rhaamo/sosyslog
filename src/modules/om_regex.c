/*	$CoreSDI: om_filter.c,v 1.1 2000/06/02 01:04:13 alejo Exp $	*/

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
 *  om_filter -- some explanation for this baby
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 *
 */

/* check what include you need !!! */
#include <sys/time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <regexp.h>
#include "syslogd.h"
#include "modules.h"

/* current time from syslogd */
extern time_t now;

struct om_filter_ctx {
	short			 flags;
	int				 size;
	struct regexp	*exp;
};


int
om_filter_doLog(f, flags, msg, context)
	struct filed	*f;
	int				 flags;
	char			*msg;
	struct om_hdr_ctx *context; /* our context */
{
	struct om_filter_ctx *ctx;

	ctx = (struct om_filter_ctx *) context;

	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_filter_doLog: no message!");
		return(-1);
	}

	/* return:
			 1  match  -> successfull
			 0  nomatch -> stop logging it
	 */
	return(regexec(ctx->exp, msg));
}


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
	char *expression;

	/* for debugging purposes */
	dprintf("om_filter init\n");

	/*
	 * Parse your options with getopt(3)
	 *
	 * we give an example for a -s argument
	 * -v flag means REVERSE matching
	 *
	 */

	if (argc < 2 || argv == NULL || argv[1] == NULL) {
		dprintf("om_filter: error on initialization\n");
		return(-1);
	}

	if (! (*c = (struct om_hdr_ctx *)
			calloc(1, sizeof(struct om_filter_ctx))))
		return (-1);


	ctx = (struct om_filter_ctx *) *c;
	ctx->size = sizeof(struct om_filter_ctx);
	ctx->exp = regcomp(argv[argc - 1]);

	if (argc == 3) {
		if (!strncmp(argv[1], "-v", 2))
			ctx->flags |= OM_FLAG_REVERSE;
	}

	
	return(1);
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
