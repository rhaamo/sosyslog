/*	$CoreSDI: om_peo.c,v 1.36 2000/06/06 00:32:39 fgsch Exp $	*/

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
 * om_peo -- peo autentication
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 */

#include "config.h"

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
#include <syslog.h>

#include "syslogd.h"
#include "modules.h"
#if 0
#include "hash.h"
#else
extern char *default_keyfile;
#define	SHA1	0
#endif


#define MAXBUF	MAXSVLINE+MAXHOSTNAMELEN+20

struct om_peo_ctx {
	short	flags;
	int	size;
	int	hash_method;
	char	*keyfile;
	char	*macfile;
};

int
om_peo_doLog(f, flags, msg, ctx)
	struct filed *f;
	int flags;
	char *msg;
	struct om_hdr_ctx *ctx;
{
	struct om_peo_ctx *c;
	int	 fd;
	u_char	 key[41];
	int	 keylen;
	int	 len;
	char	 m[MAXBUF];
	int	 mfd;
	u_char	 mkey[41];
	char	 newkey[41];
	int	 newkeylen;

	dprintf ("peo output module: doLog\n");
	
	if (f == NULL || ctx == NULL)
		return (-1);

	c = (struct om_peo_ctx*) ctx;

	len = snprintf(m, MAXBUF, "%s %s %s\n", f->f_lasttime, f->f_prevhost,
	    msg ? msg : f->f_prevline) - 1;

	dprintf ("len = %i, msg = %s\n ", len, m);

	/* open keyfile and read last key */
	if ( (fd = open(c->keyfile, O_RDWR, 0)) == -1) {
		dprintf ("opening keyfile: %s: %s\n", c->keyfile,
		    strerror(errno));
		return (-1);
	}
	bzero(key, 41);
	if ( (keylen = read(fd, key, 40)) == -1) {
		close(fd);
		dprintf ("reading form: %s: %s\n", c->keyfile, strerror(errno));
		return(-1);
	}

	/* open macfile and write mac'ed msg */
	if (c->macfile) {
		if ( (mfd = open(c->macfile, O_WRONLY, 0)) == -1) {
			close(fd);
			dprintf ("opening macfile: %s: %s\n", c->macfile, strerror(errno));
			return (-1);
		}
		lseek(mfd, (off_t)0, SEEK_END);
		write(mfd, mkey, mac2(key, keylen, m, len, mkey));
		dprintf ("write to macfile ok\n");
		close(mfd);
	}

	/* generate new key and save it on keyfile */
	lseek(fd, (off_t)0, SEEK_SET);
	ftruncate(fd, (off_t)0);
	if ( (newkeylen = mac(c->hash_method, key, keylen, m, len, newkey)) == -1) {
		close(fd);
		dprintf ("generating key[i+1]: keylen = %i: %s\n", newkeylen, strerror(errno));
		return (-1);
	}
	write(fd, newkey, newkeylen);
	close(fd);
	return(1);
}


/*
 *  INIT -- Initialize om_peo
 *
 *  Parse options
 * 
 *  params:
 * 
 *	-k <keyfile>		(default: /var/ssyslog/.var.log.messages)
 *	-l			line number corruption detect mode
 *				(generates a strcat(keyfile, ".mac") file)
 *	-m <hash_method>	md5, rmd160, or sha1 (default: sha1)
 *
 */

extern char	*optarg;
extern int	 optind;
#ifdef HAVE_OPTRESET
extern int	optreset;
#endif /* HAVE_OPTRESET */
char		*keyfile;
char		*macfile;

void
release()
{
	if (keyfile != default_keyfile)
		free(keyfile);
	if (macfile)
		free(macfile);
}

int
om_peo_init(argc, argv, f, prog, ctx)
	int			  argc;
	char			**argv;
	struct filed		 *f;
	char			 *prog;
	struct om_header_ctx	**ctx;
{
	int	 ch;
	struct	 om_peo_ctx *c;
	int	 hash_method;
	int	 mfd;

	dprintf("peo output module init: called by %s\n", prog);
	
	if (argv == NULL || *argv == NULL || argc == 0 || f == NULL ||
	    ctx == NULL)
		return (-1);

	/* default values */
	hash_method = SHA1;
	keyfile = default_keyfile;
	macfile = NULL;
	mfd = 0;

	/* parse command line */
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif /* HAVE_OPTRESET */
	optind = 1;
	while ((ch = getopt(argc, argv, "k:lm:")) != -1) {
		switch(ch) {
			case 'k':
				/* set keyfile */
				release();
				if ( (keyfile = strdup(optarg)) == NULL)
					return (-1);
				break;
			case 'l':
				/* set macfile */
				mfd = 1;
				break;
			case 'm':
				/* set method */
				if ( (hash_method = gethash(optarg)) < 0) {
					release();
					errno = EINVAL;
					return (-1);
				}
				break;
			default:
				release();
				errno = EINVAL;
				return (-1);
		}
	}

	/* set macfile */
	if (mfd) {
		if ( (macfile = strmac(keyfile)) == NULL) {
			release();
			return (-1);
		}
		if (! (mfd = open(macfile, O_CREAT, S_IRUSR|S_IWUSR))) {
			if (errno != EEXIST) {
				release();
				return (-1);
			}
		}
		else close(mfd);
	}

	/* save data on context */
	if ( (c = (struct om_peo_ctx*)
		calloc (1, sizeof(struct om_peo_ctx))) == NULL) {
			release();
			return (-1);
	}

	c->size = sizeof(struct om_peo_ctx);
	c->hash_method = hash_method;
	c->keyfile = keyfile; 
	c->macfile = macfile;
	*ctx = (struct om_header_ctx*) c;

	dprintf ("method: %d\nkeyfile: %s\nmacfile: %s\n", hash_method,
	    keyfile, macfile);

	return (1);
}


int
om_peo_close(f, ctx)
	struct filed *f;
	struct om_hdr_ctx *ctx;
{
	struct om_peo_ctx *c;

	c = (struct om_peo_ctx *) ctx;
	dprintf ("peo output module close\n");

	if (c->keyfile != default_keyfile)
		free(c->keyfile);
	if (c->macfile)
		free(c->macfile);
	return (0);
}
