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


struct m_mysql_ctx {
	MYSQL	*h;
	char	*date_col;
	char	*program_col;
	char	*host_col;
	char	*msg_col;
};


int
m_mysql_printlog(f, flags, msg, context)
	struct filed *f;
	int flags;
	char *msg;
	void *context;
{
	return (-1);
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
 *         -d <database_name>
 *         -t <table_name>
 *         -c			create table
 *         -d <date_column>
 *         -n <program_name_column>
 *         -h <host_name>
 *         -m <message_column>
 *
 */

int
m_mysql_init(line, f, prog, c)
	char *line;
	struct filed *f;
	char *prog;
	void *c;
{
	char *p, *q;
	MYSQL *h;
	char	*host, *user, *passwd, *db, *ux_sock;
	char	*table; *date_col, *program_col, *host_col, *msg_col;
	int	port, client_flag, createTable;
	void	*context;
	int	i;

	if (line == NULL || f == NULL || prog == NULL || c == NULL)
		return (-1);

	p = line;
	host = NULL; user = NULL; passwd = NULL;
	db = NULL; ux_sock = NULL; port = 0;
	client_flag = 0; createTable = 0;

	/* parse line */
	for ( p = line; p != NULL && *p != '\0';) {
		while (isspace(*p)) p++;
		if (*p != '-' || *p == '\0')
			return(-2);

		switch (*p) {
			case 's':
				/* get database host name */
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				host = (char *) calloc(1, i + 1);
				for (q = host; i--; *q++ = *p++);
				*q = '\0';

				/* get port */
				for (i = 0; *++p != '\0' && *p != ' '; i++);
				if ( i = 0) {
					port = MYSQL_PORT;
				} else {
					char	*dummy;

					dummy = (char *) calloc(1, i + 1);
					for (q = dummy; i--; *q++ = *p++)
					*q = '\0';
					port = atoi(dummy);
					free (dummy);
				}

				break;
			case 'u':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				user = (char *) calloc(1, i + 1);
				for (q = user; i--; *q++ = *p++);
				*q = '\0';
				break;
			case 'p':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				passwd = (char *) calloc(1, i + 1);
				for (q = passwd; i--; *q++ = *p++);
				*q = '\0';
				break;
			case 'd':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				db = (char *) calloc(1, i + 1);
				for (q = db; i--; *q++ = *p++);
				*q = '\0';
				break;
			case 't':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				table = (char *) calloc(1, i + 1);
				for (q = table; i--; *q++ = *p++);
				*q = '\0';
				break;
			case 'c':
				createTable++;
				break;
			case 'd':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				date_col = (char *) calloc(1, i + 1);
				for (q = date_col; i--; *q++ = *p++);
				*q = '\0';
				break;
			case 'n':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				program_col = (char *) calloc(1, i + 1);
				for (q = program_col; i--; *q++ = *p++);
				*q = '\0';
				break;
			case 'h':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				host_col = (char *) calloc(1, i + 1);
				for (q = host_col; i--; *q++ = *p++);
				*q = '\0';
				break;
			case 'm':
				for (i = 0; *++p != '\0' && *p != ':'; i++);
				msg_col = (char *) calloc(1, i + 1);
				for (q = msg_col; i--; *q++ = *p++);
				*q = '\0';
				break;
			default:
				break;
		}
	}

	if ( user == NULL || passwd == NULL || *db == NULL || 
			ux_sock == NULL || port == 0)
		return (-3);

	/* clean up the mess */
	if (!mysql_init(&h)) {
		dprintf("Error initializing handle\n");
		return(-4);
	}
	if (!mysql_real_connect(h, host, user, passwd, db, port,
			unix_socket, client_flag)) {
		dprintf("Error connecting to db server [%s:%i] user [%s] db [%s]\n",
				host, port, user, db);
		return(-5);
	}

	if (host != NULL) free(host); if (user != NULL) free(user);
	if (passwd != NULL) free(passwd); if (db != NULL) free(db);
	if (date_col != NULL) free(date_col); if (program_col != NULL) free(program_col);
	if (host_col != NULL) free(host_col); if (msg_col != NULL) free(msg_col);

	/* save handle on context */
	*c = (void *) calloc(1, sizeof(struct m_mysql_ctx));
	context = (struct m_mysql_ctx *) *c;

	return (0);
}

int
m_mysql_close(f, context)
	struct filed *f;
	void *context;
{
	return (-1);
}

int
m_mysql_flush(f, context)
	struct filed *f;
	void *context;
{
	return (-1);
}

