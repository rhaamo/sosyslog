/*	$CoreSDI: modules.c,v 1.64 2000/05/24 21:22:16 fgsch Exp $	*/

/*
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

/*
 *  syslogd generic module functions --
 *
 * Author: Alejo Sanchez for Core-SDI S.A.
 */

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <syslog.h>
#include <stdio.h>
#include "syslogd.h"
#include "modules.h"

#include "config.h"

int om_peo_doLog(struct filed *, int, char *, struct om_hdr_ctx *);
int om_peo_init(int, char **, struct filed *, char *, struct om_hdr_ctx **);
int om_peo_close(struct filed *, struct om_hdr_ctx *);
int om_peo_flush(struct filed *, struct om_hdr_ctx *);

int om_classic_doLog(struct filed *, int , char *, struct om_hdr_ctx *);
int om_classic_init(int, char **, struct filed *, char *, struct om_hdr_ctx **);
int om_classic_close(struct filed*, struct om_hdr_ctx *);
int om_classic_flush(struct filed*, struct om_hdr_ctx *);

#ifdef HAVE_MYSQL
int om_mysql_doLog(struct filed *, int , char *, struct om_hdr_ctx *);
int om_mysql_init(int, char **, struct filed *, char *, struct om_hdr_ctx **);
int om_mysql_close(struct filed*, struct om_hdr_ctx *);
int om_mysql_flush(struct filed*, struct om_hdr_ctx *);
#endif /* HAVE_MYSQL */

int im_bsd_init(struct i_module *, char **, int);
int im_bsd_getLog(struct i_module *, struct im_msg *);
int im_bsd_close(struct i_module *);
int im_unix_init(struct i_module *, char **, int);
int im_unix_getLog(struct i_module *, struct im_msg *);
int im_unix_close(struct i_module *);
int im_udp_init(struct i_module *, char **, int);
int im_udp_getLog(struct i_module *, struct im_msg *);
int im_udp_close(struct i_module *);

void    die __P((int));

int parseParams(char ***, char *);

extern struct OModule OModules[];
extern struct IModule IModules[];

int
modules_load()
{
	/* initialize module function assignations */
	memset(OModules, 0, MAX_N_OMODULES);
	memset(IModules, 0, MAX_N_IMODULES);

	/* classic module */
	OModules[OM_CLASSIC].om_name 		= "classic";
	OModules[OM_CLASSIC].om_type 		= OM_CLASSIC;
	OModules[OM_CLASSIC].om_doLog 		= om_classic_doLog;
	OModules[OM_CLASSIC].om_init 		= om_classic_init;
	OModules[OM_CLASSIC].om_close 		= om_classic_close;
	OModules[OM_CLASSIC].om_flush 		= om_classic_flush;

#ifdef HAVE_MYSQL
	/* mysql module */
	OModules[OM_MYSQL].om_name 		= "mysql";
	OModules[OM_MYSQL].om_type 		= OM_MYSQL;
	OModules[OM_MYSQL].om_doLog	 	= om_mysql_doLog;
	OModules[OM_MYSQL].om_init 		= om_mysql_init;
	OModules[OM_MYSQL].om_close 		= om_mysql_close;
	OModules[OM_MYSQL].om_flush 		= om_mysql_flush;
#endif /* HAVE_MYSQL */
  
	/* peo module */
	OModules[OM_PEO].om_name		= "peo";
	OModules[OM_PEO].om_type		= OM_PEO;
	OModules[OM_PEO].om_doLog		= om_peo_doLog;
	OModules[OM_PEO].om_init		= om_peo_init;
	OModules[OM_PEO].om_close		= om_peo_close;
	OModules[OM_PEO].om_flush		= om_peo_flush;

	IModules[IM_BSD].im_name		= "bsd";
	IModules[IM_BSD].im_type		= IM_BSD;
	IModules[IM_BSD].im_init		= im_bsd_init;
	IModules[IM_BSD].im_getLog		= im_bsd_getLog;
  	IModules[IM_BSD].im_close		= im_bsd_close;

	IModules[IM_UNIX].im_name		= "unix";
	IModules[IM_UNIX].im_type		= IM_UNIX;
	IModules[IM_UNIX].im_init		= im_unix_init;
	IModules[IM_UNIX].im_getLog		= im_unix_getLog;
  	IModules[IM_UNIX].im_close		= im_unix_close;

	IModules[IM_UDP].im_name		= "udp";
	IModules[IM_UDP].im_type		= IM_UDP;
	IModules[IM_UDP].im_init		= im_udp_init;
	IModules[IM_UDP].im_getLog		= im_udp_getLog;
  	IModules[IM_UDP].im_close		= im_udp_close;

	return(1);
}

