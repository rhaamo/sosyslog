/*	$CoreSDI: modules.c,v 1.160 2001/05/03 22:44:58 alejo Exp $	*/

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
 *  syslogd generic module functions 
 *
 * Authors: Alejo Sanchez for Core-SDI S.A.
 *          Federico Schwindt for Core-SDI S.A.
 */

#include "config.h"

#include <sys/param.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include <limits.h>
#include <time.h>
#include "modules.h"
#include "syslogd.h"

#ifdef _POSIX_PATH_MAX
#define LIB_PATH_MAX _POSIX_PATH_MAX
#else
#define LIB_PATH_MAX 254
#endif

void    logerror(char *);

int	parseParams(char ***, char *);
struct imodule *getImodule(char *);
struct omodule *getOmodule(char *);
struct imodule *addImodule(char *);
struct omodule *addOmodule(char *);

struct omodule *omodules;
struct imodule *imodules;

extern char *libdir;
extern int Debug;
char err_buf[MAXLINE];

extern void *main_lib;


/*
 * Prepare libraries for a module
 */

int
prepare_module_libs(const char *modname, void **ret) {
#if BUGGY_LIBRARY_OPEN
	int i;
	char buf[LIB_PATH_MAX];

	dprintf(MSYSLOG_INFORMATIVE, "prepare_module_libs: called for "
	    "module:%s\n", modname);

	snprintf(buf, sizeof(buf), "%s/" MLIBNAME_STR,
	    libdir ? libdir : INSTALL_LIBDIR);

	dprintf(MSYSLOG_INFORMATIVE, "prepare_module_libs: Going to open %s\n",
	    buf);

	/* Try ./ if debugging. We don't care if it is not open */
	if ( Debug && ((*ret = dlopen(buf, DLOPEN_FLAGS)) == NULL) ) {
		snprintf(buf, sizeof(buf), "./" MLIBNAME_STR);
		dprintf(MSYSLOG_INFORMATIVE, "prepare_module_libs: Going"
		    " to open %s\n", buf);
		*ret = dlopen(buf, DLOPEN_FLAGS);
	}


	dprintf(MSYSLOG_INFORMATIVE, "prepare_module_libs: lib %s was "
	    "%sopened\n", buf, *ret == NULL?  "not " : "");

#endif
	return (1);
}

/*
 * This function gets function name from
 * the main libalat.so.X.X library or
 * if not there try to open this module
 * library. (main libname is MLIBNAME_STR !
 */

int
get_symbol(const char *modname, const char *funcname, void *h, void **ret) {
	char buf[LIB_PATH_MAX];

	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "%s_%s", modname, funcname);

	*ret = NULL;
	/*
	 * Search for symbol on main library
	 * and in module libs
	 */
	if (main_lib == NULL || ( (*ret = dlsym(main_lib, buf)) == NULL &&
	    (*ret = dlsym(main_lib, buf +  1)) == NULL ) ) {

		dprintf(MSYSLOG_INFORMATIVE, "get_symbol: func %s not found "
		    "on main lib \n", buf);

	}

	if (*ret == NULL && h && ( (*ret = dlsym(h, buf)) == NULL &&
	    (*ret = dlsym(h, buf + 1)) == NULL) ) {
	   	dprintf(MSYSLOG_SERIOUS, "get_symbol: error linking function "
		    "%s, %s\n", buf, dlerror());
	   	return(-1);
	}

	/* not on main lib, not on this module's lib */
	if (*ret == NULL)
		return(-1);

	return(1);

}

/*
 * Create a new input module, and assign module functions to generic pointer
 *
 *  I is a pointer to a list of input modules, where new one will be appended
 *  line is the command line of the input module
 */

