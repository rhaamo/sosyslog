/* sha1hl.c
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dkuug.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 */

/* md5hl.c
 * Converted to MD5, only MD5End
 */

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include "md5.h"

/* ARGSUSED */
char *
MD5End(ctx, buf)
    MD5_CTX *ctx;
    char *buf;
{
    int i;
    char *p = buf;
    u_char digest[16];
    static const char hex[]="0123456789abcdef";

    if (p == NULL && (p = malloc(33)) == NULL)
	return 0;

    MD5Final(digest,ctx);
    for (i = 0; i < 16; i++) {
	p[i + i] = hex[digest[i] >> 4];
	p[i + i + 1] = hex[digest[i] & 0x0f];
    }
    p[i + i] = '\0';
    return(p);
}


