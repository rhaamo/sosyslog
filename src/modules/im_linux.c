/*	$CoreSDI: im_linux.c,v 1.13 2000/06/07 21:27:37 claudio Exp $	*/

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
 * Author: Claudio Castiglia, Core-SDI SA
 *    
 */

#include "config.h"

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

#include "syslogd.h"


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
Symbol	*ksym_lookup (Symbol*, char*);
int	 ksym_getSymbol (Symbol*);
int	 ksym_parseLine (char*, Symbol*);
char	*ksym_copyWord (char*, char*, int);


/*
 * readline
 */
int
readline (fd, buf, max)
	int	 fd;
	char	*buf;
	int	 max;
{
	return(-1);
}


/*
 * Usage
 */
void
im_linux_usage()
{
	fprintf(stderr,
		"linux input module options:\n"
		"    [ -k file ]    Use the specified file as source of kernel\n"
		"                   symbol information instead of %s.\n"
		"    [ -r ]         Force read symbol table on memory.\n"
		"    [ -s ]         Force to use syscall instead of %s\n"
		"                   to log kernel messages.\n"
		"    [ -x ]         Do not translate kernel symbols.\n"
		"Defaults:\n"
		"    Reads kernel messages from %s; if this file doesn't exists\n"
		"    it uses the syscall method.\n"
		"    Symbols are translated only if %s exists.\n\n",
		PATH_KSYM, _PATH_KLOG, _PATH_KLOG, PATH_KSYM);
}


/*
 * getLine
 */
char*
getLine (buf, i)
	char *buf;
	int  *i;
{
	if (buf[*i] == '\0')
		return(NULL);
	while (buf[*i] != '\n' && buf[*i] != '\0')
		(*i)++;
	buf[*i] = '\0';
	return(buf);
}


/*
 * Initialize linux input module
 */
int
im_linux_init(I, argv, argc)
	struct i_module *I;
	char	**argv;
	int	argc;
{
	int ch;
	int current_optind;


	dprintf ("\nim_linux_init...\n");

	/* parse command line */
	current_optind = optind;
	flags = KSYM_TRANSLATE;
	if (argc > 1) {
		optind = 1;
		while ( (ch = getopt(argc, argv, "k:rsxh?")) != -1)
			switch(ch) {
			case 'k': /* specify symbol file */
				if (strcmp(ksym_path, optarg))
					ksym_path = strdup(optarg);
				break;

			case 'r': /* force read symbol table */
				flags |= KSYM_READ_TABLE;
				break;

			case 's': /* force to use syscall instead of _PATH_KLOG */
				flags |= KLOG_USE_SYSCALL;
				break;

			case 'x': /* do not tranlate kernel symbols */
				flags &= ~KSYM_TRANSLATE;
				break;

			case 'h': /* usage */
			case '?':
			default:
				im_linux_usage();
				return(-1);
			}
	}

	I->im_path = NULL;
	I->im_fd = 0;
	if (flags & ~KLOG_USE_SYSCALL) {
		if ( (I->im_fd = open(_PATH_KLOG, O_RDONLY, 0)) >= 0)
			I->im_path = _PATH_KLOG;
		else if (errno != ENOENT) {
			warn("%s: %s: %s\n", linux_input_module, _PATH_KLOG, strerror(errno));
			return(-1);
		}
	}

	/* open/read symbol table file */
	if (flags & KSYM_TRANSLATE)
		ksym_init();

        I->im_type = IM_LINUX;
        I->im_name = "linux";
        I->im_flags |= IMODULE_FLAG_KERN;
	optind = current_optind;
        return(I->im_fd);
}


/*
 * get kernel message:
 * take input line from _PATH_KLOG or klogctl(2)
 * and log it.
 */

int
im_linux_getLog(im, ret)
	struct i_module *im;
	struct im_msg  *ret;
{
	int   i;
	int   j;
	int   readed;
	char *ptr;

	if (im->im_fd < 0)
		return (-1);

	/* read message from kernel */
	if (im->im_path == NULL || flags & KLOG_USE_SYSCALL)
		/* readed = klogctl(2, im->im_buf, sizeof(im->im_buf)); */ /* this blocks */
		readed = klogctl(4, im->im_buf, sizeof(im->im_buf));	/* ;;;this don't block... testing */
	else
		readed = read(im->im_fd, im->im_buf, sizeof(im->im_buf));

	if (readed < 0 && errno != EINTR) {
		logerror("im_linux_getLog");
		return(-1);
	}

