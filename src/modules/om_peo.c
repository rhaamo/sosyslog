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
static char rcsid[] = "$Id: om_peo.c,v 1.6 2000/05/02 21:10:58 claudio Exp $";
#endif /* not lint */

/*
 *  om_peo -- peo autentication
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 */

#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syslog.h>

#include "../syslogd.h"
#include "../modules.h"
#include "hash.h"

struct om_peo_ctx {
	short	flags;
	int	size;

	int	hash_method;
	char	*keyfile;
	char	*hashedlogfile;
};

int
om_peo_doLog(f, flags, msg, context)
	struct filed *f;
	int flags;
	char *msg;
	struct om_header_ctx *context;
{
	struct om_peo_ctx *c;
	int	 fd;
	int	 hfd;
	u_char	 mkey[41];
	u_char	 key[41];
	int	 keylen;
	int	 len;
	char	*m;

	dprintf ("peo output module: doLog\n");
	
	if (f == NULL || context == NULL)
		return (-1);

	c = (struct om_peo_ctx*) context;

	if (msg == NULL) {
		m = f->f_prevline;
		len = f->f_prevlen;
	}
	else {
		m = msg;
		len = strlen(m);
	}

	/* open keyfile and read last key */
	if ( (fd = open(c->keyfile, O_RDWR, 0)) == -1)
		return (-1);
	bzero(key, 41);
	if ( (keylen = read(fd, key, 40)) == -1) {
		close(fd);
		return(-1);
	}

	dprintf ("last key: %s\n", key);

	/* open hashedlogfile and write hashed msg */
	if ( (hfd = open(c->hashedlogfile, O_WRONLY, 0)) == -1) {
		fclose(fd);
		return (-1);
	}
	hash(SHA1, key, keylen, m, mkey);
	lseek(hfd, 0, SEEK_END);
	write(hfd, mkey, 40);
	close(hfd);

	dprintf ("hashed msg logged\n");

	/* generate new key and save it on keyfile */
	keylen = hash(c->hash_method, key, keylen, m, key);
	lseek(fd, 0, SEEK_SET);
	ftruncate(fd, 0);
	write(fd, key, keylen);

	dprintf (" new key: %s\n", key);

	close(fd);
	return(0);
}


/*
 *  INIT -- Initialize om_peo
 *
 *  Parse options
 * 
 *  params:
 * 
 *	-k <keyfile>		(default: /var/ssyslog/.var.log.messages)
 *	-l			line number corruption detect
 *				(generates a strcat(keyfile, ".msg") file)
 *	-m <hash_method>	sha1 or md5 (default: sha1)
 *
 */

extern char	*optarg;
extern int	 optind,
		 optreset;

int
om_peo_init(argc, argv, f, prog, context)
	int argc;
	char **argv;
	struct filed *f;
	char *prog;
	struct om_header_ctx **context;
{
	int	 ch;
	int	 hash_method;
	int	 hlf;
	char	*keyfile;
	char	*hashedlogfile;
	struct	 om_peo_ctx *c;

	dprintf("peo output module init: called by %s\n", prog);
	
	if (argv == NULL || *argv == NULL ||
		argc == 0 || f == NULL || context == NULL)
		return (-1);

	/* default values */
	hash_method = SHA1;
	keyfile = default_keyfile;
	hashedlogfile = NULL;
	hlf = 0;

	/* parse command line */
	optreset = 1; optind = 0;
	while ((ch = getopt(argc, argv, "k:lm:")) != -1) {
		switch(ch) {
			case 'k':
				/* set keyfile */
				if ( (keyfile = strdup(optarg)) == NULL)
					return (-1);
				break;
			case 'l':
				/* set hashedlog file */
				hlf = 1;
				break;
			case 'm':
				/* set method */
				if ( (hash_method = gethash(optarg)) < 0) {
					if (keyfile != default_keyfile)
						free(keyfile);
					errno = EINVAL;
					return (-1);
				}
				break;
			default:
				if (keyfile != default_keyfile)
					free(keyfile);
				errno = EINVAL;
				return (-1);
		}
	}

	/* set hashed log file */
	if (hlf) {
		if ( (hashedlogfile = (char*) calloc(1, strlen(keyfile)+4)) == NULL) {
			if (keyfile != default_keyfile)
				free (keyfile);
			return (-1);
		}
		strcpy(hashedlogfile, keyfile);
		strcat(hashedlogfile, ".msg");

		if (! (hlf = open(hashedlogfile, O_CREAT, S_IRUSR|S_IWUSR))) {
			if (errno != EEXIST)
				return (-1);
		}
		else close(hlf);
		
	}

	/* save data on context */
	if ( (c = (struct om_peo_ctx*)
		calloc (1, sizeof(struct om_peo_ctx))) == NULL) {
			if (keyfile != default_keyfile)
				free (keyfile);
			if (hlf)
				free(hashedlogfile);
			return (-1);
		}

	c->size = sizeof(struct om_peo_ctx);
	c->hash_method = hash_method;
	c->keyfile = keyfile; 
	c->hashedlogfile = hashedlogfile;
	*context = (struct om_header_ctx*) c;

	dprintf ("method: %d\nkeyfile: %s\nhashedlogfile: %s\n",
		hash_method, keyfile, hashedlogfile);

	return (0);
}


int
om_peo_close(f, context)
	struct filed *f;
	struct om_header_ctx **context;
{
	struct om_peo_ctx *c;

	dprintf ("peo output module close\n");

	c = (struct om_peo_ctx*) *context;
	if (c->keyfile != default_keyfile)
		free(c->keyfile);
	if (c->hashedlogfile)
		free(c->hashedlogfile);
	free (*context);
	*context = NULL;
	return (0);
}

int
om_peo_flush(f, context)
	struct filed *f;
	struct um_header_ctx *context;
{
	/* no data to flush */
	return (0);
}

