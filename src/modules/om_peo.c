/*	$CoreSDI: om_peo.c,v 1.77 2002/03/01 07:31:03 alejo Exp $	*/

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
 * om_peo -- peo autentication
 *
 * Author: Claudio Castiglia
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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <strings.h>

#include "../modules.h"
#include "../syslogd.h"
#if 1
#include "../peo/hash.h"
#else
extern char *default_keyfile;
#define	SHA1	0
#endif

#define MAXBUF	MAXSVLINE+MAXHOSTNAMELEN+20

struct om_peo_ctx {
	short	flags;
	int	size;
	int	hash_method;
	char   *keyfile;
	char   *macfile;
};

int
om_peo_write(struct filed *f, int flags, struct m_msg *msg, void *ctx)
{
	struct om_peo_ctx	*c;
	int			 fd, mfd, len, keylen, newkeylen;
	u_char			 key[41], mkey[41];
	unsigned char		 m[MAXBUF], newkey[41];
	char			 time_buf[16];
  msg->fired++;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_peo_write: Entering\n");
	
	if (f == NULL || ctx == NULL || msg == NULL)
		return (-1);

	c = (struct om_peo_ctx *) ctx;

	strftime(time_buf, sizeof(time_buf), "%b %e %H:%M:%S", &f->f_tm);
	time_buf[15] = '\0';
	len = snprintf((char *) m, MAXBUF, "%s %s %s\n", time_buf,
	    f->f_prevhost, msg->msg ? msg->msg : f->f_prevline) - 1;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_peo_write: len = %i, msg->msg = %s\n ",
	    len, m);

	/* Open keyfile and read last key */
	if ( (fd = open(c->keyfile, O_RDWR, 0)) < 0) {
		m_dprintf(MSYSLOG_SERIOUS, "om_peo_write: opening keyfile: %s:"
		    " %s\n", c->keyfile, strerror(errno));
		return (-1);
	}
	bzero(key, sizeof(key));
	if ( (keylen = read(fd, key, 40)) < 0) {
		close(fd);
		m_dprintf(MSYSLOG_SERIOUS, "om_peo_write: reading form: %s:"
		    " %s\n", c->keyfile, strerror(errno));
		return (-1);
	}

	/* Open macfile and write mac'ed msg */
	if (c->macfile) {
		if ( (mfd = open(c->macfile, O_WRONLY, 0)) < 0) {
			close(fd);
			m_dprintf(MSYSLOG_SERIOUS, "om_peo_write: opening "
			    "macfile: %s: %s\n", c->macfile,
			    strerror(errno));
			return (-1);
		}
		lseek(mfd, (off_t) 0, SEEK_END);
		write(mfd, mkey, mac2(key, keylen, m, len, mkey));
		m_dprintf(MSYSLOG_INFORMATIVE, "om_peo_write: write to macfile"
		    " ok\n");
		close(mfd);
	}

	/* Generate new key and save it on keyfile */
	lseek(fd, (off_t)0, SEEK_SET);
	ftruncate(fd, (off_t)0);
	newkeylen = mac(c->hash_method, key, keylen, m, len, newkey);
	if (newkeylen == -1) {
		close(fd);
		m_dprintf(MSYSLOG_INFORMATIVE, "om_peo_write: generating "
		    "key[i+1]: keylen = %i: %s\n", newkeylen,
		    strerror(errno));
		return (-1);
	}
	write(fd, newkey, newkeylen);
	close(fd);
	return (1);
}


/*
 *  INIT -- Initialize om_peo
 *  args:
 * 
 *	-k <keyfile>		(default: /var/ssyslog/.var.log.messages)
 *	-l			line number corruption detect mode
 *				(generates a strcat(keyfile, ".mac") file)
 *	-m <hash_method>	md5, rmd160, or sha1 (default: sha1)
 *
 */
char		*keyfile;
char		*macfile;
void

release(void)
{
	if (keyfile != default_keyfile)
		free(keyfile);
	if (macfile != NULL)
		free(macfile);
}

int
om_peo_init(
    int argc,
    char **argv,
    struct filed *f,
    struct global *global,
    void **ctx,
	  char **status)
{
	char			 statbuf[2048];
	struct om_peo_ctx	*c;
	int			 hash_method;
	int			 mfd;
	int			 ch;
	int			 argcnt;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_peo_init: Entering, called by %s\n", global->prog);
	
	if (argv == NULL || *argv == NULL || argc == 0 || f == NULL ||
	    ctx == NULL)
		return (-1);

	/* default values */
	hash_method = SHA1;
	keyfile = default_keyfile;
	macfile = NULL;
	mfd = 0;

	argcnt = 1;	/* skip module name */

	while ( (ch = getxopt(argc, argv, "k!keyfile: l!macfile m!method:",
	    &argcnt)) != -1) {

		switch (ch) {
		case 'k':
			/* set keyfile */
			release();
			if ((keyfile = strdup(argv[argcnt])) == NULL) {
				return (-1);
			}
			break;
		case 'l':
			/* set macfile */
			mfd = 1;
			break;
		case 'm':
			/* set method */
			if ( (hash_method = gethash(argv[argcnt])) < 0) {
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
		argcnt++;
	}

	/* set macfile */
	if (mfd) {
		if ( (macfile = (char *) strmac(keyfile)) == NULL) {
			release();
			return (-1);
		}
		if (! (mfd = open(macfile, O_CREAT, S_IRUSR | S_IWUSR))) {
			if (errno != EEXIST) {
				release();
				return (-1);
			}
		} else
			close(mfd);
	}

	/* save data on context */
	if ( (c = (struct om_peo_ctx*)
	    calloc(1, sizeof(struct om_peo_ctx))) == NULL) {
		release();
		return (-1);
	}

	c->size = sizeof(struct om_peo_ctx);
	c->hash_method = hash_method;
	c->keyfile = keyfile; 
	c->macfile = macfile;
	*ctx = (void *) c;

	snprintf(statbuf, sizeof(statbuf), "om_peo: method: "
	    "%d\nkeyfile: %s\nmacfile: %s\n", hash_method, keyfile,
	    macfile);
	*status = strdup(statbuf);

	return (1);
}


int
om_peo_close(struct filed *f, void *ctx)
{
	struct om_peo_ctx *c;

	c = (struct om_peo_ctx *) ctx;
	m_dprintf(MSYSLOG_INFORMATIVE, "om_peo_close\n");

	if (c->keyfile != default_keyfile)
		free(c->keyfile);
	if (c->macfile != NULL)
		free(c->macfile);
	return (0);
}