	if (readed) {
		im->im_buf[readed-1] = '\0';

		/* log each msg line */
		i = j = 0;
		while ( (ptr = getLine(im->im_buf + i, &j)) != NULL && readed) {
			i += j + 1;
			readed -= j;
			
			/* get priority */
			if (j >= 3 && ptr[0] == '<' && ptr[2] == '>' && isdigit(ptr[1])) {
				ret->im_pri = ptr[1] - '0';
				ptr += 3;
			}
			else
				ret->im_pri = LOG_WARNING;	/* from printk.c: DEFAULT_MESSAGE_LOGLEVEL */

			/* parse kernel/module symbols */
			if (flags & KSYM_TRANSLATE) {
				;;;
				;;;
			}

			/* log msg */
			ret->im_len = snprintf(ret->im_msg, sizeof(ret->im_msg), "kernel: %s", ptr);
			strncpy(ret->im_host, LocalHostName, sizeof(ret->im_host));
			ret->im_host[sizeof(ret->im_host)-1] = '\0';
			logmsg(ret->im_pri, ret->im_msg, ret->im_host, ret->im_flags);


			;;;detectar el final de la linea verdadera => con readed
		}
	}
	return(0);

//	char *p, *q, *lp;
//	int i, c;
//
//	(void)strncpy(ret->im_msg, _PATH_UNIX, sizeof(ret->im_msg) - 3);
//	(void)strncat(ret->im_msg, ": ", 2);
//	lp = ret->im_msg + strlen(ret->im_msg);
//
//	i = read(im->im_fd, im->im_buf, sizeof(im->im_buf) - 1);
//	if (i > 0) {
//		(im->im_buf)[i] = '\0';
//		for (p = im->im_buf; *p != '\0'; ) {
//			/* fsync file after write */
//			ret->im_flags = SYNC_FILE | ADDDATE;
//			ret->im_pri = DEFSPRI;
//			if (*p == '<') {
//				ret->im_pri = 0;
//				while (isdigit(*++p))
//				    ret->im_pri = 10 * ret->im_pri + (*p - '0');
//				if (*p == '>')
//				        ++p;
//			} else {
//				/* kernel printf's come out on console */
//				ret->im_flags |= IGN_CONS;
//			}
//			if (ret->im_pri &~ (LOG_FACMASK|LOG_PRIMASK))
//				ret->im_pri = DEFSPRI;
//			q = lp;
//			while (*p != '\0' && (c = *p++) != '\n' &&
//					q < (ret->im_msg+sizeof ret->im_msg))
//				*q++ = c;
//			*q = '\0';
//			strncat(ret->im_host, LocalHostName, sizeof(ret->im_host) - 1);
//			ret->im_len = strlen(ret->im_msg);
//			logmsg(ret->im_pri, ret->im_msg, ret->im_host, ret->im_flags);
//		}
//
//	} else if (i < 0 && errno != EINTR) {
//		logerror("im_bsd_getLog");   
//		im->im_fd = -1;
//	}
//
//	/* if ok return (2) wich means already logged */
//	return(im->im_fd == -1 ? -1: 2);
//	return(-1);
}


/*
 * Close linux input module
 */
int
im_linux_close (im)
	struct i_module *im;
{
	ksym_close();
	if (im->im_path != NULL) 
		return(close(im->im_fd));
	return(0);
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
		return(-1);
	}
	if (flags & KSYM_READ_TABLE) {
		last = NULL;
		while (fgets(buf, sizeof(buf), ksym_fd) != NULL) {
			if ( (next = (Symbol*)malloc(sizeof(Symbol))) == NULL) {
				warn("%s: ksym_init", linux_input_module);
				ksym_close();
				return(-1);
			}
			next->next = NULL;
			if (last)
				last->next = next;
			else
				ksym_first = next;
			if (ksym_parseLine(buf, next) < 0) {
				warnx("%s: ksym_init: incorrect symbol file: %s", linux_input_module, ksym_path);
				ksym_close();
				return(-1);
			}
			last = next;
		}
		fclose(ksym_fd);
		ksym_fd = NULL;
	}
	return(0);
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
 * Lookup symbol:
 * search for a symbol on internal table/file that
 * matches an address.
 * If the symbol do not exists it returns NULL
 */
Symbol*
ksym_lookup (sym, addr)
	Symbol *sym;
	char *addr;
{
	/* reset symbol table/file */
	if (ksym_fd == NULL)
		ksym_current = ksym_first;
	else
		fseek(ksym_fd, 0, SEEK_SET);

	/* search for symbol */
	while(!ksym_getSymbol(sym))
		if (!strcasecmp(sym->addr, addr))
			return(sym);

	return(NULL);
}


/*
 * Get a symbol from file or table
 * returns 0 on success and -1 on end of file/table
 */
int
ksym_getSymbol (sym)
	Symbol *sym;
{
	char msg[MAXLINE];

	if (ksym_fd == NULL) {
		if (ksym_current != NULL) {
			sym = ksym_current;
			ksym_current = ksym_current->next;
			return(0);
		}
	} else if (fgets(msg, sizeof(msg), ksym_fd) != NULL)
			return(ksym_parseLine(msg, sym));
	return(-1);
}


/*
 * ksym_parseLine: converts a line onto a Symbol
 * returns 0 on success and -1 on error
 */
#define QUIT_BLANK(a)	while (*a == ' ' || *a == '\t') a++;

int
ksym_parseLine (p, sym)
	char *p;
	Symbol *sym;
{
	if (sym == NULL || p == NULL || p[0] == '\0')
		return(-1);

	/* copy address */
	QUIT_BLANK(p);
	if (*p == '\0' || *p == '\n')
		return(-1);
 	p = ksym_copyWord(sym->addr, p, MAX_ADDR_LEN);

	/* copy name */
	QUIT_BLANK(p);
	if (*p == '\0' || *p == '\n')
		return(-1);
	p = ksym_copyWord(sym->name, p, MAX_NAME_LEN);

	/* copy module name (if any) */
	QUIT_BLANK(p);
	ksym_copyWord(sym->mname, p, MAX_MNAME_LEN);

	return(0);
}


/*
 * copyWord(dst, src, len)
 * Copy from src to dst until reaches
 * len bytes or '\0' or '\n'
 */
char*
ksym_copyWord (dst, src, max)
	char *dst;
	char *src;
	int   max;
{
	int i = 0;

	if (max) {
		max--;
		while (*src != ' ' && *src != '\t' && *src != '\0' && *src != '\n' && i < max)
			dst[i++] = *src++;
		dst[i] = '\0';
	}
	return(src);
}

