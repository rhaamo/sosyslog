/*	$CoreSDI: im_linux.c,v 1.44 2001/01/27 01:04:19 alejo Exp $	*/

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
 *  im_linux -- input module to log linux kernel messages
 *      
 * Author: Claudio Castiglia, Core-SDI S.A.
 *    
 */

#include "../../config.h"

#include <sys/types.h>
#include <sys/uio.h>
#include <sys/klog.h>
#include <sys/param.h>

#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <time.h>

#include "../modules.h"
#include "../syslogd.h"


#define	KSYM_TRANSLATE		0x01
#define KSYM_READ_TABLE		0x02
#define KLOG_USE_SYSCALL	0x04
int	 flags;
char	*linux_input_module = "linux input module";


/*
 * kernel symbols
 */
#define PATH_KSYM	"/proc/ksyms"
#define MAX_ADDR_LEN	16
#define MAX_NAME_LEN	80
#define MAX_MNAME_LEN	20

typedef struct _Symbol {
	char addr[MAX_ADDR_LEN + 1];
	char name[MAX_NAME_LEN + 1];
	char mname[MAX_MNAME_LEN + 1];
	struct _Symbol *next;
} Symbol;

FILE	*ksym_fd = NULL;
char	*ksym_path = PATH_KSYM;
Symbol	*ksym_first = NULL;
Symbol	*ksym_current = NULL;

int	 ksym_init();
void	 ksym_close();
int	 ksym_snprintf (char*, int, char*);
Symbol	*ksym_lookup (Symbol*, char*);
int	 ksym_get_symbol (Symbol*);
int	 ksym_parseline (char*, Symbol*);
char	*ksym_copyword (char*, char*, int);


/*
 * Usage
 */
void
im_linux_usage()
{
	dprintf(DPRINTF_INFORMATIVE)("linux input module options:\n"
	    "   [ -k file ]    Use the specified file as source of kernel\n"
	    "                  symbol information instead of %s.\n"
	    "   [ -r ]         Force read symbol table on memory.\n"
	    "   [ -s ]         Force to use syscall instead of %s\n"
	    "                  to log kernel messages.\n"
	    "   [ -x ]         Do not translate kernel symbols.\n"
	    "Defaults:\n"
	    "   Reads kernel messages from %s; "
#if 0
	    " if this file doesn't exists\n"
	    "   it uses the syscall method.\n"
#endif
	    "   Symbols are translated only if %s exists.\n\n",
	    PATH_KSYM, _PATH_KLOG, _PATH_KLOG, PATH_KSYM);
}


/*
 * getLine:
 * Search for a line on a string buffer
 * returns a pointer to the line or NULL if the buffer is empty
 */
char*
getLine (char *buf, int *i)
{
	if (*buf == '\0')
		return (NULL);
	while (buf[*i] != '\n' && buf[*i] != '\0')
		(*i)++;
	if (buf[*i] == '\0')
		(*i)--;
	else
		buf[*i] = '\0';
	return (buf);
}


/*
 * Sets console loglevel
 */
int
im_linux_set_console_loglevel (char *strlv)
{
	char		*err;
	unsigned long	 loglevel;

	if ( (loglevel = strtoul(strlv, &err, 10)) < 0 ||
	      loglevel > 7 || *err != '\0') {
		warnx("%s: invalid loglevel <%s>", linux_input_module, optarg);
		return (-1);
	}
	warnx("%s: setting console loglevel to <%lu>", linux_input_module,
	      loglevel);
	if (klogctl(8, NULL, loglevel) < 0) {
		warn("%s", linux_input_module);
		return (-1);
	}
	return (0);
}


/*
 * Initialize linux input module
 */
