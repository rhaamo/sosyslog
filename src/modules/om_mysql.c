/*	$CoreSDI: om_mysql.c,v 1.38 2000/09/04 23:43:45 alejo Exp $	*/

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
 * om_mysql -- MySQL database syupport Module
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <syslog.h>
#include <unistd.h>
#include <mysql.h>
#include "../config.h"
#include "../modules.h"
#include "../syslogd.h"
#include "sql_misc.h"

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
om_mysql_doLog(struct filed *f, int flags, char *msg, struct om_hdr_ctx *ctx) {
	struct om_mysql_ctx *c;
	char	*dummy, *y, *m, *d, *h, *host, *msg_q;
	time_t now;

	if (!ctx) {
		dprintf("MySQL doLog: error, no context\n");
		return(-1);
	}

	if (f == NULL)
		return (-1);

	c = (struct om_mysql_ctx *) ctx;
	if (!(c->h)) {
		dprintf("MySQL doLog: error, handle\n");
		return(-1);
	}

	dprintf("MySQL doLog: entering [%s] [%s]\n", msg, f->f_prevline);

	memset(c->query, 0, MAX_QUERY);

	host = f->f_prevhost;

	/* mysql needs 2000-01-25 like format */
	if (NULL==(dummy = strdup(f->f_lasttime))) return -1;
	*(dummy + 3)  = '\0'; *(dummy + 6)  = '\0';
	*(dummy + 15) = '\0';
	m = dummy;
	d = dummy + 4;
	h = dummy + 7;


	(void) time(&now);
	if (NULL==(y = strdup(ctime(&now) + 20))) {
		free(dummy);
		return -1;
	}

	*(y + 4) = '\0';
	if (*d == ' ')
		*d = '0';

	if (NULL==(msg_q=to_sql(msg))) {
		free(dummy);
		free(y);
		return -1;
	}
	/* table, YYYY-Mmm-dd, hh:mm:ss, host, msg  */ 
	snprintf(c->query, MAX_QUERY - 2, "INSERT INTO %s"
			" VALUES('%s-%.2d-%s', '%s', '%s', '%s')",
			c->table, y, month_number(m), d, h, host, msg_q);

	free(msg_q);
	free(dummy);
	free(y);

	return (mysql_query(c->h, c->query) < 0? -1 : 1);
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

extern char *optarg;
extern int optind;
#ifdef HAVE_OPTRESET
extern int optreset;
#endif

int
om_mysql_init(int argc, char **argv, struct filed *f, char *prog,
		struct om_hdr_ctx **c) {
	MYSQL *h;
	struct om_mysql_ctx	*ctx;
	char	*host, *user, *passwd, *db, *table, *p, *query;
	int	port, client_flag, createTable, ch;

	dprintf("MySQL: entering initialization\n");

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
			c == NULL)
		return (-1);

	h = (MYSQL *) calloc(1, sizeof(MYSQL));
	user = NULL; passwd = NULL; db = NULL; port = 0; host = NULL; table = NULL;
	client_flag = 0; createTable = 0;

	/* parse line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
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
				if (!host) return -1;
				break;
			case 'u':
				user = strdup(optarg);
				if (!user) return -1;
				break;
			case 'p':
				passwd = strdup(optarg);
				if (!passwd) return -1;
				break;
			case 'd':
				db = strdup(optarg);
				if (!db) return -1;
				break;
			case 't':
				table = strdup(optarg);
				if (!table) return -1;
				break;
			case 'c':
				createTable++;
				break;
			default:
				return(-1);
		}
	}

	if (user == NULL || passwd == NULL || db == NULL || port == 0 ||
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
	if (! (*c = (struct om_hdr_ctx *)
		calloc(1, sizeof(struct om_mysql_ctx))))
		return (-1);

	if (! (query = (char *) calloc(1, MAX_QUERY)))
		return (-1);

	ctx = (struct om_mysql_ctx *) *c;
	ctx->size = sizeof(struct om_mysql_ctx);
	ctx->h = h;
	ctx->host = host;
	ctx->port = port;
	ctx->user = user;
	ctx->passwd = passwd;
	ctx->db = db;
	ctx->table = table;
	ctx->query = query;

	return (1);
}

void
om_mysql_destroy_ctx(ctx)
	struct om_mysql_ctx *ctx;
{
	free(ctx->h);
	free(ctx->host);
	free(ctx->user);
	free(ctx->passwd);
	free(ctx->db);
	free(ctx->table);
	free(ctx->query);
}

int
om_mysql_close(struct filed *f, struct om_hdr_ctx *ctx) {
	if (((struct om_mysql_ctx*)ctx)->h) {
		mysql_close(((struct om_mysql_ctx*)ctx)->h);
		om_mysql_destroy_ctx((struct om_mysql_ctx*)ctx);
	}

	return (-1);
}
