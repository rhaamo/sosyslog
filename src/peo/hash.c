/*	$CoreSDI: hash.c,v 1.20 2000/05/29 17:35:40 claudio Exp $	*/

/*
 * hash -- few things used by both peo output module and peochk 
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 */

#include "../config.h"

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_OPENBSD
	#include <md5.h>
	#include <rmd160.h>
	#include <sha1.h>
	#define RANDOM_DEVICE	"/dev/srandom"
#else
	#include "md5.h"
	#include "rmd160.h"
	#include "sha1.h"
	#define RANDOM_DEVICE	"/dev/random"
#endif

#include "hash.h"


char *default_keyfile = "/var/ssyslog/.var.log.messages.key";

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
 *	Fills dest with a key and returns dest lenght
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
		if (data1len > data2len)
			tmplen += ((tmplen = data1len/data2len*data2len) < data1len) ? data2len:0;
		else if (data1len < data2len)
			tmplen += ((tmplen = data2len/data1len*data1len) < data2len) ? data1len:0;
		else tmplen = data1len;
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
			MD5Final(dest, &ctx.md5);
			destlen = 16;
			break;
		case RMD160:
			RMD160Init(&ctx.rmd160);
			RMD160Update(&ctx.rmd160, tmp, tmplen);
			RMD160Final(dest, &ctx.rmd160);
			destlen = 20;
			break;
		case SHA1:
		default:
			SHA1Init(&ctx.sha1);
			SHA1Update(&ctx.sha1, tmp, tmplen);
			SHA1Final(dest, &ctx.sha1);
			destlen = 20;
			break;
	}
		
	free(tmp);
	return (destlen);
}


/*
 * mac2:
 *	data1:		buffer 1 (commonly key[i])
 *	data1len:	data1 lenght
 *	data2:		buffer 2 (commonly message)
 *	data2len:	data2 lenght
 *	dest:		destination buffer (commonly key[i+1])
 *
 *	Fills dest with a digest and returns dest lenght
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
 * strdot:
 * 	Receives something like this: /a/b/c/d/e
 *	and chages it to something like this: .a.b.c.d.e
 */
char*
strdot (s)
	char *s;
{
	char *b;

	if ( (b = s) != NULL)
		while ( (b = strchr(b, '/')) != NULL)
			*b = '.';
	return (s);
}


/*
 * strallocat:
 *	Concatenates two strings and returns a pointer to the new string
 *	The new buffer should be freed using free(3)
 */
char*
strallocat (s1, s2)
	const char *s1;
	const char *s2;
{
	char *dest;
	int   size;

	if ( (dest = (char*) calloc(1, (size = strlen(s1) + strlen(s2) + 1))) != NULL)
		snprintf (dest, size, "%s%s", (s1) ? s1 : "", (s2) ? s2 : "");

	return (dest);
}


/*
 * strmac
 */
char *strmac (s)
	const char *s;
{
return strallocat(s, ".mac");
}


/*
 * strkey0
 */
char *strkey0 (s)
	const char *s;
{
return strallocat(s, "0");
}


/*
 * asc2bin:
 *	Translates a hex string to binary buffer
 *	Buffer lenght = string lenght / 2
 *	(2 byte string "ab" is translated to 1 byte buffer 0xab)
 */

#define ASC2BIN(x)	((x <= '9') ? x-'0' : x-'a'+10)

unsigned char*
asc2bin (dst, src)
	unsigned char       *dst;
	const unsigned char *src;
{
	int   		 i;
	int   		 j;
	unsigned char	*tmp;

	if (src == NULL || dst == NULL || (strlen(src) & 1))
		return (NULL);

	if (dst == src) {
		if ( (tmp = strdup(src)) == NULL)
			return (NULL);
	} else
		tmp = (unsigned char*) src;

	for (j = i = 0; tmp[i] != '\0'; i+=2, j++)
		dst[j] = (ASC2BIN(tmp[i]) << 4) | ASC2BIN(tmp[i+1]);

	if (dst == src)
		free(tmp);

	return (dst);
}


/*
 * bin2asc:
 *	Translates a binary buffer to string
 *	Based on XXXEnd function
 *	(2 byte buffer 0x3, 0x9a is translated to 4 byte string "039a")
 */

char hex[] = { "0123456789abcdef" };

unsigned char*
bin2asc (dst, src, srclen)
	unsigned char		*dst;
	const unsigned char	*src;
	int         		 srclen;
{
	int i;

	if (dst == NULL || src == NULL)
		return (NULL);

    	for (i = 0; i < srclen; i++) {
        	dst[i + i] = hex[src[i] >> 4];
        	dst[i + i + 1] = hex[src[i] & 0x0f];
    	}
    	dst[i + i] = '\0';

	return (dst);
}


/*
 * getrandom:
 *	Open RANDOM_DEVICE and reads len bytes random values.
 *	Returns 0 on success and -1 on error
 */
int
getrandom (buffer, bytes)
	char *buffer;
	int   bytes;
{
	int fd;

	if ( (fd = open(RANDOM_DEVICE, O_RDONLY, 0)) >= 0) {
		if (read(fd, buffer, bytes) == bytes)
			bytes = 0;
		else
			bytes = -1;

		close(fd);
		return (bytes);
	}

	return (-1);
}


