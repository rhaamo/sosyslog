/*
 * Copyright (c) 1983, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";*/
static char rcsid[] = "$Id: om_peo.c,v 1.3 2000/04/19 22:43:13 claudio Exp $";
#endif /* not lint */

/*
 *  om_peo -- peo autentication
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
#include <unistd.h>
#include <sys/syslog.h>

#include "syslogd.h"
#include "modules.h"


enum {
	SHA1 = 0,
	MD5,
	UNKNOWN
	};


struct om_peo_ctx {
	short	flags;
	int	size;

	int	hash_method;
	char	*keyfile;
	};


int
om_peo_doLog(f, flags, msg, context)
	struct filed *f;
	int flags;
	char *msg;
	void *context;
{
	return (-1);
}

/*
 *  INIT -- Initialize om_peo
 *
 *  Parse options
 * 
 *  params:
 * 
 *	-k <keyfile>
 *	-m <hash_method>	sha1 or md5
 *
 */

extern char	*optarg;
extern int	optind,
		optreset;

int
om_peo_init(argc, argv, f, prog, context)
	int argc;
	char **argv;
	struct filed *f;
	char *prog;
	void *context;
{
	int	ch;
	int	hash_method;
	char	*keyfile;

	dprintf("peo output module init\n");
	
	if (argv == NULL || *argv == NULL || !argc || f == NULL || c == NULL)
		return (-1);

	/* default values */
	hash_method = SHA1;
	if (! (keyfile = strdup("/var/ssyslog")))
		return (-1);

	/* parse line */
	optreset = 1; optind = 0;
	while ((ch = getopt(argc, argv, "k:m:")) != -1) {
		switch(ch) {
			case 'm':
				/* set method */
				if (!strcasecmp(optarg, "sha1"))
					hash_method = SHA1;
				else if (!strcasecmp(optarg, "md5"))
					hash_method = MD5; 
				else {
					free (keyfile);
					return (-1);
					}
				break;
			case 'k':
				/* set keyfile */
				free (keyfile);
				if (! (keyfile = strdup(optarg)))
					return (-1);
				break;
			default:
				return (-1);
		}
	}

	/* save data on context */
	if (! (*c = (struct om_header_ctx*)
	      calloc (1, sizeof(struct om_peo_ctx)))) {
		free (keyfile);
		return (-1);
		}

	context = (struct om_peo_ctx*) *c;
	context->size = sizeof(struct om_peo_ctx);
	context->hash_method = hash_method;
	context->keyfile = keyfile; 

	return (0);
}


int
om_peo_close(f, context)
	struct filed *f;
	void *context;
{
	return (-1);
}

int
om_peo_flush(f, context)
	struct filed *f;
	void *context;
{
	return (-1);
}

