/*	$CoreSDI: modules.c,v 1.109 2000/07/04 19:02:32 claudio Exp $	*/

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
 *  syslogd generic module functions --
 *
 * Authors: Alejo Sanchez for Core-SDI S.A.
 *          Federico Schwindt for Core-SDI S.A.
 */

#include "config.h"

#include <sys/param.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include "syslogd.h"
#include "modules.h"

int     modules_load(void);
int     imodule_init(struct i_module *, char *);
int     omodule_create(char *, struct filed *, char *);
void    logerror(char *);
void    logmsg(int, char *, char *, int);
void    die(int);

int	parseParams(char ***, char *);
struct imodule *getImodule(char *);
struct omodule *getOmodule(char *);
struct imodule *addImodule(char *);
struct omodule *addOmodule(char *);

struct omodule *omodules;
struct imodule *imodules;

extern struct sglobals *sglobals;

/* assign module functions to generic pointer */
int
imodule_init (struct i_module *I, char *line) {
	int argc;
	char **argv, *p;
	struct i_module *im, *im_prev;

	/* create initial node for Inputs list */
	if (I == NULL) {
	    dprintf("imodule_init: Error from caller\n");
	    return(-1);
	}

	for (im_prev = I; im_prev->im_next != NULL; im_prev = im_prev->im_next);
	if (im_prev == I && im_prev->im_fd > -1) {
		if((im_prev->im_next = (struct i_module *) calloc(1,
		        sizeof(struct i_module))) == NULL) {
		    dprintf("No memory available for calloc\n");
		    return(-1);
		}
		im = im_prev->im_next;
		im->im_fd = -1;
		im->im_prev = im_prev;
	} else {
		im = im_prev;
		im->im_prev = NULL;
	}

	for (p = line; *p != '\0'; p++)
	    if (*p == ':')
	        *p = ' ';

	if ((argc = parseParams(&argv, line)) < 1) {
	    dprintf("Error initializing module %s [%s]\n", argv[0], line);
	    return(-1);
	}

	/* is it already initialized ? searching... */
	if ((im->im_func = getImodule(argv[0])) == NULL)
		if ((im->im_func = addImodule(argv[0])) == NULL) {
	   		dprintf("Error loading dynamic input module %s [%s]\n", argv[0], line);
			die(0);
		}

	/* got it, now try to initialize it */
	if ((*(im->im_func->im_init))(im, argv, argc, sglobals) < 0) {
	   	dprintf("Error initializing input module %s [%s]\n", argv[0], line);
		die(0);
	}

	return(1);
}

/* create all necesary modules for a specific filed */
int
omodule_create(char *c, struct filed *f, char *prog) {
	char	*line, *p, quotes, *argv[20];
	int	argc;
	struct o_module	*om, *om_prev;

	line = strdup(c); quotes = 0;
	p = line;

	/* create context and initialize module for logging */
	while (*p) {
		if (f->f_omod == NULL) {
			f->f_omod = (struct o_module *) calloc(1,
			    	sizeof(*f->f_omod));
			om = f->f_omod;
			om_prev = NULL;
		} else {
			for (om_prev = f->f_omod; om_prev->om_next; om_prev = om_prev->om_next);
			om_prev->om_next = (struct o_module *) calloc(1, sizeof *f->f_omod);
			om = om_prev->om_next;
			om->om_prev = om_prev;
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
				if ((om->om_func = getOmodule(argv[0])) == NULL) {
					if ((om->om_func = addOmodule(argv[0])) != NULL) {
				   		dprintf("Error loading dynamic output module "
								"%s [%s]\n", argv[0], line);
						die(0);
					}
				}

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
				argv[argc++]="classic";
				argv[argc++]=p;
				p+=strlen(p);
				/* find for matching module */
				if ((om->om_func = getOmodule(argv[0])) == NULL) {
					if ((om->om_func = addOmodule(argv[0])) == NULL) {
				   		dprintf("Error loading dynamic output module "
								"%s [%s]\n", argv[0], line);
						die(0);
					}
				}

				break;
		}
		if ((*(om->om_func->om_init))(argc, argv, f,
				prog, (void *) &(om->ctx), sglobals) < 0) {
			dprintf("Error initializing dynamic output module "
								"%s [%s]\n", argv[0], line);
			return(-1);
		}
	}
	free(line);
	if (f->f_type == F_UNUSED)
		f->f_type = F_MODULE;
	return(1);
}

