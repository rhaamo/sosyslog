/*      $Id: hash.c,v 1.6 2000/05/04 00:19:13 claudio Exp $
 *
 * hash -- few things used by both peo output module and peochk 
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 */

#include "../config.h"

#include <sys/types.h>

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_OPENBSD
	#include <md5.h>
	#include <sha1.h>
#else
	#include "md5.h"
	#include "sha1.h"
#endif

#include "rmd160.h"
#include "hash.h"


char *default_keyfile = "/var/ssyslog/.var.log.messages";

char *hmstr[] = { /* enum order */
	"md5",
	"rmd160",
	"sha1"
};

typedef union {
	MD5_CTX 	md5;
	RMD160_CTX	rmd160;
	SHA1_CTX	sha1;
} HASH_CTX;


/*
 * mac:
 *	method:		hash method to use (see enum, gethash(..) output)
 *	data1:		buffer 1 (commonly a key[i])
 *	data1len:	data1 lenght
 *	data2:		buffer 2 (commonly a message)
 *	data2len:	data2 lenght
 *	dest:		destination buffer
 *
 *	Fills dest with a key converted to string (including final \0)
 *	and returns dest lenght on (without the final \0).
 *	dest should have enough space.
 *	On error returns -1
 */
int
mac (method, data1, data1len, data2, data2len, dest)
	int		 method;
	const char	*data1;
	int		 data1len;
	const char	*data2;
	int		 data2len;
	char		*dest;
{
	HASH_CTX	 ctx;
	int		 destlen;
	int		 i;
	char		*tmp;
	int		 tmplen;

	/* calculate tmp buffer lenght */
	if (data1len && data2len) {
		if (data1len > data2len) {
			if ( (tmplen = data1len / data2len * data2len) < data1len)
				tmplen += data2len;
		}
		else if ( (tmplen = data2len / data1len * data1len) < data2len)
			tmplen += data1len;
	}
	else if ( (tmplen = (data1len) ? data1len : data2len) == 0)
		tmplen = 1; 
	
	/* allocate needed memory and clear tmp buffer */
	if ( (tmp = (char*) calloc (1, tmplen)) == NULL)
		return (-1);
	bzero(tmp, tmplen);

	/* tmp = data1 xor data2 */
	if (data1len && data2len)
		for (i = 0; i < tmplen; i++)
			tmp[i] = data1[i % data1len] ^ data2[i % data2len];
	else if (data1len)
		memcpy(tmp, data1, tmplen);
	else
		memcpy(tmp, data2, tmplen);

	/* dest = hash(tmp) */
	switch(method) {
		case MD5:
			MD5Init(&ctx.md5);
			MD5Update(&ctx.md5, tmp, tmplen);
			MD5End(&ctx.md5, dest);
			destlen = 32;
			break;
		case RMD160:
			RMD160Init(&ctx.rmd160);
			RMD160Update(&ctx.rmd160, tmp, tmplen);
			RMD160End(&ctx.rmd160, dest);
			destlen = 40;
			break;
		case SHA1:
		default:
			SHA1Init(&ctx.sha1);
			SHA1Update(&ctx.sha1, tmp, tmplen);
			SHA1End(&ctx.sha1, dest);
			destlen = 40;
			break;
	}
		
	free(tmp);
	return (destlen);
}


/*
 * gethash:
 *	Converts method string to method number.
 *	The string should be one of those specified in hmstr declaration,
 *	otherwise -1 is returned.
 *	Case is ignored.
 */
int
gethash (str)
	const char *str;
{
	int i;

	for (i = 0; i < LAST_HASH; i++)
		if (!strcasecmp(str, hmstr[i]))
			return (i);

	return (-1);
}


/*
 * strkey:
 * 	Receives something like this: /a/b/c/d/e
 *	and generates something like this: .a.b.c.d.e
 *	The new buffer should be freed using free(3)
 */
char*
strkey (logfile)
	const char *logfile;
{
	char *b;
	char *keyfile;

	if ( (b = keyfile = strdup(logfile)) != NULL)
		while ( (b = strchr(b, '/')) != NULL)
			*b = '.';

	return (keyfile);
}


/*
 * mac2:
 *	data1:		buffer 1 (commonly key[i])
 *	data1len:	data1 lenght
 *	data2:		buffer 2 (commonly message)
 *	data2len:	data2 lenght
 *	dest:		destination buffer (commonly key[i+1])
 *
 *	Fills dest with a digest converted to string (including final \0)
 *	and returns dest lenght (without the final \0).
 *	dest should have enough space.
 *	On error returns -1
 */
int
mac2 (data1, data1len, data2, data2len, dest)
	const char	*data1;
	int		 data1len;
	const char	*data2;
	int		 data2len;
	char		*dest;
{
	int destlen;

	if ( (destlen = mac(SHA1, data1, data1len, data2, data2len, dest)) != -1)
		destlen = mac(RMD160, dest, destlen, data2, data2len, dest);

	return (destlen);
}


/*
 * strmac:
 *	generates macfile name based on keyfile name
 *	macfile = keyfile + ".mac"
 *	the new buffer should be freed using free(3)
 */
char*
strmac (keyfile)
	const char *keyfile;
{
	char *macfile;

	if ( (macfile = (char*) calloc(1, strlen(keyfile)+4)) != NULL) {
		strcpy(macfile, keyfile);
		strcat(macfile, ".mac");
	}

	return (macfile);
}


