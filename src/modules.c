/*	$Id: modules.c,v 1.33 2000/04/25 01:32:56 alejo Exp $
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

#include "config.h"

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

int om_classic_doLog(struct filed *, int , char *, struct om_header_ctx *);
int om_classic_init(int, char **, struct filed *, char *, struct om_header_ctx **);
int om_classic_close(struct filed*, struct om_header_ctx **);
int om_classic_flush(struct filed*, struct om_header_ctx *);

int om_mysql_doLog(struct filed *, int , char *, struct om_header_ctx *);
int om_mysql_init(int, char **, struct filed *, char *, struct om_header_ctx **);
int om_mysql_close(struct filed*, struct om_header_ctx **);
int om_mysql_flush(struct filed*, struct om_header_ctx *);
int im_bsd_init(struct i_module *,int , char **, struct im_bsd_ctx *);
int im_bsd_getLog(struct i_module *, struct im_msg *);
int im_bsd_close(struct i_module *);

/* assign module functions to generic pointer */
int modules_init (I, inputs)
	struct i_module **I;
	int		inputs;
{
	/* initialize module function assignations */
	memset(OModules, 0, sizeof(OModules));
	*I = (struct i_module *) calloc(sizeof(struct i_module), 1);
	(*I)->fd = -1;

	/* classic module */
	/* classic module */
	OModules[OM_CLASSIC].om_name 		= "classic";
	OModules[OM_CLASSIC].om_type 		= OM_CLASSIC;
	OModules[OM_CLASSIC].om_doLog 		= om_classic_doLog;
	OModules[OM_CLASSIC].om_init 		= om_classic_init;
	OModules[OM_CLASSIC].om_close 		= om_classic_close;
	OModules[OM_CLASSIC].om_flush 		= om_classic_flush;

#ifndef HAVE_LINUX
	/* mysql module */
	OModules[OM_MYSQL].om_name 		= "mysql";
	OModules[OM_MYSQL].om_type 		= OM_MYSQL;
	OModules[OM_MYSQL].om_doLog	 	= om_mysql_doLog;
	OModules[OM_MYSQL].om_init 		= om_mysql_init;
	OModules[OM_MYSQL].om_close 		= om_mysql_close;
	OModules[OM_MYSQL].om_flush 		= om_mysql_flush;
#endif
  
	IModules[IM_BSD].im_name		= "bsd";
	IModules[IM_BSD].im_type		= IM_BSD;
	IModules[IM_BSD].im_init		= im_bsd_init;
	IModules[IM_BSD].im_getLog		= im_bsd_getLog;
	IModules[IM_BSD].im_close		= im_bsd_close;
 #if 0 
	IModules[IM_SYSV].im_name		= "sysv";
	IModules[IM_SYSV].im_type		= IM_SYSV;
	IModules[IM_SYSV].im_init		= im_sysv_init;
	IModules[IM_SYSV].im_getLog		= im_sysv_getLog;
	IModules[IM_SYSV].im_close		= im_sysv_close;
#endif

	*I = (struct i_module *) calloc(1, sizeof(struct i_module));
	if (inputs & INPUT_BSD)
		im_bsd_init(*I);
#if 0
	if (inputs & INPUT_SYSV)
		im_sysv_init(I);
#endif

	return(1);
}


/* close all modules of a specific filed */
int modules_close(f)
	struct filed *f;
{
	/* close all modules */
	return(-1);
}

/* create all necesary modules for a specific filed */
int omodule_create(c, f, prog)
	char *c;
	struct filed *f;
	char *prog;
{
	char	*line, *p;
	char	*argv[20];
	int	argc;
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
				while (isspace(*(++p)));
				argv[argc++] = p;
				while (!isspace(*p)) p++;

				*p++=0;

				/* find for matching module */
				for (i = 0; i < MAX_N_OMODULES; i++) {
					if (OModules[i].om_name == NULL)
						continue;
					if (strncmp(argv[0], OModules[i].om_name,
							MAX_MODULE_NAME_LEN) == 0)
						break;
				}
				/* no matching module */
				if (i == MAX_N_OMODULES)
					return(-1);

				m->om_type = OModules[i].om_type;

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
				m->om_type = OM_CLASSIC;
				break;
		}
		(OModules[m->om_type].om_init)(argc, argv, f, prog, (void *) &(m->context));
	}
	free(line);
	return(1);
}

char *getomodulename(type)
	int type;
{
	int i;

	for(i = 0; i < MAX_N_OMODULES && OModules[i].om_type != type; i++);
	if (i == MAX_N_OMODULES)
		return (NULL);
	return OModules[i].om_name;
}

int getomoduleid(mname)
	char *mname;
{
	register int i;

	for(i = 0; i < MAX_N_OMODULES && OModules[i].om_name != NULL
			&& !strcmp(OModules[i].om_name, mname); i++);
	if (i == MAX_N_OMODULES)
		return (-1);
	return OModules[i].om_type;
}


/*
 *
 * Input modules
 *
 *
 *
 *
 *
 */

/* create all necesary modules for a specific input */
int imodule_create(c, f, prog)
	char *c;
	struct filed *f;
	char *prog;
{
	return(-1);
}
