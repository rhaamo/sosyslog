/*	$CoreSDI: om_mymodule.c,v 1.4 2000/05/29 21:08:28 alejo Exp $	*/

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
 *  om_mymodule -- some explanation for this baby
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
#include "syslogd.h"
#include "modules.h"

/* current time from syslogd */
extern time_t now;

int
om_mymodule_doLog(f, flags, msg, context)
	struct filed *f;    /* current filed struct */
	int flags;          /* flags for this message */
	char *msg;          /* the message string */
	struct om_hdr_ctx *context; /* our context */
{

	/* always check, just in case ;) */
	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_mymodule_doLog: no message!");
		return(-1);
	}

	/*  here you must do your loggin
	    take care with repeats and if message was repeated
	    increase f_prevcount, else set f_prevcount to 0.
	 */

	/* return:
			 1  successfull
			 0  stop logging it (used by filters)
			-1  error logging (but let other modules process it)
	 */

	return(1);
}


/*
 *  INIT -- Initialize om_mymodule
 *
 */
int
om_mymodule_init(argc, argv, f, prog, context)
	int argc;		/* argumemt count */
	char **argv;		/* argumemt array, like main() */
	struct filed *f;	/* our filed structure */
	char *prog;		/* program name doing this log */
	struct om_hdr_ctx **context; /* our context */
{

	/* for debugging purposes */
	dprintf("om_mymodule init\n");

	/* parse your options with getopt(3)
	 */

	/* open files, connect  to database, initialize algorithms,
	   etc. Save them in your context if necesary.
 	 */   

	/* return:
			 1  OK
			-1  something went wrong
	*/

	return(1);
}

int
om_mymodule_close(f, ctx)
	struct filed *f;
	struct om_hdr_ctx *ctx;
{
	/* flush any buffered data and close this output */

	/* return:
			 1  OK
			-1  BAD
	 */
	return (ret);
}

int
om_mymodule_flush(f, context)
	struct filed *f;
	struct om_hdr_ctx *context;
{
	/* flush any pending output */

	/* return:
			 1  OK
			-1  BAD
	 */
	return(1);

}
