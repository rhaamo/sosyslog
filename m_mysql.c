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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1983, 1988, 1993, 1994\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "@(#)syslogd.c	8.3 (Berkeley) 4/4/94";*/
static char rcsid[] = "$NetBSD: syslogd.c,v 1.5 1996/01/02 17:48:41 perry Exp $";
#endif /* not lint */

/*
 *  m_mysql -- MySQL database syupport Module
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *         form syslogd.c Eric Allman  and Ralph Campbell
 *
 */

#include <sys/time.h>

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syslog.h>

#include "syslogd.h"
#include "modules.h"

#include "/usr/local/include/mysql/mysql.h"

#define MAX_QUERY	8192

struct m_mysql_ctx {
	short	flags;
	int	size;
	MYSQL	*h;
	char	*host;
	int	port;
	char	*user;
	char	*passwd;
	char	*db;
	char	*table;
	char	*query;		/* speed hack: this is a buffer for querys */
};


int
m_mysql_doLog(f, flags, msg, context)
	struct filed *f;
	int flags;
	char *msg;
	struct m_header *context;
{
	struct m_mysql_ctx *c;
	char	*dummy, *y, *m, *d, *h;

	if (f == NULL)
		return (-1);

	/* this module doesn't cache */
	if (msg == NULL)
		return(0);

	c = (struct m_mysql_ctx *) context;
	memset(c->query, 0, MAX_QUERY);

	/* get date and time */

	/* mysql needs 2000-01-25 like format */
	dummy = strdup(f->f_lasttime);
	*(dummy + 3)  = '\0'; *(dummy + 6)  = '\0';
	*(dummy + 15) = '\0'; *(dummy + 20) = '\0';
	m = dummy;
	d = dummy + 4;
	h = dummy + 7;
	y = dummy + 16;

	/* table, YYYY-Mmm-dd, hh:mm:ss, host, programname,  msg  */ 
	if (snprintf(c->query, MAX_QUERY - 2, "INSERT INTO %s VALUES('%s-%s-%s', '%s',
			'%s', '%s', '%s', '%s",
			c->table, y, m, d, h, f->f_prevhost, msg) == MAX_QUERY - 2 ) {
		/* force termination if msg filled the buffer */
		c->query[MAX_QUERY - 2] = '\'';
		c->query[MAX_QUERY - 1] = ')';
		c->query[MAX_QUERY]     = '\0';
	}

	free(dummy);
	return (mysql_query(c->h, c->query));
}


/*
 *  INIT -- Initialize m_mysql
 *
 *  Parse options and connect to database
 *
 *  params:
 *         -s <host:port>
 *         -u <user>
 *         -p <password>
 *         -b <database_name>
 *         -t <table_name>
 *         -c			create table
 *
 */

int
m_mysql_init(argc, argv, f, prog, c)
	int	argc;
	char	**argv;
	struct filed *f;
	char *prog;
	struct m_header **c;
{
	MYSQL *h;
	char	*host, *user, *passwd, *db, *ux_sock;
	char	*table;
	int	port, client_flag, createTable;
	struct m_mysql_ctx	*context;
	int	ch, i;

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
			prog == NULL || c == NULL)
		return (-1);

	p = *argv;
	host = NULL; user = NULL; passwd = NULL;
	db = NULL; ux_sock = NULL; port = 0;
	client_flag = 0; createTable = 0;

	/* parse line */
	while ((ch = getopt(argc, argv, "s:u:p:b:t:c")) != -1)

	switch (*p) {
		case 's':
			/* get database host name and port */
			if ((c = strstr(optarg, ":") == NULL)
				port = MYSQL_PORT;
			else
				port = atoi(++c);
			host = strdup(optarg);
			break;
		case 'u':
			user = strdup(optarg);
			break;
				break;
		case 'p':
			passwd = strdup(optarg);
			break;
		case 'b':
			db = strdup(optarg);
			break;
		case 't':
			table = strdup(optarg);
			break;
		case 'c':
			createTable++;
			break;
		default:
			return(-1);
		}
	}

	if ( user == NULL || passwd == NULL || *db == NULL || 
			ux_sock == NULL || port == 0)
		return (-3);

	/* clean up the mess */
	if (!mysql_init(h)) {
		dprintf("Error initializing handle\n");
		return(-4);
	}
	if (!mysql_real_connect(h, host, user, passwd, db, port,
			NULL, client_flag)) {
		dprintf("Error connecting to db server [%s:%i] user [%s] db [%s]\n",
				host, port, user, db);
		return(-5);
	}

	/* save handle and stuff on context */
	*c = (struct m_header *) calloc(1, sizeof(struct m_mysql_ctx));

	context = (struct m_mysql_ctx *) *c;
	context->size = sizeof(struct m_mysql_ctx);
	context->h = h;
	context->host = host;
	context->port = port;
	context->user = user;
	context->passwd = passwd;
	context->db = db;
	context->table = table;
	context->query = (char *) calloc(1, MAX_QUERY);

	return (0);
}

void
m_mysql_destroy_ctx(context)
	struct m_mysql_ctx *context;
{
	free(context->h);
	free(context->host);
	free(context->user);
	free(context->passwd);
	free(context->db);
	free(context->table);
	free(context->query);
}

int
m_mysql_close(f, context)
	struct filed *f;
	struct m_header **context;
{
	struct m_mysql_ctx *c;

	c = (struct m_mysql_ctx *) *context;
	mysql_close(c->h);
	m_mysql_destroy_ctx(c);
	free(*context);
	context = NULL;

	return (-1);
}

int
m_mysql_flush(f, context)
	struct filed *f;
	void *context;
{
	/* this module doesn't need to "flush" data */
	return (0);
}

