/*	$CoreSDI: om_pgsql.c,v 1.7 2000/05/29 23:39:10 fgsch Exp $	*/

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
 */

#include <sys/types.h>
#include <sys/syslog.h>
#include <sys/time.h>
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

#include <libpq-fe.h>

#define MAX_QUERY     8192

char  *Months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
		    "Aug", "Sep", "Oct", "Nov", "Dec" };

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

static char *
to_sql(s)
	char *s;
{
	char *p, *b;
	int ns, ss;

	if(!s)
		return strdup("NULL");
	
	if(s[0]=='-' && s[1]=='0')
		return strdup("NULL");

	for(p=s, ns=0; *p; p++) 
		if(*p=='\'') ns++;

	ss=(int)(p-s);

	b=malloc(ss+ns+4);
	if(!b)
		return NULL;

	p=b; *p++='\'';
	for(;*s; s++)
	{
		if(*s=='\'') {
			*p++='\'';
			*p++='\'';
		} else
			*p++=*s;
	}
	*p++='\'';
	*p=0;
	
	return b;
}



int
om_pgsql_doLog(f, flags, msg, ctx)
	struct filed *f;
	int flags;
	char *msg;
	struct om_hdr_ctx *ctx;
{
	PGresult *r;
	struct om_pgsql_ctx *c;
	char    *dummy, *y, *m, *d, *h, *host;
	int     mn, err;

	dprintf("PostgreSQL doLog: entering [%s] [%s]\n", msg, f->f_prevline);
	if (f == NULL)
		return (-1);

	c = (struct om_pgsql_ctx *) ctx;
	/*
	memset(c->query, 0, MAX_QUERY);
	*/

	host = f->f_prevhost;

	/* PostgreSQL needs 2000-01-25 like format */
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
	for(mn = 0; Months[mn] && strncmp(Months[mn], m, 3); mn++);
	mn++;


	m=to_sql(msg);
	if(!m)
		return (-1);

	/* table, YYYY-Mmm-dd, hh:mm:ss, host, msg  */ 
	snprintf(c->query, MAX_QUERY - 2, "INSERT INTO %s"
			" VALUES('%s-%02d-%s', '%s', '%s', %s)",
			c->table, y, mn, d, h, host, m);

	free(m);
	free(dummy);
	free(y); 

	err=0;
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
om_pgsql_init(argc, argv, f, prog, c)
	int     argc;
	char    **argv;
	struct filed *f;
	char *prog;
	struct om_header_ctx **c;
{
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
	if (! (*c = (struct om_header_ctx *)
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
om_pgsql_close(f, ctx)
	struct filed *f;
	struct om_header_ctx *ctx;
{
	PQfinish(((struct om_pgsql_ctx *)ctx)->h);
	om_pgsql_destroy_ctx(ctx);

	return (-1);
}

int
om_pgsql_flush(f, ctx)
	struct filed *f;
	struct om_header_ctx *ctx;
{
	/* this module doesn't need to "flush" data */
	return (0);
}