int
imodule_create(struct i_module *I, char *line)
{
	int argc, ret, i;
	char *p, **argv = NULL;
	struct i_module *im, *im_prev;

	/* create initial node for Inputs list */
	if (I == NULL) {
	    dprintf(MSYSLOG_SERIOUS, "imodule_create: Error from caller\n");
	    return (-1);
	}

	/* go to last item on list */
	for (im_prev = I; im_prev->im_next != NULL; im_prev = im_prev->im_next);

	if (im_prev == I && im_prev->im_fd == -1) {
		im = im_prev;
	} else {
		if((im_prev->im_next = (struct i_module *) calloc(1,
		    sizeof(struct i_module))) == NULL) {
			dprintf(MSYSLOG_SERIOUS, "No memory available for "
			    "calloc\n");
			return (-1);
		}
		im = im_prev->im_next;
		im->im_fd = -1;
	}

	for (p = line; *p != '\0'; p++)
	    if (*p == ':')
	        *p = ' ';

	if ((argc = parseParams(&argv, line)) < 1) {
		snprintf(err_buf, sizeof(err_buf), "Error initializing module "
		    "%s [%s]\n", argv[0], line);
		ret = -1;
		goto imodule_create_bad;
	}

	/* is it already initialized ? searching... */
	if ((im->im_func = getImodule(argv[0])) == NULL)
		if ((im->im_func = addImodule(argv[0])) == NULL) {
			snprintf(err_buf, sizeof(err_buf), "Error loading "
			    "dynamic input module %s [%s]\n",
			    argv[0], line);
			ret = -1;
			goto imodule_create_bad;
		}

	/* got it, now try to initialize it */
	if ((*(im->im_func->im_init))(im, argv, argc) < 0) {
		snprintf(err_buf, sizeof(err_buf), "Error initializing "
		    "input module %s [%s]\n", argv[0], line);
		ret = -1;
		goto imodule_create_bad;
	}

	ret = 1;

imodule_create_bad:

	if (ret == -1) {

		/* log error first */
		logerror(err_buf);

		/* free allocated input module on queue */
		if (im_prev == I && im_prev->im_next == NULL) {
			im_prev->im_fd = -1;
		} else if (im_prev->im_next == im) {
			free (im);
			im_prev->im_next = NULL;
		}
	}

	/* free argv params if there */
	if (argv != NULL) {
		char *f;

		for (i = 0; (f = argv[i]) != NULL ; i++)
			free(f);

		free(argv);
	}

	return (ret);

}

/*
 * Create a new input module, and assign module functions to generic pointer
 * while addinf it to a filed
 *
 *  c (line) is the command line of the input module
 *  f is a pointer to a filed structure
 *  prog is the program to match
 *
 */

