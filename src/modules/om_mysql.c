/*	$CoreSDI: om_mysql.c,v 1.36.2.4.2.3.4.19 2000/10/30 23:14:21 alejo Exp $	*/

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
 * om_mysql -- MySQL database support Module
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

/* size of  query buffer */
#define MAX_QUERY	8192
/* how many seconds to wait to give again the error message */
#define MSYSLOG_MYSQL_ERROR_DELAY	30

struct om_mysql_ctx {
	MYSQL	h;
	int	lost;
	char	*table;
	char	*host;
	int	port;
	char	*user;
	char	*passwd;
	char	*db;
};

int
om_mysql_doLog(struct filed *f, int flags, char *msg, void *ctx)
{
	struct om_mysql_ctx *c;
	char	query[MAX_QUERY], err_buf[100];
	int i, j;
	void (*sigsave)(int);

	if (ctx == NULL) {
		dprintf("om_mysql_dolog: error, no context\n");
		return (-1);
	}

	if (f == NULL)
		return (-1);

	dprintf("om_mysql_dolog: entering [%s] [%s]\n", msg, f->f_prevline);

	c = (struct om_mysql_ctx *) ctx;

	/* ignore sigpipes   for mysql_ping */
	sigsave = signal(SIGPIPE, SIG_IGN);

	if ((mysql_ping(&c->h) != 0) && ((mysql_init(&c->h) == NULL) ||
	    (mysql_real_connect(&c->h, c->host, c->user, c->passwd, c->db,
	    c->port, NULL, 0) == NULL))) {

		/* restore previous SIGPIPE handler */
		signal(SIGPIPE, sigsave);
		c->lost++;
		if (c->lost == 1) {
			dprintf("om_mysql_dolog: Lost connection!");
			logerror("om_mysql_dolog: Lost connection!");
		}
		return(1);
	}

	/* restore previous SIGPIPE handler */
	signal(SIGPIPE, sigsave);

	if (c->lost) {

		/*
		 * Report lost messages, but 2 of them are lost of
		 * connection and this one (wich we are going
		 * to log anyway)
		 */
		snprintf(err_buf, sizeof(err_buf), "om_mysql_dolog: %i "
		    "messages were lost due to lack of connection",
		    c->lost - 2);

		/* count reset */
		c->lost = 0;

		/* table, YYYY-Mmm-dd, hh:mm:ss, host, msg  */ 
		i = snprintf(query, sizeof(query), "INSERT INTO %s"
		    " VALUES('%.4d-%.2d-%.2d', '%.2d:%.2d:%.2d', '%s', '", c->table,
		    f->f_tm.tm_year + 1900, f->f_tm.tm_mon + 1, f->f_tm.tm_mday,
		    f->f_tm.tm_hour, f->f_tm.tm_min, f->f_tm.tm_sec, f->f_prevhost);

		/* put message escaping special SQL characters */
		i += to_sql(query + i, err_buf, sizeof(query) - i);

		/* finish it with "')" */
		query[i++] =  '\'';
		query[i++] =  ')';
		if (i < sizeof(query))
			query[i]   =  '\0';

		/* just in case */
		query[sizeof(query) - 1] = '\0';

		dprintf("om_mysql_doLog: query [%s]\n", query);

		if (mysql_query(&c->h, query) < 0)
			return (-1);
	}

	/* table, YYYY-Mmm-dd, hh:mm:ss, host, msg  */ 
	i = snprintf(query, sizeof(query), "INSERT INTO %s"
	    " VALUES('%.4d-%.2d-%.2d', '%.2d:%.2d:%.2d', '%s', '", c->table,
	    f->f_tm.tm_year + 1900, f->f_tm.tm_mon + 1, f->f_tm.tm_mday,
	    f->f_tm.tm_hour, f->f_tm.tm_min, f->f_tm.tm_sec, f->f_prevhost);

	/* put message escaping special SQL characters */
	i += to_sql(query + i, msg, sizeof(query) - i);

	/* finish it with "')" */
	query[i++] =  '\'';
	query[i++] =  ')';
	if (i < sizeof(query))
		query[i]   =  '\0';

	/* just in case */
	query[sizeof(query) - 1] = '\0';

	dprintf("om_mysql_doLog: query [%s]\n", query);

	return (mysql_query(&c->h, query) < 0? -1 : 1);
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
 *
 */

int
om_mysql_init(int argc, char **argv, struct filed *f, char *prog, void **c)
{
	struct om_mysql_ctx	*ctx;
	char	*host, *user, *passwd, *db, *table, *p, err_buf[256];
	int	port, ch;

	dprintf("om_mysql_init: entering initialization\n");

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
	    c == NULL || *c != NULL)
		return (-1);

	user = NULL; passwd = NULL; db = NULL; port = 0;
	host = NULL; table = NULL;

	/* parse line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "s:u:p:d:t:")) != -1) {
		switch (ch) {
		case 's':
			/* get database host name and port */
			if ((p = strstr(optarg, ":")) == NULL) {
				port = MYSQL_PORT;
			} else {
				*p = '\0';
				port = atoi(++p);
			}
			host = optarg;
			break;
		case 'u':
			user = optarg;
			break;
		case 'p':
			passwd = optarg;
			break;
		case 'd':
			db = optarg;
			break;
		case 't':
			table = optarg;
			break;
		default:
			return(-1);
		}
	}

	if (user == NULL || passwd == NULL || db == NULL || port == 0 ||
	    host == NULL || table == NULL)
		return (-1);

	/* alloc context */
	if ((*c = (void *) calloc(1, sizeof(struct om_mysql_ctx))) == NULL)
		return(-1);
	ctx = (struct om_mysql_ctx *) *c;

	ctx->table = strdup(table);
	ctx->host = strdup(host);
	ctx->port = port;
	ctx->user = strdup(user);
	ctx->passwd = strdup(passwd);
	ctx->db = strdup(db);

	/* connect to the database */
	if (!mysql_init(&ctx->h)) {

		snprintf(err_buf, sizeof(err_buf), "om_mysql_init: Error "
		    "initializing handle");
		logerror(err_buf);
		return(-1);
	}

	if (!mysql_real_connect(&ctx->h, host, user, passwd, db, port,
	    NULL, 0)) {

		snprintf(err_buf, sizeof(err_buf), "om_mysql_init: Error "
		    "connecting to db server [%s:%i] user [%s] db [%s]",
		    host, port, user, db);
		logerror(err_buf);
		return(1);
	}

	return (1);

}


int
om_mysql_close(struct filed *f, void *ctx)
{
	struct om_mysql_ctx *c;

	c = (struct om_mysql_ctx*) ctx;

	if (c->table)
		free(c->table);
	if (c->table)
		free(c->table);
	if (c->host)
		free(c->host);
	if (c->user)
		free(c->user);
	if (c->passwd)
		free(c->passwd);
	if (c->db)
		free(c->db);

	return (0);
}
