/*	$CoreSDI: sql_misc.c,v 1.5 2000/09/15 00:00:02 alejo Exp $	*/

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
 * sql_misc - Functions shared by SQL modules
 *
 * Author: Gerardo_Richarte@core-sdi.com
 *         Extracted from om_pgsql.c by Oliver Teuber (ot@penguin-power.de)
 *
 */

#include <stdlib.h>
#include <string.h>

char *
to_sql(s)
	char *s;
{
	char *p, *b;
	int ns;

	if (s == NULL)
		return NULL;
	
	for (p=s, ns=1+strlen(s); *p; p++) 
		if (*p=='\'') ns++;

	if (NULL==(b=malloc(ns))) return NULL;

	p=b; 
	for (;*s; s++) {
		if (*s=='\'') *p++='\\';
		*p++=*s;
	}
	*p=0;
	
	return b;
}

int
month_number(month_name)
	char *month_name;
{
	const char *Months[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
	int i=0;
	for (;i<sizeof(Months)/sizeof(Months[0]);i++)
		if (!strncmp(Months[i],month_name,3)) return i+1;
	return 0;
}