int
omodule_create(char *c, struct filed *f, char *prog)
{
	char	*line, *p, quotes, *argv[20];
	int	argc;
	struct o_module	*om, *om_prev;

	line = strdup(c); quotes = 0;
	p = line;

	/* create context and initialize module for logging */
	while (*p) {
		if (f->f_omod == NULL) {
			f->f_omod = (struct o_module *) calloc(1, sizeof(*f->f_omod));
			om = f->f_omod;
			om_prev = NULL;
		} else {
			for (om_prev = f->f_omod; om_prev->om_next; om_prev = om_prev->om_next);
			om_prev->om_next = (struct o_module *) calloc(1, sizeof *f->f_omod);
			om = om_prev->om_next;
		}

		switch (*p) {
			case '%':
				/* get this module name */
				argc = 0;
				while (isspace((int)*(++p)));
				argv[argc++] = p;
				while (!isspace((int)*p)) p++;

				*p++ = 0;

				/* find for matching module */
				if ((om->om_func = getOmodule(argv[0]))
				    == NULL) {
					if ((om->om_func = addOmodule(argv[0]))
					    == NULL) {

						snprintf(err_buf,
						    sizeof(err_buf), "Error "
						    "loading dynamic output "
						    "module %s [%s]\n",
						    argv[0], line);
						goto omodule_create_bad;

					}
				}

				dprintf(MSYSLOG_INFORMATIVE, "omodule_create: "
				    "got output module %s\n", argv[0]);

				/* build argv and argc, modifies input p */
				while (isspace((int)*p)) p++;
				while (*p && *p != '%' && *p != '\n' &&
				    *p != '\r' && argc<sizeof(argv) /
				    sizeof(argv[0])) { 
				
					if (*p == '"' || *p == '\'')
					    quotes = *p++;

					argv[argc++] = p;
					if (quotes) {
						while (*p != '\0' &&
						    *p != quotes) p++;
						if (*p == '\0') {
							/* not ending, fix */
							quotes = 0;
							break;
						} else {
							/* closing */
							quotes = 0;
						}
					} else {
						while ( *p != '\0' &&
						    !isspace((int)*p))
							p++;
					}
					if (*p != 0)
						*p++ = 0;
					while (*p != '\0' &&
					    isspace((int) *p))	
						p++;
				}

				dprintf(MSYSLOG_INFORMATIVE, "omodule_create:"
				    " successfully made output module "
				    "%s's argv[]\n", argv[0]);

				break;
			case '@':
			case '/':
			case '|':
			case '*':
			default:
				/* classic style */
				/* prog is already on this filed */
				argc = 0;
				argv[argc++] = "classic";
				argv[argc++] = p;
				p += strlen(p);
				/* find for matching module */
				if ((om->om_func = getOmodule(argv[0]))
				    == NULL) {
					if ((om->om_func = addOmodule(argv[0]))
					    == NULL) {
						snprintf(err_buf,
						    sizeof(err_buf), "Error "
						    "loading dynamic output "
						    "module %s [%s]\n",
						    argv[0], line);
						goto omodule_create_bad;
					}
				}

				break;
		}

		if (!om->om_func->om_init ||
		    (*(om->om_func->om_init))(argc, argv, f, prog, (void *)
		    &(om->ctx), &om->status) < 0) {
			snprintf(err_buf, sizeof(err_buf), "Error "
			    "initializing dynamic output module %s [%s]\n",
			    argv[0], line);
			goto omodule_create_bad;
		}
	}

	dprintf(MSYSLOG_INFORMATIVE, "omodule_create: all done for output "
	    "module %s\n", argv[0]);

	free(line);

	return (1);

omodule_create_bad:

	logerror(err_buf);

	if (line)
		free(line);

	/* free allocated module */
	if (f->f_omod == om) {
		f->f_omod = NULL;
	} else if (om_prev)
			om_prev->om_next = NULL;

	if (om)
		free(om);

	return (-1);

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

	line = strdup(c);
	p = line;

	/* initialize arguments before starting */
	*ret = (char **) calloc(20, sizeof(char *));
	argc = 0;

	for(q = p; *p != '\0'; p = q) {
		/* skip initial spaces */
		while (isspace((int)*p)) p++;
		if (*p == '\0')
		    break;

		if (*p == '\'') {
		    for(q = ++p; *q != '\'' && *q != '\0'; q++);

		    if (*q != '\0') {
		        *q++ = '\0';
		    }

		} else {
		    /* see how long this word is, alloc, and copy */
		    for(q = p; *q != '\0' && !isspace((int)*q); q++);
		    if (*q != '\0') {
		        *q++ = '\0';
		    }

		}

		(*ret)[argc++] = strdup(p);
		if ((argc % 20) == 18)
			if ( (*ret = (char **) realloc(*ret, sizeof(char *) *
			    (argc + 20))) == NULL) {
				free(line);
				return(-1);
			}
		if (*ret != NULL)
			(*ret)[argc] = NULL;
	}
	
	free(line);
	return (argc);
}

struct imodule *
addImodule(char *name)
{
	struct imodule *im;
	char buf[LIB_PATH_MAX];

	if (name == NULL)
		return (NULL);

	if (imodules == NULL) {
		imodules = (struct imodule *) calloc(1, sizeof(*im));
		im = imodules;
	} else {
		for(im = imodules; im->im_next; im = im->im_next);
		im->im_next = (struct imodule *) calloc(1, sizeof(*im));
		im = im->im_next;
	}

	snprintf(buf, sizeof(buf), "im_%s", name);

	if (prepare_module_libs(buf, &im->h) == -1 ||
	    get_symbol(buf, "init", im->h, (void *) &im->im_init) == -1 ||
	    get_symbol(buf, "read", im->h, (void *) &im->im_read) == -1) {

		if (imodules == im) {
			imodules = NULL;
		} else {
			struct imodule *i = imodules;
			for (; i && i->im_next == im; i = i->im_next);
			if (i)
				i->im_next = NULL;
		}

		free(im);

		dprintf(MSYSLOG_SERIOUS, "addImodule: couldn't config %s input"
		    " module\n", buf);
		return(NULL);
	}

	/* this is not mandatory */
	get_symbol(buf, "close", im->h, (void *) &im->im_close); 

	im->im_name = strdup(name);

	dprintf(MSYSLOG_INFORMATIVE, "addImodule: successfully configured %s "
	    "input module\n", buf);

	return (im);
}


