/*	$CoreSDI: modules.c,v 1.132 2000/11/03 19:54:53 alejo Exp $	*/

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
#include <limits.h>
#include "syslogd.h"
#include "modules.h"

#ifdef _POSIX_PATH_MAX
#define LIB_PATH_MAX _POSIX_PATH_MAX
#else
#define LIB_PATH_MAX 254
#endif

#ifdef HAVE_LINUX
#define DLOPEN_FLAGS RTLD_LAZY | RTLD_GLOBAL
#else
#define DLOPEN_FLAGS RTLD_LAZY
#endif

#ifdef HAVE_OPENBSD
#define SYMBOL_PREFIX "_"
#else
#define SYMBOL_PREFIX ""
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

/*
 *
 * Here are declared needed module libs when dynamic
 *
 */

struct {
	char *name;
	char *libs[MLIB_MAX];
} mlibs[] = {
	{"mysql", {"libmysqlclient.so", NULL} },
	{"pgsql", {"libpq.so", NULL} },
	{ NULL, { NULL } }
};


/* assign module functions to generic pointer */
int
imodule_init(struct i_module *I, char *line)
{
	int argc, ret;
	char *p, **argv = NULL;
	struct i_module *im, *im_prev;

	/* create initial node for Inputs list */
	if (I == NULL) {
	    dprintf("imodule_init: Error from caller\n");
	    return (-1);
	}

	/* go to last item on list */
	for (im_prev = I; im_prev->im_next != NULL; im_prev = im_prev->im_next);

	if (im_prev == I && im_prev->im_fd == -1) {
		im = im_prev;
	} else {
		if((im_prev->im_next = (struct i_module *) calloc(1,
		        sizeof(struct i_module))) == NULL) {
		    dprintf("No memory available for calloc\n");
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
		goto imodule_init_bad;
	}

	/* is it already initialized ? searching... */
	if ((im->im_func = getImodule(argv[0])) == NULL)
		if ((im->im_func = addImodule(argv[0])) == NULL) {
			snprintf(err_buf, sizeof(err_buf), "Error loading "
			    "dynamic input module %s [%s]\n",
					argv[0], line);
			ret = -1;
			goto imodule_init_bad;
		}

	/* got it, now try to initialize it */
	if ((*(im->im_func->im_init))(im, argv, argc) < 0) {
		snprintf(err_buf, sizeof(err_buf), "Error initializing "
		    "input module %s [%s]\n", argv[0], line);
		ret = -1;
		goto imodule_init_bad;
	}

	ret = 1;

imodule_init_bad:

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
		int i;

		for (i = 0; (f = argv[i]) != NULL ; i++)
				free(f);

		free(argv);
	}

	return (ret);

}

/* create all necesary modules for a specific filed */
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

				/* build argv and argc, modifies input p */
				while (isspace((int)*p)) p++;
				while (*p && *p != '%' && *p != '\n' &&
				    *p != '\r' && argc<sizeof(argv) /
				    sizeof(argv[0])) { 
				
					(*p == '"' || *p == '\'') ?
					    quotes = *p++ : 0;
						
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
					*p++ = 0;
					while (*p != '\0' &&
					    isspace((int) *p))	
						p++;
				}


				break;
			case '@':
			case '/':
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

		if ((*(om->om_func->om_init))(argc, argv, f, prog,
		    (void *) &(om->ctx)) < 0) {
			snprintf(err_buf, sizeof(err_buf), "Error "
			    "initializing dynamic output module %s [%s]\n",
			    argv[0], line);
			goto omodule_create_bad;
		}
	}

	free(line);

	if (f->f_type == F_UNUSED)
		f->f_type = F_MODULE;

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
		    *ret = (char **) realloc(*ret, sizeof(char *) * (argc + 20));
		(*ret)[argc] = NULL;
	}
	
	free(line);
	return (argc);
}

