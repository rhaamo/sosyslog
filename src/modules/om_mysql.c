/*	$CoreSDI: om_mysql.c,v 1.78 2001/10/22 22:49:42 alejo Exp $	*/

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
 * om_mysql -- MySQL database support Module
 *
 * Author: Alejo Sanchez for Core-SDI SA
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
#include <signal.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SYSLOG_NAMES
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include "../modules.h"
#include "../syslogd.h"
#include "sql_misc.h"

/* size of  query buffer */
#define MAX_QUERY	8192
/* how many seconds to wait to give again the error message */
#define MSYSLOG_MYSQL_ERROR_DELAY	30

struct om_mysql_ctx {
	void	*h;
	int	lost;
	char	*table;
	char	*host;
	int	port;
	char	*user;
	char	*passwd;
	char	*db;
	void	*lib;
	int	flags;
	int	(*mysql_ping)(void *);
	void *	(*mysql_init)(void *);
	void *	(*mysql_real_connect)(void *, char *, char *, char *,
	    char *, int, void *, int);
	int	(*mysql_query)(void *, char *);
	void	(*mysql_close)(void *);
	char *	(*mysql_error)(void *);
};
#define	OM_MYSQL_DELAYED_INSERTS	0x2
#define	OM_MYSQL_FACILITY		0x4
#define	OM_MYSQL_PRIORITY		0x8

int om_mysql_close(struct filed *, void *);
char	*decode_val(int, CODE *);

/*
 * Define our prototypes for MySQL functions
 */

#define MYSQL_PORT 3306