struct omodule *
addOmodule(char *name)
{
	struct omodule *om;
	char buf[LIB_PATH_MAX];

	if (name == NULL)
		return (NULL);

	if (omodules == NULL) {
		omodules = (struct omodule *) calloc(1, sizeof(*om));
		om = omodules;
	} else {
		for(om = omodules; om->om_next; om = om->om_next);
		om->om_next = (struct omodule *) calloc(1, sizeof(*om));
		om = om->om_next;
	}

	snprintf(buf, sizeof(buf), "om_%s", name);

	if (prepare_module_libs(buf, &om->h) == -1 ||
	    get_symbol(buf, "init", om->h, (void *) &om->om_init) == -1 ||
	    get_symbol(buf, "write", om->h, (void *) &om->om_write) == -1) {

		if (omodules == om) {
			omodules = NULL;
		} else {
			struct omodule *o = omodules;

			for (; o && o->om_next == om; o = o->om_next);
			if (o)
				o->om_next = NULL;
		}

		free(om);

		return(NULL);
	}

	/* this is not mandatory */
	get_symbol(buf, "close", om->h, (void *) &om->om_close); 
	get_symbol(buf, "flush", om->h, (void *) &om->om_flush); 

	dprintf(MSYSLOG_INFORMATIVE, "addOmodule: successfully configured %s "
	    "output module\n", buf);


	om->om_name = strdup(name);

	return (om);
}

int
omoduleDestroy(struct omodule *om)
{
	if (om == NULL || om->h == NULL || om->om_next)
		return (-1);

	if (om->h && dlclose(om->h) < 0) {
	   	dprintf(MSYSLOG_SERIOUS, "Error [%s]\n", dlerror());
		return (-1);
	}

	free(om->om_name);

	return (1);
}

struct imodule *
getImodule(char *name)
{
	struct imodule *im;
	unsigned int len;

	if (imodules == NULL || name == NULL)
		return (NULL);

	for(im = imodules, len = strlen(name); im; im = im->im_next)
		if (im->im_name && !strncmp(im->im_name, name, len))
			break;

	return (im);
}

struct omodule *
getOmodule(char *name) {
	struct omodule *om;
	unsigned int len;

	if (omodules == NULL || name == NULL)
		return (NULL);

	for(om = omodules, len = strlen(name); om; om = om->om_next)
		if (om->om_name && !strncmp(om->om_name, name, len))
			break;

	return (om);
}


/*
 * This function removes an output module and
 * its dynamic libraries
 *
 */

int 
imodules_destroy(struct imodule *i)
{
    struct imodule *im, *im_next, *last;
    
	for (im = i, last = NULL; im; im = im_next) {

		im_next = im->im_next;

		if (!im->h) {
			last = im;
			continue;
		}

		if (last)
			last->im_next = im->im_next;

		dlclose(im->h);

		free(im);
	}

	if (last) {
		last->im_next = NULL;
		return (1); /* there are some static modules on */
	}

	return (0);
}


/*
 * This function removes an output module and
 * its dynamic libraries
 *
 */

int
omodules_destroy(struct omodule *o)
{
    struct omodule *om, *om_next, *last;
    
	for (om = o, last = NULL; om; om = om_next) {

		om_next = om->om_next;

		if (!om->h) {
			last = om;
			continue;
		}

		if (last)
			last->om_next = om->om_next;

		dlclose(om->h);
		free(om);
	}

	if (last) {
		last->om_next = NULL;
		return (1); /* there are some static modules on */
	}

	return (0);
}


