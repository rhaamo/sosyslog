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
static char rcsid[] = "$Id: om_mysql.c,v 1.6 2000/04/19 20:47:27 alejo Exp $";
#endif /* not lint */

/*
 *  om_mysql -- MySQL database syupport Module
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
#include <sys/types.h>
#include <time.h>

#include "syslogd.h"
#include "modules.h"

#include "/usr/local/include/mysql/mysql.h"

#define MAX_QUERY	8192

struct om_mysql_ctx {
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
om_mysql_doLog(f, flags, msg, context)
	struct filed *f;
	int flags;
	char *msg;
	struct om_header_ctx *context;
{
	struct om_mysql_ctx *c;
	char	*dummy, *y, *m, *d, *h, *host, mymsg[1024];
	int	mn;
	char	*months[] = { "Jan", "Feb", "Mar", "Apr", "May",
				"Jun", "Jul", "Aug", "Sep", "Oct",
				"Nov", "Dec"};

	dprintf("MySQL doLog: entering [%s] [%s]\n", msg, f->f_prevline);
	if (f == NULL)
		return (-1);

	c = (struct om_mysql_ctx *) context;
	memset(c->query, 0, MAX_QUERY);

        if (msg == NULL) {
        	if (f->f_prevcount > 1) {
                	sprintf(mymsg, "last message repeated %d times",
                    			f->f_prevcount);
		} else {
                	sprintf(mymsg, "%s", f->f_prevline);
        	}
        }

	host = f->f_prevhost;

	/* mysql needs 2000-01-25 like format */
	dummy = strdup(f->f_lasttime);
	*(dummy + 3)  = '\0'; *(dummy + 6)  = '\0';
	*(dummy + 15) = '\0'; *(dummy + 20) = '\0';
	m = dummy;
	d = dummy + 4;
	h = dummy + 7;
	y = strdup(dummy + 16);

	if(strcmp(y, "") == 0) {
		time_t now;

		(void) time(&now);
		y = strdup(ctime(&now) + 20);
	}

	*(y + 4) = '\0';
	if (*d == ' ')
		*d = '0';
	for(mn = 0; months[mn] && strncmp(months[mn], m, 3); mn++);
	mn++;

	/* table, YYYY-Mmm-dd, hh:mm:ss, host, msg  */ 
	snprintf(c->query, MAX_QUERY - 2, "INSERT INTO %s"
			" VALUES('%s-%.2d-%s', '%s', '%s', '%s')",
			c->table, y, mn, d, h, host, mymsg);

	free(dummy);
	free(y);

	return (mysql_query(c->h, c->query));
}


/*
 *  INIT -- Initialize om_mysql
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

extern char *oprtarg;

int
om_mysql_init(argc, argv, f, prog, c)
	int	argc;
	char	**argv;
	struct filed *f;
	char *prog;
	struct om_header_ctx **c;
{
	MYSQL *h;
	struct om_mysql_ctx	*context;
	char	*host, *user, *passwd, *db, *table, *p;
	int	port, client_flag, createTable;
	int	ch;

	dprintf("MySQL: entering initialization\n");

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
			c == NULL)
		return (-1);

	h = (MYSQL *) calloc(1, sizeof(MYSQL));
	user = NULL; passwd = NULL; db = NULL; port = 0; host = NULL; table = NULL;
	client_flag = 0; createTable = 0;

	/* parse line */
	optreset = 1; optind = 0;
	while ((ch = getopt(argc, argv, "s:u:p:d:t:c")) != -1) {
		switch (ch) {
			case 's':
				/* get database host name and port */
				if ((p = strstr(optarg, ":")) == NULL) {
					port = MYSQL_PORT;
				} else {
					*p = '\0';
					port = atoi(++p);
				}
				host = strdup(optarg);
				break;
			case 'u':
				user = strdup(optarg);
				break;
					break;
			case 'p':
				passwd = strdup(optarg);
				break;
			case 'd':
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

	if ( user == NULL || passwd == NULL || db == NULL || port == 0 ||
			host == NULL || table == NULL)
		return (-3);

	/* connect to the database */
	if (!mysql_init(h)) {
		dprintf("Error initializing handle\n");
		return(-4);
	}
	if (!mysql_real_connect(h, host, user, passwd, db, port,
			NULL, client_flag)) {
		dprintf("MySQL module: Error connecting to db server"
			" [%s:%i] user [%s] db [%s]\n",
				host, port, user, db);
		return(-5);
	}

	/* save handle and stuff on context */
	*c = (struct om_header_ctx *) calloc(1, sizeof(struct om_mysql_ctx));

	context = (struct om_mysql_ctx *) *c;
	context->size = sizeof(struct om_mysql_ctx);
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
om_mysql_destroy_ctx(context)
	struct om_mysql_ctx *context;
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
om_mysql_close(f, context)
	struct filed *f;
	struct om_header_ctx **context;
{
	struct om_mysql_ctx *c;

	c = (struct om_mysql_ctx *) *context;
	mysql_close(c->h);
	om_mysql_destroy_ctx(c);
	free(*context);
	context = NULL;

	return (-1);
}

int
om_mysql_flush(f, context)
	struct filed *f;
	struct om_header_ctx *context;
{
	/* this module doesn't need to "flush" data */
	return (0);
}

