/*	$CoreSDI: hash.h,v 1.17 2001/03/07 21:35:16 alejo Exp $
 */
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
 * hash -- Some things used by both peo output module and peochk
 *
 * Author: Claudio Castiglia
 *
 */

#ifndef HASH_H
#define HASH_H

enum {
	MD5,
	RMD160,
	SHA1,
	LAST_HASH
};

extern char *default_keyfile;

extern int		mac (int, const unsigned char *, unsigned int,
			     const unsigned char *, unsigned int,
			     unsigned char *);
extern int		mac2 (const unsigned char *, int,
			      const unsigned char *, int, unsigned char *);
extern int		gethash (const char *);
extern char	       *strdot (char *);
extern char	       *strallocat (const char*, const char *);
extern char	       *strmac (const char *);
extern char	       *strkey0 (const char *);
extern char	       *strrealpath (const char *);
extern unsigned char   *asc2bin (unsigned char *, const unsigned char *);
extern unsigned char   *bin2asc (unsigned char *, const unsigned char *, int);
extern int		getrandom (unsigned char *, int);

#endif