int
im_linux_init (struct i_module *I, char **argv, int argc)
{
	int ch;
	int current_optind;

	dprintf(DPRINTF_INFORMATIVE)("im_linux_init: Entering\n");

	/* parse command line */
	current_optind = optind;	/* syslogd calls im_linux_init when
					 * parsing command line
					 * This should be changed
					 */
	flags = KSYM_TRANSLATE;
	if (argc > 1) {
		optind = 1;
		while ( (ch = getopt(argc, argv, "c:C:k:rsxh?")) != -1)
			switch(ch) {
			case 'c': /* specify console loglevel */
				if (im_linux_set_console_loglevel(optarg) < 0)
					return (-1);
				break;

			case 'C': /* specify console loglevel and force exit */
				im_linux_set_console_loglevel(optarg);
				return (-1);

			case 'k': /* specify symbol file */
				if (strcmp(ksym_path, optarg))
				    if ( (ksym_path = strdup(optarg)) == NULL) {
					warn("%s", linux_input_module);
					return (-1);
				    }
				break;

			case 'r': /* force to read symbol table and keep
				   * it in memory
				   */
				flags |= KSYM_READ_TABLE;
				break;

/* not supported yet, we need to talk about somethings */
#if 0
			case 's': /* force to use syscall instead
				   * of _PATH_KLOG
				   */
				flags |= KLOG_USE_SYSCALL;
				break;
#endif

			case 'x': /* do not translate kernel symbols */
				flags &= ~KSYM_TRANSLATE;
				break;

			case 'h': /* usage */
			case '?':
			default:
				im_linux_usage();
				return (-1);
			}
	}

	I->im_path = NULL;
	I->im_fd = 0;
	if (flags & ~KLOG_USE_SYSCALL) {
		if ( (I->im_fd = open(_PATH_KLOG, O_RDONLY, 0)) >= 0)
			I->im_path = _PATH_KLOG;

/* if /proc not mounted.. sorry: syscall not supported yet */
#if 0
		else if (errno != ENOENT) {
			warn("%s: %s: %s\n",
			     linux_input_module, _PATH_KLOG, strerror(errno));
			return (-1);
		} else
			/* /proc not mounted, use syscall */
			I->im_fd = 0;
#endif
#if 1
		else {
			warn("%s: %s: %s\n",
			     linux_input_module, _PATH_KLOG, strerror(errno));
			return (-1);
		}
#endif

	}

	/* open/read symbol table file */
	if ((flags & KSYM_TRANSLATE) && ksym_init() < 0)
		return (-1);

        I->im_name = "linux";
        I->im_flags |= IMODULE_FLAG_KERN;
	optind = current_optind;
	add_fd_input(I->im_fd , I);
        return (I->im_fd);
}


/*
 * get kernel message:
 * take input line from _PATH_KLOG or klogctl(2)
 * and log it.
 */
int
im_linux_read (struct i_module *im, struct im_msg *ret)
{
	int   i;
	char *ptr;

	if (im->im_fd < 0)
		return (-1);

	/* read message from kernel */

/* syscall not supported yet */
#if 0
	if (im->im_path == NULL || flags & KLOG_USE_SYSCALL)
		/* this blocks */
		/* i = klogctl(2, im->im_buf, sizeof(im->im_buf)-1);
		 */
		/* ;;;this don't block... testing */
		i = klogctl(4, im->im_buf, sizeof(im->im_buf)-1);
	else
#endif
		i = read(im->im_fd, im->im_buf, sizeof(im->im_buf)-1);

	if (i < 0 && errno != EINTR) {
		logerror("im_linux_read");
		return (-1);
	}

	if (i) {
		im->im_buf[i] = '\0';

		/* log each msg line */
		i = 0;
		ptr = im->im_buf;
		while ( (ptr = getLine(ptr, &i)) != NULL) {
			
			/* get priority */
			if (i >= 3 && ptr[0] == '<' &&
			    ptr[2] == '>' && isdigit(ptr[1])) {
				ret->im_pri = ptr[1] - '0';
				ptr += 3;
				i -= 3;
			}
			else
				/* from printk.c: DEFAULT_MESSAGE_LOGLEVEL */
				ret->im_pri = LOG_WARNING;

			/* parse kernel/module symbols */
			if (flags & KSYM_TRANSLATE)
				ret->im_len = ksym_snprintf(ret->im_msg,
				    ret->im_mlen, ptr);
			else
				ret->im_len = snprintf(ret->im_msg,
				    ret->im_mlen, "kernel: %s", ptr);

			/* log msg */
			if (ret->im_len < 0)
			    ret->im_len = ret->im_mlen;
			strncpy(ret->im_host, LocalHostName,
			    sizeof(ret->im_host));
			ret->im_host[sizeof(ret->im_host)-1] = '\0';
			logmsg(ret->im_pri, ret->im_msg,
			    ret->im_host, ret->im_flags);
			ptr += i + 1;
			i = 0;
		}
	}
	return (0);
}


/*
 * Close linux input module
 */
int
im_linux_close (struct i_module *im)
{
	ksym_close();
	if (im->im_path != NULL) 
		return (close(im->im_fd));

	return (0);
}


/*
 * Open/load symbol table
 * Returns 0 on success and -1 on error
 */
int
ksym_init()
{
	char	 buf[128];
	Symbol	*last;
	Symbol	*next;

	ksym_close();
	if ( (ksym_fd = fopen(ksym_path, "r")) == NULL) {
		warn("%s: ksym_init: %s", linux_input_module, ksym_path);
		return (-1);
	}
	if (flags & KSYM_READ_TABLE) {
		last = NULL;
		while (fgets(buf, sizeof(buf), ksym_fd) != NULL) {
			if ( (next = (Symbol*) malloc(sizeof(Symbol))) == NULL) {
				warn("%s: ksym_init", linux_input_module);
				ksym_close();
				return (-1);
			}
			next->next = NULL;
			if (last)
				last->next = next;
			else
				ksym_first = next;
			if (ksym_parseline(buf, next) < 0) {
				warnx("%s: ksym_init: incorrect symbol file: %s"
				      , linux_input_module, ksym_path);
				ksym_close(); /* this also frees *next */
				return (-1);
			}
			last = next;
		}
		fclose(ksym_fd);
		ksym_fd = NULL;
	}
	return (0);
}


