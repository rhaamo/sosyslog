/*	$CoreSDI: hash.c,v 1.23.2.3.2.1.4.1 2000/10/12 02:13:34 fgsch Exp $	*/
 
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
#include <limits.h>
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
mac (int method, const char *data1, unsigned int data1len,
     const char *data2, unsigned int data2len, char *dest)
{
	HASH_CTX	 ctx;
	int		 destlen, i, tmplen = 0;
	char		*tmp;

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
		MD5Update(&ctx.md5, (unsigned char*) tmp, tmplen);
		MD5Final((unsigned char*)dest, &ctx.md5);
		destlen = 16;
		break;
	case RMD160:
		RMD160Init(&ctx.rmd160);
		RMD160Update(&ctx.rmd160, (unsigned char*) tmp, tmplen);
		RMD160Final((unsigned char*) dest, &ctx.rmd160);
		destlen = 20;
		break;
	case SHA1:
	default:
		SHA1Init(&ctx.sha1);
		SHA1Update(&ctx.sha1, (unsigned char*) tmp, tmplen);
		SHA1Final((unsigned char*) dest, &ctx.sha1);
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
mac2 (const char *data1, int data1len,
      const char *data2, int data2len, char *dest)
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
gethash (const char *str)
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
strdot (char *s)
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
strallocat (const char *s1, const char *s2)
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
char*
strmac (const char *s)
{
	return strallocat(s, ".mac");
}


/*
 * strkey0
 */
char*
strkey0 (const char *s)
{
	return strallocat(s, "0");
}


/*
 * stresolvepath
 */
char*
strrealpath (const char *path)
{
	char *resolved;

	if ( (resolved = (char*)calloc(1, PATH_MAX)) != NULL)
		return realpath(path, resolved);
	return(NULL);
}


/*
 * asc2bin:
 *	Translates a hex string to binary buffer
 *	Buffer lenght = string lenght / 2
 *	(2 byte string "ab" is translated to 1 byte buffer 0xab)
 */

#define ASC2BIN(x)	((x <= '9') ? x-'0' : x-'a'+10)

unsigned char*
asc2bin (unsigned char *dst, const unsigned char *src)
{
	int   		 i;
	int   		 j;
	unsigned char	*tmp;

	if (src == NULL || dst == NULL || (strlen((char*) src) & 1))
		return (NULL);

	if (dst == src) {
		if ( (tmp = (unsigned char*) strdup((char*) src)) == NULL)
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
bin2asc (unsigned char *dst, const unsigned char *src, int srclen)
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
getrandom (char *buffer, int bytes)
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


