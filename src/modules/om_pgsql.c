/*	$CoreSDI: om_pgsql.c,v 1.16 2000/07/10 21:01:53 claudio Exp $	*/

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
 * om_pgsql -- PostgreSQL database support Module
 *
 * Author: Oliver Teuber (ot@penguin-power.de)
 *         Based on om_mysql from Alejo Sanchez
 *
 * Changes:
 *
 * 06/08/2000 - Gerardo_Richarte@core-sdi.com
 *   Moved to_sql() to sql_misc.c to reuse it in om_mysql
 *   removed some code regarding msg being NULL, this is checked before calling
 *   doLog
 */

#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/time.h>
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "config.h"
#include "modules.h"
#include "syslogd.h"
#include "sql_misc.h"

#include <libpq-fe.h>

#define MAX_QUERY     8192

struct om_pgsql_ctx {
	short   flags;
	int     size;
	PGconn  *h;
	char    *host;
	char    *port;
	char    *user;
	char    *passwd;
	char    *db;
	char    *table;
	char    *query;         /* speed hack: this is a buffer for querys */
};

int
om_pgsql_doLog(struct filed *f, int flags, char *msg,
	struct om_hdr_ctx *ctx, struct sglobals *sglobals) {
	PGresult *r;
	struct om_pgsql_ctx *c;
	char    *dummy, *y, *mo, *d, *h, *host, *m;
	int     err;

	dprintf("PostgreSQL doLog: entering [%s] [%s]\n", msg, f->f_prevline);

	c = (struct om_pgsql_ctx *) ctx;
	if ( !(f  && c && c->h)) {
		dprintf("om_pgsql: Error, handle not working\n");
		return (-1);
	}

	/*
	memset(c->query, 0, MAX_QUERY);
	*/

	host = f->f_prevhost;

	/* PostgreSQL needs 2000-01-25 like format */
	dummy = strdup(f->f_lasttime);
	*(dummy + 3)  = '\0'; *(dummy + 6)  = '\0';
	*(dummy + 15) = '\0'; *(dummy + 20) = '\0';
	mo = dummy;
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

	m=to_sql(msg);
	if(!m)
		return (-1);

	/* table, YYYY-Mmm-dd, hh:mm:ss, host, msg  */ 
	snprintf(c->query, MAX_QUERY - 2, "INSERT INTO %s"
			" VALUES('%s-%02d-%s', '%s', '%s', '%s')",
			c->table, y, month_number(mo), d, h, host, m);

	free(m);
	free(dummy);
	free(y); 

	err=1;
	r=PQexec(c->h, c->query);
	if(PQresultStatus(r) != PGRES_COMMAND_OK)
	{
		fprintf(stderr,"%s\n",PQresultErrorMessage(r));
		err=-1;
	}
	PQclear(r);

	return (err);
}


/*
 *  INIT -- Initialize om_pgsql
 *
 *  Parse options and connect to database
 *
 *  params:
 *         -s <host:port>
 *         -u <user>
 *         -p <password>
 *         -b <database_name>
 *         -t <table_name>
 *         -c                 create table
 *
 */

extern char *optarg;
extern int  optind;
#ifdef HAVE_OPTRESET
extern int optreset;
#endif



int
om_pgsql_init(int argc, char **argv, struct filed *f, char *prog,
	struct om_hdr_ctx **c, struct sglobals *sglobals) {
	PGconn  *h;
	PGresult *r;
	struct  om_pgsql_ctx *ctx;
	char    *host, *user, *passwd, *db, *table, *port, *p, *query;
	int     client_flag, createTable;
	int     ch;

	dprintf("PostgreSQL: entering initialization\n");

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
		 c == NULL)
		return (-1);

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
						port = NULL;
					} else {
						*p = '\0';
						port = strdup(++p);
					}
					host = strdup(optarg);
					break;
			case 'u':
					user = strdup(optarg);
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

	if (user == NULL || passwd == NULL || db == NULL || table == NULL)
		return (-3);

	/* connect to the database */

	h = PQsetdbLogin( host, port,
			  NULL, NULL,
			  db,
			  user,passwd);

	/* check to see that the backend connection was successfully made */
	if (PQstatus(h) == CONNECTION_BAD)
	{
		dprintf("PostgreSQL module: Error connecting to db server"
			" [%s:%s] user [%s] db [%s]\n",
				host?host:"(unix socket)", port?port:"(none)", user, db);
		PQfinish(h); 
		return(-5);
	}

	if (! (query = (char *) calloc(1, MAX_QUERY)))
		return (-1);

	if(createTable)
	{
		snprintf(query,MAX_QUERY - 2,"CREATE TABLE %s ( logdate DATE, logtime TIME, host VARCHAR(64), msg TEXT )",table);
		r=PQexec(h, query);
		if(PQresultStatus(r) != PGRES_COMMAND_OK)
		{
			dprintf("PostgreSQL module: CREATE TABLE (%s)\n",PQresultErrorMessage(r));
			PQclear(r);
			PQfinish(h); 
			free(query);

			return (-5);
		}
		PQclear(r);
	}


	/* save handle and stuff on context */
	if (! (*c = (struct om_hdr_ctx *)
		calloc(1, sizeof(struct om_pgsql_ctx))))
		return (-1);


	ctx = (struct om_pgsql_ctx *) *c;
	ctx->size = sizeof(struct om_pgsql_ctx);
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
om_pgsql_destroy_ctx(ctx)
	struct om_pgsql_ctx *ctx;
{
	if(ctx->host)
		free(ctx->host);

	if(ctx->port)
		free(ctx->port);

	free(ctx->user);
	free(ctx->passwd);
	free(ctx->db);
	free(ctx->table);

	if(ctx->query)
		free(ctx->query);
}

int
om_pgsql_close(struct filed *f, struct om_hdr_ctx *ctx,
		struct sglobals *sglobals) {
	PQfinish(((struct om_pgsql_ctx *)ctx)->h);
	om_pgsql_destroy_ctx(ctx);

	return (-1);
}