/* assign module functions to generic pointer */
int
modules_init (I, line)
	struct	 i_module *I;
	char	*line;
{
	int argc;
	char **argv, *p;
	struct i_module *im;

	/* create initial node for Inputs list */
	if (I == NULL) {
	    dprintf("modules_init: Error from caller\n");
	    return(-1);
	}

	for(im = I; im->im_next != NULL; im = im->im_next);
	if (im == I && im->im_fd > -1) {
		im->im_next = (struct i_module *) calloc(1, sizeof(struct i_module));
		im = im->im_next;
		im->im_fd = -1;
	}

	for(p = line;*p != '\0'; p++)
	    if (*p == ':')
	        *p = ' ';
	if ((argc = parseParams(&argv, line)) < 1) {
	    dprintf("Error initializing module %s [%s]\n", argv[0], line);
	    return(-1);
	}

	if (!strncmp(argv[0], "bsd", 3)) {
	    if (im_bsd_init(im, argv, argc) < 0) {
	    	dprintf("Error initializing module %s [%s]\n", argv[0], line);
	        die(0);
	    }

	} else if (!strncmp(argv[0], "unix", 4)) {
	    if (im_unix_init(im, argv, argc) < 0) {
	    	dprintf("Error initializing module %s [%s]\n", argv[0], line);
	        die(0);
	    }
	} else if (!strncmp(argv[0], "udp", 3)) {
	    if (im_udp_init(im, argv, argc) < 0) {
	    	dprintf("Error initializing module %s [%s]\n", argv[0], line);
	        die(0);
	    }
	}

	return(1);
}


/* close THIS input module */
int
im_close(im)
	struct i_module *im;
{
	return(IModules[im->im_type].im_close(im));
}

/* close ALL output modules of a specific filed */
int
om_close(f, ctx)
	struct filed *f;
	struct om_hdr_ctx *ctx;
{
	struct o_module *om;
	int ret;

	ret = 0;
	for(om = f->f_omod; om ; om = om->om_next) {
		if (OModules[om->om_type].om_close(f, om->ctx) < 0)
			ret--;
	}

	return(ret == 0? 1: ret);
}

/* create all necesary modules for a specific filed */
int omodule_create(c, f, prog)
	char *c;
	struct filed *f;
	char *prog;
{
	char	*line, *p, quotes, *argv[20];
	int	argc, i;
	struct o_module	*m, *prev;

	line = strdup(c); quotes = 0;
	p = line; prev = NULL;

	/* create context and initialize module for logging */
	while (*p) {
		if (f->f_omod == NULL) {
			f->f_omod = (struct o_module *) calloc(1, sizeof *f->f_omod);
			m = f->f_omod;
			prev = NULL;
		} else {
			for (prev = f->f_omod; prev->om_next; prev = prev->om_next);
			prev->om_next = (struct o_module *) calloc(1, sizeof *f->f_omod);
			m = prev->om_next;
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
		if ((OModules[m->om_type].om_init)(argc, argv, f,
				prog, (void *) &(m->ctx)) < 0) {
			free(m);
			m = NULL;
			if (prev == NULL) {
				f->f_omod = NULL;
			} else {
				prev->om_next = NULL;
			}
		}
	}
	free(line);
	return(1);
}


/*
 *
 * Input modules
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

/*
 * Parse a line and return argc & argv
 *
 * space and tabs are separators
 *
 */

int
parseParams(ret, c)
	char ***ret;
	char *c;
{
	char	*line, *p, *q;
	int	argc;

	line = strdup(c); p = line;

	/* initialize arguments before starting */
	*ret = (char **) calloc(20, sizeof(char *));
	argc = 0;

	for(q = p; *p != '\0'; p = q) {
		/* skip initial spaces */
		while (isspace(*p)) p++;
		if (*p == '\0')
		    break;

		if (*p == '\'') {
		    for(q = ++p; *q != '\'' && *q != '\0'; q++);

		    if (*q != '\0') {
		        *q++ = '\0';
		    }

		} else {
		    /* see how long this word is, alloc, and copy */
		    for(q = p; *q != '\0' && !isspace(*q); q++);
		    if (*q != '\0') {
		        *q++ = '\0';
		    }

		}

		(*ret)[argc++] = strdup(p);
		if ((argc % 20) == 18)
		    *ret = (char **) realloc(*ret, sizeof(char *) * (argc + 20));
		(*ret)[argc] = NULL;
	}
	
	free(line);
	return(argc);
}