struct imodule *
addImodule(char *name)
{
	struct imodule *im;
	char buf[LIB_PATH_MAX], *r;
	int i, j;

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

	for( i = 0; mlibs[i].name; i++) {
		if(!strcmp(name, mlibs[i].name)) {
			for(j = 0; (r = mlibs[i].libs[j]) && j < MLIB_MAX;
			    j++) { 
				dprintf("addImodule: going to open library %s "
				    "for module %s\n", name, r);
				if ((im->ih[j] = dlopen(r, DLOPEN_FLAGS))
				    == NULL) {
					dprintf("Error [%s] on file [%s]\n",
							dlerror(), r);
					goto addImod_bad;
				}
			}
		}
	}

	snprintf(buf, sizeof(buf), "%s/libmsyslog_im_%s.so",
			libdir ? libdir : INSTALL_LIBDIR, name);

	if ((im->h = dlopen(buf, DLOPEN_FLAGS)) == NULL) {
	   	dprintf("Error [%s] on file [%s]\n", dlerror(), buf);
	   	goto addImod_bad;
	}

	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "im_%s_init", name);

	if ((im->im_init = dlsym(im->h, buf)) == NULL) {
	   	dprintf("addImodule: error linking %s function %s\n", buf,
		    dlerror());
	   	goto addImod_bad;
	}

	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "im_%s_getLog", name);

	if ((im->im_getLog = dlsym(im->h, buf)) == NULL) {
	   	dprintf("addImodule: error linking %s function %s\n", buf,
		    dlerror());
	   	goto addImod_bad;
	}


	/* this one could be null */
	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "im_%s_close", name);

	im->im_close = dlsym(im->h, buf);

	im->im_name = strdup(name);

	return (im);

addImod_bad:
	if (imodules == im) {
		imodules = NULL;
	} else {
		struct imodule *i = imodules;
		for (; i && i->im_next == im; i = i->im_next);
		if (i)
			i->im_next = NULL;
	}

	free(im);

	return(NULL);
}


struct omodule *
addOmodule(char *name)
{
	struct omodule *om;
	char buf[LIB_PATH_MAX], *r;
	int i, j;

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

	for( i = 0; mlibs[i].name; i++) {
		if(!strcmp(name, mlibs[i].name)) {
			for(j = 0; (r = mlibs[i].libs[j]) && j < MLIB_MAX;
			    j++) {
				dprintf("addImodule: going to open library %s "
				    "for module %s\n", name, r);
				if ((om->oh[j] = dlopen(r, DLOPEN_FLAGS))
				    == NULL) {
					dprintf("Error [%s] on file [%s]\n",
							dlerror(), r);
				goto addOmod_bad;
				}
			}
		}
	}

	snprintf(buf, LIB_PATH_MAX, "%s/libmsyslog_om_%s.so.1.01",
			libdir ? libdir : INSTALL_LIBDIR, name);

	if ((om->h = dlopen(buf, DLOPEN_FLAGS)) == NULL) {
	   	dprintf("Error [%s] on file [%s]\n", dlerror(), buf);
	   	goto addOmod_bad;
	}

	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "om_%s_init", name);

	if ((om->om_init = dlsym(om->h, buf)) == NULL) {
	   	dprintf("addOmodule: error linking %s function \n", buf);
	   	goto addOmod_bad;
	}

	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "om_%s_doLog", name);

	if ((om->om_doLog = dlsym(om->h, buf)) == NULL) {
	   	dprintf("addOmodule: error linking %s function \n", buf);
	   	goto addOmod_bad;
	}

	/* this ones could be null */
	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "om_%s_close", name);
	om->om_close = dlsym(om->h, buf);
	snprintf(buf, sizeof(buf), SYMBOL_PREFIX "om_%s_flush", name);
	om->om_flush = dlsym(om->h, buf);

	om->om_name = strdup(name);

	return (om);

addOmod_bad:
	if (omodules == om) {
		omodules = NULL;
	} else {
		struct omodule *o = omodules;
		for (; o && o->om_next != om; o = o->om_next);
		if (o)
			o->om_next = NULL;
	}
	free(om);
	return (NULL);
}

int
omoduleDestroy(struct omodule *om)
{
	if (om == NULL || om->h == NULL || om->om_next)
		return (-1);

	if (dlclose(om->h) < 0) {
	   	dprintf("Error [%s]\n", dlerror());
		return (-1);
	}

	free(om->om_name);

	return (1);
}

struct imodule *
getImodule(char *name)
{
	struct imodule *im;
	int len;

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
	int len;

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
		int j;

		im_next = im->im_next;

		if (!im->h) {
			last = im;
			continue;
		}

		if (last)
			last->im_next = im->im_next;

		for (j = 0; j < MLIB_MAX && im->ih[j]; j++)
			dlclose(im->ih[j]);

		if (im->h)
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
		int j;

		om_next = om->om_next;

		if (!om->h) {
			last = om;
			continue;
		}

		if (last)
			last->om_next = om->om_next;

		for (j = 0; j < MLIB_MAX && om->oh[j]; j++)
			dlclose(om->oh[j]);

		dlclose(om->h);
		free(om);
	}

	if (last) {
		last->om_next = NULL;
		return (1); /* there are some static modules on */
	}

	return (0);
}