/*
 * Parse a line and return argc & argv
 *
 * space and tabs are separators
 *
 */

int
parseParams(char ***ret, char *c)
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

struct imodule *
addImodule(name)
	char *name;
{
	struct imodule *im;
	char buf[128];

	if (name == NULL)
		return(NULL);

	if (imodules == NULL) {
		imodules = (struct imodule *) calloc(1, sizeof(*im));
		im = imodules;
	} else {
		for(im = imodules; im->im_next; im = im->im_next);
		im->im_next = (struct imodule *) calloc(1, sizeof(*im));
		im = im->im_next;
	}

	snprintf(buf, 127, "libmsyslog_im_%s.so.%1.1f", name, VERSION);

	if ((im->h = dlopen(buf, RTLD_LAZY)) == NULL) {
	   	dprintf("Error [%s] on file [%s]\n", dlerror(), buf);
	   	return(NULL);
	}

	snprintf(buf, 127, "_im_%s_init", name);
	if ((im->im_init = dlsym(im->h, buf)) == NULL) {
	   	dprintf("addImodule: error linking %s function \n", buf);
	   	return(NULL);
	}

	snprintf(buf, 127, "_im_%s_getLog", name);
	if ((im->im_getLog = dlsym(im->h, buf)) == NULL) {
	   	dprintf("addImodule: error linking %s function \n", buf);
	   	return(NULL);
	}

	/* this one could be null */
	snprintf(buf, 127, "_im_%s_close", name);
	im->im_close = dlsym(im->h, "im_close");

	im->im_name = strdup(name);

	return(im);

}

int
imoduleDestroy(im)
	struct imodule *im;
{
	if (im == NULL || im->h == NULL || im->im_next)
		return(-1);

	if (dlclose(im->h) < 0) {
	   	dprintf("Error [%s]\n", dlerror());
		return(-1);
	}
	free(im->im_name);

	return(1);
}

struct omodule *
addOmodule(char *name) {
	struct omodule *om;
	char buf[256];

	if (name == NULL)
		return(NULL);

	if (omodules == NULL) {
		omodules = (struct omodule *) calloc(1, sizeof(*om));
		om = omodules;
	} else {
		for(om = omodules; om->om_next; om = om->om_next);
		om->om_next = (struct omodule *) calloc(1, sizeof(*om));
		om = om->om_next;
	}

	snprintf(buf, 127, "libmsyslog_om_%s.so.%1.1f", name, VERSION);

	if ((om->h = dlopen(buf, RTLD_LAZY)) == NULL) {
	   	dprintf("Error [%s] on file [%s]\n", dlerror(), buf);
	   	return(NULL);
	}

	snprintf(buf, 127, "_om_%s_init", name);
	if ((om->om_init = dlsym(om->h, buf)) == NULL) {
	   	dprintf("addOmodule: error linking %s function \n", buf);
	   	return(NULL);
	}
	snprintf(buf, 127, "_om_%s_doLog", name);
	if ((om->om_doLog = dlsym(om->h, buf)) == NULL) {
	   	dprintf("addOmodule: error linking %s function \n", buf);
	   	return(NULL);
	}

	/* this ones could be null */
	snprintf(buf, 127, "_om_%s_close", name);
	om->om_close = dlsym(om->h, "om_close");
	snprintf(buf, 127, "_om_%s_flush", name);
	om->om_close = dlsym(om->h, "om_close");

	om->om_name = strdup(name);

	return(om);

}

int
omoduleDestroy(om)
	struct omodule *om;
{
	if (om == NULL || om->h == NULL || om->om_next)
		return(-1);

	if (dlclose(om->h) < 0) {
	   	dprintf("Error [%s]\n", dlerror());
		return(-1);
	}

	free(om->om_name);

	return(1);
}

struct imodule *
getImodule(name)
	char *name;
{
	struct imodule *im;
	int len;

	if (imodules == NULL || name == NULL)
		return(NULL);

	for(im = imodules, len = strlen(name); im; im = im->im_next)
		if (!strncmp(im->im_name, name, len))
			break;

	return(im);
}

struct omodule *
getOmodule(name)
	char *name;
{
	struct omodule *om;
	int len;

	if (omodules == NULL || name == NULL)
		return(NULL);

	for(om = omodules, len = strlen(name); om; om = om->om_next)
		if (!strncmp(om->om_name, name, len))
			break;

	return(om);
}


