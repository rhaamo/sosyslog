/*	$Id: modules.c,v 1.19 2000/04/07 19:53:48 alejo Exp $
 * Copyright (c) 1983, 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

/*
 *  syslogd generic module functions --
 *
 * Author: Alejo Sanchez for Core-SDI S.A.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/syslog.h>
#include "syslogd.h"
#include "modules.h"

/* assign module functions to generic pointer */
int modules_init ()
{
	/* initialize module function assignations */
	memset(Modules, 0, sizeof(Modules));

	/* classic module */
	/* classic module */
	Modules[M_CLASSIC].om_name 		= "classic";
	Modules[M_CLASSIC].om_type 		= M_CLASSIC;
	Modules[M_CLASSIC].om_doLog 		= om_classic_doLog;
	Modules[M_CLASSIC].om_init 		= om_classic_init;
	Modules[M_CLASSIC].om_close 		= om_classic_close;
	Modules[M_CLASSIC].om_flush 		= om_classic_flush;

	/* mysql module */
	Modules[M_MYSQL].om_name 		= "mysql";
	Modules[M_MYSQL].om_type 		= M_MYSQL;
	Modules[M_MYSQL].om_doLog	 	= om_mysql_doLog;
	Modules[M_MYSQL].om_init 		= om_mysql_init;
	Modules[M_MYSQL].om_close 		= om_mysql_close;
	Modules[M_MYSQL].om_flush 		= om_mysql_flush;

}


/* close all modules of a specific filed */
int modules_close(f)
	struct filed *f;
{
	/* close all modules */
}

/* create all necesary modules for a specific filed */
int omodule_create(c, f, prog)
	char *c;
	struct filed *f;
	char *prog;
{
	char	*line, *p;
	char	*argv[20];
	char	argc;
	struct o_module		*m;
	char quotes=0;
	int	i;

	line = strdup(c);
	p = line;

	/* create context and initialize module for logging */
	while (*p) {
		if (f->f_mod == NULL) {
			f->f_mod = (struct o_module *) calloc(1, sizeof *f->f_mod);
			m = f->f_mod;
		} else {
			for (m = f->f_mod; m->om_next; m = m->om_next);
			m->om_next = (struct o_module *) calloc(1, sizeof *f->f_mod);
			m = m->om_next;
		}

		switch (*p) {
			case '%':
				/* get this module name */
				argc=0;
				while (isspace(*++p));
				argv[argc++] = p;
				while (!isspace(*p)) p++;

				*p++=0;

				/* find for matching module */
				for (i = 0; i < MAX_N_MODULES; i++) {
					if (Modules[i].om_name == NULL)
						continue;
					if (strncmp(argv[0], Modules[i].om_name,
							MAX_MODULE_NAME_LEN) == 0)
						break;
				}
				/* no matching module */
				if (i == MAX_N_MODULES)
					return(-1);

				m->om_type = Modules[i].om_type;

				/* build argv and argc, modifies input p */
				while (isspace(*p)) p++;
				while (*p && *p!='%' && *p !='\n' && *p!='\r'
						&& argc<sizeof(argv)/sizeof(argv[0])) { 
				
					(*p=='"' || *p=='\'')? quotes = *p++ : 0;
						
					argv[argc++] = p;
					if (quotes) while (*p != quotes) p++;
						else while ( *p != '\0' && !isspace(*p)) p++;
					if (*p == '\0')
						break;
					*p++ = 0;
					while (*p != '\0' && isspace(*p)) p++;
				}


				break;
			case '@':
			case '/':
			case '*':
			default:
				/* classic style */
				/* prog is already on this filed */
				argc=0;
				argv[argc++]="auto_classic";
				argv[argc++]=p;
				p+=strlen(p);
				m->om_type = M_CLASSIC;
				break;
		}
		(Modules[m->om_type].om_init)(argc, argv, f, prog, (void *) &(m->context));
	}
	free(line);
}

char *getmodulename(type)
	int type;
{
	int i;

	for(i = 0; i < MAX_N_MODULES && Modules[i].om_type != type; i++);
	if (i == MAX_N_MODULES)
		return (NULL);
	return Modules[i].om_name;
}

int getmoduleid(mname)
	char *mname;
{
	int i;

	for(i = 0; i < MAX_N_MODULES && Modules[i].om_name != NULL
			&& !strcmp(Modules[i].om_name, mname); i++);
	if (i == MAX_N_MODULES)
		return (-1);
	return Modules[i].om_type;
}