int
om_mysql_write(struct filed *f, int flags, struct m_msg *m, void *ctx)
{
	struct om_mysql_ctx *c;
	char	query[MAX_QUERY], err_buf[100], facility[11], priority[11];
	int i;
	RETSIGTYPE (*sigsave)(int);

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mysql_write: entering [%s] [%s]\n",
	    m->msg, f->f_prevline);

	c = (struct om_mysql_ctx *) ctx;

	/* ignore sigpipes   for mysql_ping */
	sigsave = place_signal(SIGPIPE, SIG_IGN);

	if ( ((c->mysql_ping)(c->h)) != 0 && (((c->mysql_init)(c->h) == NULL)
	    || ((c->mysql_real_connect)(c->h, c->host, c->user, c->passwd, c->db,
	    c->port, NULL, 0)) == NULL) ) {

		/* restore previous SIGPIPE handler */
		place_signal(SIGPIPE, sigsave);
		c->lost++;
		if (c->lost == 1) {
			snprintf(err_buf, sizeof(err_buf), "om_mysql_write: "
			    "Lost connection! [%s]",
#ifndef mysql_error
			    (c->mysql_error)
#else
			    mysql_error
#endif
			    (c->h));
			m_dprintf(MSYSLOG_SERIOUS, "%s", err_buf);
			logerror(err_buf);
		}
		return (1);
	}

	/* restore previous SIGPIPE handler */
	place_signal(SIGPIPE, sigsave);

	/*
	 * NOTE: could use prioritynames[] and facilitynames[]
	 */
	if (c->flags & OM_MYSQL_FACILITY)
		snprintf(facility, sizeof(facility), "'%s',", decode_val(m->fac<<3, facilitynames));
	if (c->flags & OM_MYSQL_PRIORITY)
		snprintf(priority, sizeof(priority), "'%s',", decode_val(m->pri, prioritynames));

	/* table, yyyy-mm-dd, hh:mm:ss, host, msg  */ 
	i = snprintf(query, sizeof(query), "INSERT %sINTO %s (%s%s date, time, "
	    "host, message) VALUES(%s%s '%.4d-%.2d-%.2d', '%.2d:%.2d:%.2d', '%s', '",
	    (c->flags & OM_MYSQL_DELAYED_INSERTS)? "DELAYED " : "", c->table,
	    (c->flags & OM_MYSQL_FACILITY)? "facility, " : "",
	    (c->flags & OM_MYSQL_PRIORITY)? "priority, " : "",
	    (c->flags & OM_MYSQL_FACILITY)? facility : "",
	    (c->flags & OM_MYSQL_PRIORITY)? priority : "",
	    f->f_tm.tm_year + 1900, f->f_tm.tm_mon + 1, f->f_tm.tm_mday,
	    f->f_tm.tm_hour, f->f_tm.tm_min, f->f_tm.tm_sec, f->f_prevhost);

	if (c->lost) {
		int pos = i;

		/*
		 * Report lost messages, but 2 of them are lost of
		 * connection and this one (wich we are going
		 * to log anyway)
		 */
		snprintf(err_buf, sizeof(err_buf), "om_mysql_write: %i "
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

		m_dprintf(MSYSLOG_INFORMATIVE2, "om_mysql_write: query [%s]\n",
		    query);

		if ((c->mysql_query)(c->h, query) < 0)
			return (-1);
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

	m_dprintf(MSYSLOG_INFORMATIVE2, "om_mysql_write: query [%s]\n",
	    query);

	if ((i = (c->mysql_query)(c->h, query)) < 0) {
		snprintf(err_buf, sizeof(err_buf), "om_mysql_write: error "
		    "inserting on table [%s]",
#ifndef mysql_error
		    (c->mysql_error)
#else
		    mysql_error
#endif
		    (c->h));
		m_dprintf(MSYSLOG_SERIOUS, "%s\n", err_buf);
		return (-1);
	}

	return (1);
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
 *         -P
 *         -F
 *
 */

int
om_mysql_init(int argc, char **argv, struct filed *f, char *prog, void **c,
    char **status)
{
	struct om_mysql_ctx	*ctx;
	char	*p, err_buf[256], statbuf[1024];
	int	ch;

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
	    c == NULL || *c != NULL)
		return (-1);

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mysql_init: alloc context\n");
	/* alloc context */
	if ((*c = (void *) calloc(1, sizeof(struct om_mysql_ctx))) == NULL)
		return (-1);
	ctx = (struct om_mysql_ctx *) *c;

	if ((ctx->lib = dlopen("libmysqlclient.so", DLOPEN_FLAGS)) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_mysql_init: Error loading"
		    " api library, %s\n", dlerror());
		free(ctx);  
		return (-1);
	}

	if ( !(ctx->mysql_ping = (int(*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "mysql_ping")) || !(ctx->mysql_init =
	    (void * (*)(void*)) dlsym(ctx->lib, SYMBOL_PREFIX "mysql_init"))
	    || !(ctx->mysql_real_connect = (void *(*)(void *, char *, char *,
	    char *, char *, int, void *, int)) dlsym(ctx->lib, SYMBOL_PREFIX
	    "mysql_real_connect"))
	    || !(ctx->mysql_query = (int (*)(void *, char *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "mysql_query"))
	    || !(ctx->mysql_close = (void (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "mysql_close"))
#ifndef mysql_error
	    || !(ctx->mysql_error = (char * (*)(void *)) dlsym(ctx->lib,
	    SYMBOL_PREFIX "mysql_error"))
#endif
	    ) {
		m_dprintf(MSYSLOG_SERIOUS, "om_mysql_init: Error resolving"
		    " api symbols, %s\n", dlerror());
		free(ctx);  
		return (-1);
	}

	/* parse line */
	optind = 1;
#ifdef HAVE_OPTRESET
	optreset = 1;
#endif
	while ((ch = getopt(argc, argv, "s:u:p:d:t:DPF")) != -1) {
		switch (ch) {
		case 's':
			/* get database host name and port */
			if ((p = strstr(optarg, ":")) == NULL) {
				ctx->port = MYSQL_PORT;
			} else {
				*p = '\0';
				ctx->port = atoi(++p);
			}
			ctx->host = strdup(optarg);
			break;
		case 'u':
			ctx->user = strdup(optarg);
			break;
		case 'p':
			ctx->passwd = strdup(optarg);
			break;
		case 'd':
			ctx->db = strdup(optarg);
			break;
		case 't':
			ctx->table = strdup(optarg);
			break;
		case 'D':
			ctx->flags |= OM_MYSQL_DELAYED_INSERTS;
			break;
		case 'P':
			ctx->flags |= OM_MYSQL_FACILITY;
			break;
		case 'F':
			ctx->flags |= OM_MYSQL_PRIORITY;
			break;
		default:
			goto om_mysql_init_bad;
		}
	}

	if (ctx->user == NULL || ctx->db == NULL || ctx->port == 0 ||
	    ctx->host == NULL || ctx->table == NULL)
		goto om_mysql_init_bad;

	/* connect to the database */
	if (! (ctx->h = (ctx->mysql_init)(NULL)) ) {

		snprintf(err_buf, sizeof(err_buf), "om_mysql_init: Error "
		    "initializing handle");
		logerror(err_buf);
		goto om_mysql_init_bad;
	}

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mysql_init: mysql_init returned %p\n",
	    ctx->h);

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mysql_init: params %p %s %s %s %i"
	    " \n", ctx->h, ctx->host, ctx->user, ctx->db, ctx->port);

	snprintf(statbuf, sizeof(statbuf), "om_mysql: sending "
	    "messages to %s, database %s, table %s.", ctx->host,
	    ctx->db, ctx->table);
	*status = strdup(statbuf);

	if (!((ctx->mysql_real_connect)(ctx->h, ctx->host, ctx->user,
	    ctx->passwd, ctx->db, ctx->port, NULL, 0)) ) {

		snprintf(err_buf, sizeof(err_buf), "om_mysql_init: Error "
		    "connecting to db server [%s], [%s:%i] user [%s] db [%s]",
#ifndef mysql_error
			    (ctx->mysql_error)(ctx->h),
#else
			    mysql_error(ctx->h),
#endif
		    ctx->host, ctx->port, ctx->user, ctx->db);
		logerror(err_buf);
		return (1);
	}

	return (1);

om_mysql_init_bad:
	if (ctx) {
		om_mysql_close(f, ctx);
		free(ctx);
		*c = NULL;
	}

	*status = NULL;

	return(-1);
}


int
om_mysql_close(struct filed *f, void *ctx)
{
	struct om_mysql_ctx *c;

	c = (struct om_mysql_ctx*) ctx;

	(c->mysql_close)(c->h);

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
	if (c->lib)
		dlclose(c->lib);

	return (0);
}
