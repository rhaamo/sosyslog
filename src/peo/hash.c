/*      $Id: hash.c,v 1.3 2000/04/28 17:59:48 claudio Exp $
 *
 * hash -- few things used by both peo output module and peochk 
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 */

#include <sys/types.h>
#include <md5.h>
#include <sha1.h>
#include <string.h>

#include "rmd160.h"
#include "hash.h"

char *default_keyfile = "/var/ssyslog/.var.log.messages";

typedef union {
	MD5_CTX 	md5;
	RMD160_CTX	rmd160;
	SHA1_CTX	sha1;
} HASH_CTX;

char *hmstr[] = { /* enum order */
	"md5",
	"rmd160",
	"sha1"
};


/*
 * hash:
 *	method:		hash method to use (see enum, gethash(..) output)
 *	lastkey:	buffer with last key
 *	llastkey:	lastkey buffer lenght
 *	data:		buffer with new data (string ending with \0)
 *	nkbuf:		new key buffer
 *
 * Returns the new key lenght and puts it on nkbuf as a string
 * The nkbuf should have enough space
 */
int
hash (method, lastkey, llastkey, data, nkbuf)
	int   method;
	char *lastkey;
	int   llastkey;
	char *data;
	char *nkbuf;
{
	HASH_CTX	ctx;
	int		len;

	switch(method) {
		case MD5:
			MD5Init(&ctx.md5);
			MD5Update(&ctx.md5, lastkey, llastkey);
			MD5Update(&ctx.md5, data, strlen(data));
			MD5End(&ctx.md5, nkbuf);
			len = 30;
			break;
		case RMD160:
			RMD160Init(&ctx.rmd160);
			RMD160Update(&ctx.rmd160, lastkey, llastkey);
			RMD160Update(&ctx.rmd160, data, strlen(data));
			RMD160End(&ctx.rmd160, nkbuf);
			len = 40;
			break;
		case SHA1:
		default:
			SHA1Init(&ctx.sha1);
			SHA1Update(&ctx.sha1, lastkey, llastkey);
			SHA1Update(&ctx.sha1, data, strlen(data));
			SHA1End(&ctx.sha1, nkbuf);
			len = 40;
			break;
	}

return len;
}


/*
 * gethash:
 *	Converts method string to method number.
 *	The string should be one of: "md5", "rmd160", or "sha1",
 *	if not -1 is returned instead.
 *	Case is ignored
 */
int
gethash (str)
	char *str;
{
	int i;
	for (i = 0; i < LAST_HASH; i++)
		if (!strcasecmp(str, hmstr[i]))
			return (i);

	return (-1);
}


/*
 * strkey:
 * 	receives something like this: /a/b/c/d/e
 *	and generates something like this: .a.b.c.d.e
 *	the new buffer should be freed using free(3)
 */
char *
strkey(logfile)
	char *logfile;
{
	char *keyfile;
	char *i;

	if ( (i = keyfile = strdup(logfile)) != NULL)
		while ( (i = strchr(i, '/')) != NULL)
			*i = '.';

	return keyfile;
}

