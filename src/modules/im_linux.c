/*	$CoreSDI: im_linux.c,v 1.9 2000/06/06 00:53:23 claudio Exp $	*/

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

#include <ctype.h>
#include <fcntl.h>
#include <errno.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "syslogd.h"


#define	TRANSLATE_KSYMBOLS	0x01
#define USE_SYSCALL		0x02
int flags;

int parseCommandLine(int, char**);


/*
 * kernel symbols
 */

#define PATH_KSYM	"/proc/ksyms"
#define MAX_ADDR_LEN	16
#define MAX_NAME_LEN	80
#define MAX_MNAME_LEN	20

typedef struct {
	char addr[MAX_ADDR_LEN + 1];
	char name[MAX_NAME_LEN + 1];
	char mname[MAX_MNAME_LEN + 1];
} Symbol;

int	 symbols = 0;
int	 offset = 0;
FILE	*ksym_fd = NULL;
Symbol	*symbol_table = NULL;

int	 ksym_init(struct i_module*);
void	 ksym_close();
Symbol	*ksym_lookup(Symbol*, char*);
int	 ksym_getSymbol(Symbol*);
int	 ksym_parseLine(char*, Symbol*);
int	 ksym_copy(char*, char*, int);


/*
 * initialize linux kernel input module
 */

int
im_linux_init(I, argv, argc)
	struct i_module *I;
	char	**argv;
	int	argc;
{
	dprintf ("im_linux_init...\n");

	flags = TRANSLATE_KSYMBOLS;
	if (!parseCommandLine(argc, argv))
		return(-1);
	I->im_path = NULL;
	I->im_fd = 0;
	if (flags & ~USE_SYSCALL) {
		if ((I->im_fd = open(_PATH_KLOG, O_RDONLY, 0)) >= 0)
			I->im_path = _PATH_KLOG;
		else if (errno != ENOENT) {
			dprintf("can;t open %s: %s\n", _PATH_KLOG, strerror(errno));
			return(-1);
		}
	}
        I->im_type = IM_LINUX;
        I->im_name = "linux";
        I->im_flags |= IMODULE_FLAG_KERN;
        return(I->im_fd);
}


int
parseCommandLine(argc, argv)
	int    argc;
	char **argv;
{
	int i;
	for (i = 0; i < argc; i++)
		dprintf("%i: %s\n", argv[i]);
	return(-1);
}


/*
 * get kernel messge:
 * take input line from _PATH_KLOG or klogctl(2),
 * split and format similar to syslog().
 */

int
im_linux_getLog(im, ret)
	struct i_module *im;
	struct im_msg  *ret;
{
	int i;
	int pri;
	int readed;

	if (im->im_fd < 0)
		return (-1);

	/* read message from kernel */
	if (im->im_path)
		readed = read(im->im_fd, im->im_buf, sizeof(im->im_buf));
	else
		/* readed = klogctl(2, im->im_buf, sizeof(im->im_buf)); */ /* this blocks */
		readed = klogctl(4, im->im_buf, sizeof(im->im_buf));	/* this don't block... testing */

	if (readed < 0 && errno != EINTR) {
		logerror("im_linux_getLog");
		return(-1);
	}

	im->im_buf[readed-1] = '\0';

	/* get priority */
	i = 0;
	if (readed >= 3 && im->im_buf[0] == '<' && im->im_buf[2] == '>' && isdigit(im->im_buf[1])) {
		pri = '9' - im->im_buf[1];
		i = 3;
		readed -= 3;
	}
	else
		pri = LOG_WARNING;	/* from printk.c: DEFAULT_MESSAGE_LOGLEVEL */

	/* parse kernel/module symbols */
	;;;
	;;;

	/* log msg */
	ret->im_len = snprintf(ret->im_msg, sizeof(ret->im_msg), "kernel: %s", &im->im_buf[i]);
	strncpy(ret->im_host, LocalHostName, sizeof(ret->im_host));
	ret->im_host[sizeof(ret->im_host)-1] = '\0';

	logmsg(ret->im_pri, ret->im_msg, ret->im_host, ret->im_flags);
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
 * close linux input module
 */

int
im_linux_close(im)
	struct i_module *im;
{
	if (im->im_path != NULL) 
		return(close(im->im_fd));

	return(0);
}


/*
 * open/load symbol table
 */

int
ksym_init(I)
	struct i_module *I;
{
	ksym_close();
	if ( (ksym_fd = fopen(PATH_KSYM, "r")) < 0) {
		/* read ksym using syscalls */
		;;;
		;;;
	}
	return(0);
}


/*
 * close/delete symbol table
 */

void
ksym_close()
{
	if (ksym_fd != NULL) {
		fclose(ksym_fd);
		ksym_fd = NULL;
	}
	if (symbol_table != NULL) {
		free(symbol_table);
		symbol_table = NULL;
		symbols = 0;
	}
}


/*
 * lookup symbol:
 * search for a symbol on internal table/file that
 * matches an address.
 * if the symbol do not exists it returns NULL
 */

Symbol*
ksym_lookup(sym, addr)
	Symbol *sym;
	char *addr;
{
	/* reset symbol table/file */
	if (ksym_fd < 0)
		offset = 0;
	else
		fseek(ksym_fd, 0, SEEK_SET);

	/* search for symbol */
	while(ksym_getSymbol(sym))
		if (!strcasecmp(sym->addr, addr))
			return(sym);

	return(NULL);
}


/*
 * Get a symbol from file or table
 */

int
ksym_getSymbol (sym)
	Symbol *sym;
{
	char msg[MAXLINE];

	if (ksym_fd != NULL) {
		if (offset == symbols)
			return(0);
		*sym = symbol_table[offset++];
	} else {
		if ( fgets(msg, sizeof(msg), ksym_fd) == NULL)
			return(0);
		return(ksym_parseLine(msg, sym));
	}
	return(1);
}


/*
 * ksym_parseLine (converts a line onto a Symbol)
 * returns 1 on success and 0 on error
 */

#define QUIT_BLANK(p)	while(isblank(*p) && *p != '\0' && *p != '\n') p++;

int
ksym_parseLine (p, sym)
	char *p;
	Symbol *sym;
{
	if (sym == NULL || p == NULL || p[0] == '\0')
		return(0);

	/* copy symbol address */
	QUIT_BLANK(p);
	if (*p == '\0' || *p == '\n')
		return(0);
	if (!ksym_copy(sym->addr, p, MAX_ADDR_LEN))
		return(0);

	/* copy symbol name */
	QUIT_BLANK(p);
	if (*p == '\0' || *p == '\n')
		return(0);
	ksym_copy(sym->name, p, MAX_NAME_LEN);

	/* copy module name (if any) */
	QUIT_BLANK(p);
	ksym_copy(sym->mname, p, MAX_MNAME_LEN);

	return(1);
}


/*
 * copy
 */

int
ksym_copy (src, dst, max)
	char *dst;
	char *src;
	int   max;
{
	int i = 0;

	while (!isblank(*src) && *src != '\0' && *src != '\n' && i < max)
		dst[i++] = *src++;
	dst[i] = '\0';
	return((*src == '\0' || *src == '\n') ? 0 : 1);
}


