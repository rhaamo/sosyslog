/*	$Id: sql_misc.c,v 1.13 2002/09/17 05:20:28 alejo Exp $	*/

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
 * sql_misc - Functions shared by SQL modules
 *
 * Author: Gerardo_Richarte@corest.com
 *         Extracted from om_pgsql.c by Oliver Teuber (ot@penguin-power.de)
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>

int
to_sql(char *dst, char *src, int maxlen)
{
	int i;

	if(dst == NULL || src == NULL || maxlen < 2)
		return -1;

	for(i = 0; *src && i < (maxlen - 2) ; src++) {

		/*
		 * escape \n \r \\ \' " and del (ctrl-z 127)
		 */

		if (*src == '\'' || *src == '\n' || *src == '\r' ||
		    *src == '\\' || *src == '"' || *src == 127) {
			dst[i++] = '\\';
		}

		dst[i++] = *src;

	}

	/* terminate string if possible */
	if (i < maxlen)
		dst[i] = 0;
	
	return i;
}