/*
 * Close/delete symbol table
 */
void
ksym_close()
{
	Symbol *s;

	if (ksym_fd != NULL) {
		fclose(ksym_fd);
		ksym_fd = NULL;
	}
	while (ksym_first) {
		s = ksym_first->next;
		free(ksym_first);
		ksym_first = s;
	}
	if (ksym_path != PATH_KSYM)
		free(ksym_path);
}


/*
 * ksym_snprintf
 */
int
ksym_snprintf (char *buf, int bufsize, char *raw)
{
	int     i;
	int	printed;
	char   *p1;
	char   *p2;
	Symbol  sym;

	if ( (printed = snprintf(buf, bufsize, "kernel: ")) < 0)
		return (-1);
	bufsize -= printed;

	while (bufsize && *raw != '\0') {
		if ( (p1 = strstr(raw, "[<")) != NULL &&
		     (p2 = strstr(p1, ">]")) != NULL) {
			for (i = 2; p1+i < p2 && isxdigit(p1[i]); i++);
			if (p1+i == p2) {
				*p2 = '\0';
				if (ksym_lookup(&sym, p1+2) != NULL) {
					*p1 = '\0';
					if ( (printed +=
					    snprintf(buf+printed, bufsize,
					    "%s [<%s> %s.%s ]", raw, sym.addr,
					    sym.mname, sym.name)) < 0)
						return (-1);
					bufsize -= printed;

					/* we need to solve some things
 					 * about buf and msg params on
					 * im_xxxxx_read.
					 * so, i think that is better
					 * not to change raw data ;;;
					 */
					*p1 = '[';
					*p2 = '>';
					raw = p2+2;
					continue;
				}
				*p2 = '>';
			} 
		} 
		break;
	}

	if (*raw) {
		/* kernel message without symbols */
		if ( (i = snprintf(buf+printed, bufsize, "%s", raw)) < 0)
			return (-1);
		else
			printed += i;
	}

	return (printed);
}


/*
 * Lookup symbol:
 * search for a symbol on internal table/file that
 * matches an address.
 * If the symbol does not exists it returns NULL
 */
Symbol*
ksym_lookup (Symbol *sym, char *addr)
{
	/* reset symbol table/file pointer */
	if (ksym_fd == NULL)
		ksym_current = ksym_first;
	else
		fseek(ksym_fd, 0, SEEK_SET);

	/* search for symbol */
	while (!ksym_get_symbol(sym))
		if (!strcasecmp(sym->addr, addr))
			return (sym);

	return (NULL);
}


/*
 * Get a symbol from table/file
 * returns 0 on success and -1 on end of file/table
 */
int
ksym_get_symbol (Symbol *sym)
{
	char msg[MAXLINE];

	if (ksym_fd == NULL) {
		if (ksym_current != NULL) {
			*sym = *ksym_current;
			ksym_current = ksym_current->next;
			return (0);
		}
	} else if (fgets(msg, sizeof(msg), ksym_fd) != NULL)
			return (ksym_parseline(msg, sym));
	return (-1);
}


/*
 * ksym_parseline: converts a line onto a Symbol
 * returns 0 on success and -1 on error
 */
#define QUIT_BLANK(a)	while (*a == ' ' || *a == '\t') a++;

int
ksym_parseline (char *p, Symbol *sym)
{
	if (sym == NULL || p == NULL || p[0] == '\0')
		return (-1);

	sym->addr[0] = sym->name[0] = sym->mname[0] = '\0';

	/* copy address */
	QUIT_BLANK(p);
	if (*p == '\0' || *p == '\n')
		return (-1);
 	p = ksym_copyword(sym->addr, p, MAX_ADDR_LEN);

	/* copy name */
	QUIT_BLANK(p);
	if (*p == '\0' || *p == '\n')
		return (-1);
	p = ksym_copyword(sym->name, p, MAX_NAME_LEN);

	/* copy module name (if any) */
	QUIT_BLANK(p);
	ksym_copyword(sym->mname, p, MAX_MNAME_LEN);

	return (0);
}


/*
 * copyword(dst, src, len)
 * Copy from src to dst until reaches
 * len bytes or '\0' or '\n'
 */
char*
ksym_copyword (char *dst, char *src, int max)
{
	int i = 0;

	if (max) {
		max--;
		while (*src != ' ' && *src != '\t' &&
		       *src != '\0' && *src != '\n' && i < max)
			dst[i++] = *src++;
		dst[i] = '\0';
	}
	return (src);
}

