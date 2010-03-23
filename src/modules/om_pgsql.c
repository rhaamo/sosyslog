/*	$CoreSDI: om_pgsql.c,v 1.53 2002/03/01 07:31:03 alejo Exp $	*/

/*
 * Copyright (c) 2001, Core SDI S.A., Argentina
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
 * 06/08/2000 - Gerardo_Richarte@corest.com
 *   Moved to_sql() to sql_misc.c to reuse it in om_mysql
 *   removed some code regarding msg being NULL, this is checked before calling
 *   write
 * 10/10/2000 - Federico Schwindt
 *   Cleanup code
 * 10/12/2000 - Alejo Sanchex
 *   Move alloc query and dates to context structure
 *
 */

#include "config.h"

#include <sys/types.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include "../modules.h"
#include "../syslogd.h"
#include "sql_misc.h"

#define MAX_QUERY     8192

/*
 * Define needed PostgreSQL functions
 */

typedef enum
{
	CONNECTION_OK,
	CONNECTION_BAD
} Some_psql_needed_ConnStatusType;
typedef enum
{
	PGRES_EMPTY_QUERY = 0,
	PGRES_COMMAND_OK
/* all other enums are not needed */
} ExecStatusType;



struct om_pgsql_ctx {
	void	*h;
	char	*table;
	int	lost;
	void	*lib;
	int	(*PQstatus)(void *);
	char *	(*PQerrorMessage)(void *);
	int	(*PQresultStatus)(void *);
	void	(*PQreset)(void *);
	void *	(*PQexec)(void *, char *);
	char *	(*PQresultErrorMessage)(void *);
	void	(*PQclear)(void *);
	void *	(*PQsetdbLogin)(char *, char *, void *, void *,
	    char *, char *,char *);
	void	(*PQfinish)(void *); 
	int	flags;
};
#define	OM_PGSQL_DELAYED_INSERTS	0x2
#define	OM_PGSQL_FACILITY		0x4
#define	OM_PGSQL_PRIORITY		0x8


/*
 * Send to database
 */

int
om_pgsql_write(struct filed *f, int flags, struct m_msg *m, void *ctx)
{
	void	*r;
	struct	om_pgsql_ctx *c;
	int	err, i;
	char    query[MAX_QUERY], err_buf[512], facility[18], priority[18], *p;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_pgsql_write: entering [%s] [%s]\n",
	    m->msg, f->f_prevline);

	c = (struct om_pgsql_ctx *) ctx;

	if ((c->h) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_pgsql_write: error, no "
		    "connection\n");
		return (-1);
	}

	if ((c->PQstatus(c->h)) == CONNECTION_BAD) {

		/* try to reconnect */
		(c->PQreset(c->h));

		/* connection can't be established */
		if ((c->PQstatus(c->h)) == CONNECTION_BAD) {

			c->lost++;
			if (c->lost == 1) {
				logerror("om_pgsql_write: Lost connection!");
			}
		}
		return (1);

	}

	if (c->flags & OM_PGSQL_FACILITY) {
		if ((p = decode_val(m->fac<<3, CODE_FACILITY)) != NULL)
			snprintf(facility, sizeof(facility), "'%s',", p);
		else
			snprintf(facility, sizeof(facility), "'%d',", m->fac<<3);
	}

	if (c->flags & OM_PGSQL_PRIORITY) {
		if ((p = decode_val(m->pri, CODE_PRIORITY)) != NULL)
			snprintf(priority, sizeof(priority), "'%s',", p);
		else
			snprintf(priority, sizeof(priority), "'%d',", m->pri);
	}

	/* table, YYYY-Mmm-dd, hh:mm:ss, host, msg  */ 
	i = snprintf(query, sizeof(query), "INSERT INTO %s (%s%sdate, time, "
	    "host, message) VALUES(%s%s'%.4d-%.2d-%.2d', '%.2d:%.2d:%.2d', '%s', '",
	    c->table,
	    (c->flags & OM_PGSQL_FACILITY)? "facility, " : "",
	    (c->flags & OM_PGSQL_PRIORITY)? "priority, " : "",
	    (c->flags & OM_PGSQL_FACILITY)? facility : "",
	    (c->flags & OM_PGSQL_PRIORITY)? priority : "",
	    f->f_tm.tm_year + 1900, f->f_tm.tm_mon + 1,
	    f->f_tm.tm_mday, f->f_tm.tm_hour, f->f_tm.tm_min, f->f_tm.tm_sec,
	    f->f_prevhost);

	if (c->lost) {
		int pos = i;

		/*
		 * Report lost messages, but 2 of them are lost of
		 * connection and this one (which we are going
		 * to log anyway)
		 */
		snprintf(err_buf, sizeof(err_buf), "om_pgsql_write: %i "
		    "messages were lost due to lack of connection",
		    c->lost - 2);

		/* count reset */
		c->lost = 0;

		/* put message escaping special SQL characters */
		pos += to_sql(query + pos, err_buf, sizeof(query) - pos);

		/* finish it with "')" */
		query[pos++] =  '\'';
		query[pos++] =  ')';
		if (pos < sizeof(query))
			query[pos]   =  '\0';
		else
			query[sizeof(query) - 1] = '\0';

		m_dprintf(MSYSLOG_INFORMATIVE2, "om_pgsql_write: query [%s]\n",
		    query);

		r = (c->PQexec(c->h, query));
		if ((c->PQresultStatus(r)) != PGRES_COMMAND_OK) {
			m_dprintf(MSYSLOG_SERIOUS, "om_pgsql_write: %s\n",
			    (c->PQresultErrorMessage(r)));
			return (-1);
		}
		(c->PQclear(r));
	}

	/* put message escaping special SQL characters */
	i += to_sql(query + i, m->msg, sizeof(query) - i);

	/* finish it with "')" */
	query[i++] =  '\'';
	query[i++] =  ')';
	if (i < sizeof(query))
		query[i]   =  '\0';
	else
		query[sizeof(query) - 1] = '\0';

	m_dprintf(MSYSLOG_INFORMATIVE2, "om_pgsql_write: query [%s]\n", query);

	err = 1;
	r = (c->PQexec(c->h, query));
	if ((c->PQresultStatus(r)) != PGRES_COMMAND_OK) {
		m_dprintf(MSYSLOG_INFORMATIVE, "%s\n",
		    (c->PQresultErrorMessage(r)));
		err = -1;
	}

	(c->PQclear(r));

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
 *
 */

int
om_pgsql_init(int argc, char **argv, struct filed *f, char *prog, void **c,
    char **status)
{
	char			statbuf[1024];
	void			*h;
	struct om_pgsql_ctx	*ctx;
	char			*host, *user, *passwd, *db, *table, *port, *p;
	int			ch;
	int			argcnt;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_pgsql_init: entering "
	    "initialization\n");

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
		 c == NULL)
		return (-1);

	user = NULL; passwd = NULL; db = NULL; port = 0; host = NULL;
	table = NULL;

	/* save handle and stuff on context */
	if (! (*c = calloc(1, sizeof(struct om_pgsql_ctx))))
		return (-1);
	ctx = (struct om_pgsql_ctx *) *c;

	if ((ctx->lib = dlopen("libpq.so", DLOPEN_FLAGS)) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_pgsql_init: Error loading"
		    " api library, %s\n", dlerror());
		free(ctx);
		return (-1);
	}

	if ( !(ctx->PQstatus = (int (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "PQstatus"))   
	    || !(ctx->PQerrorMessage = (char * (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "PQerrorMessage"))   
	    || !(ctx->PQresultStatus = (int (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "PQresultStatus"))   
	    || !(ctx->PQreset = (void (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "PQreset"))   
	    || !(ctx->PQexec = (void * (*)(void *, char *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "PQexec"))   
	    || !(ctx->PQresultErrorMessage = (char * (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX
	    "PQresultErrorMessage"))   
	    || !(ctx->PQclear = (void (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "PQclear"))   
	    || !(ctx->PQsetdbLogin = (void * (*)(char *, char *, void *,
	    void *, char *, char *, char *)) dlsym(ctx->lib, SYMBOL_PREFIX
	    "PQsetdbLogin"))   
	    || !(ctx->PQfinish = (void (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "PQfinish"))) {
		m_dprintf(MSYSLOG_SERIOUS, "om_pgsql_init: Error resolving"
		    " api symbols, %s\n", dlerror());
		free(ctx);
		return (-1);
	}

	argcnt = 1;	/* skip module name */

	while ((ch = getxopt(argc, argv, "s!server: u!user: p!password:"
	    " d!database: t!table: c!create F!facility P!priority", &argcnt))
	    != -1) {

		switch (ch) {
		case 's':
			/* get database host name and port */
			if ((p = strstr(argv[argcnt], ":")) == NULL) {
				port = NULL;
			} else {
				*p = '\0';
				port = ++p;
			}
			host = argv[argcnt];
			break;
		case 'u':
			user = argv[argcnt];
			break;
		case 'p':
			passwd = argv[argcnt];
			break;
		case 'd':
			db = argv[argcnt];
			break;
		case 't':
			table = argv[argcnt];
			break;
		case 'F':
			ctx->flags |= OM_PGSQL_FACILITY;
			break;
		case 'P':
			ctx->flags |= OM_PGSQL_PRIORITY;
			break;
		case 'c':
			m_dprintf(MSYSLOG_INFORMATIVE, "(om_pgsql_init: "
			    "TABLE CREATION NOT SUPPORTED ANYMORE)\n");
			break;
		default:
			m_dprintf(MSYSLOG_INFORMATIVE, "(om_pgsql_init: "
			    "error on parameter '%c')\n", ch);
			return (-1);
		}
		argcnt++;
	}

	if (user == NULL || db == NULL || table == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_pgsql_init: Error missing "
		    "params!\n");
		dlclose(ctx->lib);
		free(ctx);
		return (-1);
	}

	/* connect to the database */

	h = (ctx->PQsetdbLogin)(host, port, NULL, NULL, db, user,passwd);

	/* check to see that the backend connection was successfully made */
	if ((ctx->PQstatus)(h) == CONNECTION_BAD) {
		m_dprintf(MSYSLOG_SERIOUS, "om_pgsql_init: Error connecting "
		    "to db server [%s:%s] user [%s] db [%s] error[%s]\n",
		    host?host:"(unix socket)", port?port:"(none)", user, db,
		    ctx->PQerrorMessage(h));
		(ctx->PQfinish)(h); 
		dlclose(ctx->lib);
		free(ctx);
		return (-1);
	}

	ctx->h = h;
	ctx->table = strdup(table);

	snprintf(statbuf, sizeof(statbuf), "om_pgsql: sending "
	    "messages to host %s, database %s, table %s", host,
	    db, table);
	*status = strdup(statbuf);

	return (1);
}

int
om_pgsql_close(struct filed *f, void *ctx) {
	struct om_pgsql_ctx *c;

	c = (struct om_pgsql_ctx *) ctx;

	if (((struct om_pgsql_ctx *)ctx)->h) {
		(c->PQfinish)(((struct om_pgsql_ctx *)ctx)->h);
	}

	if (c->table)
		free(c->table);

	return (0);
}

