/*	$CoreSDI: om_db2.c,v 1.62 2001/02/26 22:34:38 alejo Exp $	*/

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
 * om_db2 -- MySQL database support Module
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 */

#include "../../config.h"

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
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include <ctype.h>
#include "../../config.h"
#include "../modules.h"
#include "../syslogd.h"
#include "sql_misc.h"

/* size of  query buffer */
#define MAX_QUERY	16384
/* how many seconds to wait to give again the error message */
#define MSYSLOG_MYSQL_ERROR_DELAY	30

struct om_db2_ctx {
	void	*h;
	int	lost;
	char	*table;
	char	*host;
	int	port;
	char	*user;
	char	*passwd;
	char	*db;
	void	*lib;
	int	(*mysql_ping)(void *);
	void *	(*mysql_init)(void *);
	void *	(*mysql_real_connect)(void *, char *, char *, char *,
	    char *, int, void *, int);
	int	(*mysql_query)(void *, char *);
};

int om_db2_close(struct filed *, void *);

/*
 * Define our prototypes for MySQL functions
 */

#define MYSQL_PORT 3306


int
om_db2_write(struct filed *f, int flags, char *msg, void *ctx)
{
	struct om_db2_ctx *c;
	char	query[MAX_QUERY], err_buf[100], *p;
	int i, fieldcount;
	RETSIGTYPE (*sigsave)(int);

	dprintf(DPRINTF_INFORMATIVE)("om_db2_write: entering [%s]\n", msg);

	c = (struct om_db2_ctx *) ctx;

	/* ignore sigpipes   for mysql_ping */
	sigsave = place_signal(SIGPIPE, SIG_IGN);

	if ( ((c->mysql_ping)(c->h)) != 0 && (((c->mysql_init)(c->h) == NULL)
	    || ((c->mysql_real_connect)(c->h, c->host, c->user, c->passwd, c->db,
	    c->port, NULL, 0)) == NULL) ) {

		/* restore previous SIGPIPE handler */
		place_signal(SIGPIPE, sigsave);
		c->lost++;
		if (c->lost == 1) {
			dprintf(DPRINTF_SERIOUS)("om_db2_write: Lost "
			    "connection!");
			logerror("om_db2_write: Lost connection!");
		}
		return (1);
	}

	/* restore previous SIGPIPE handler */
	place_signal(SIGPIPE, sigsave);


if (isdigit((int) *msg)) {
	/* table, yyyy-mm-dd, hh:mm:ss, host, msg  */ 
	i = snprintf(query, sizeof(query), "INSERT INTO %s"
	    " VALUES('", c->table);

	if (c->lost) {
		int pos = i;

		/*
		 * Report lost messages, but 2 of them are lost of
		 * connection and this one (wich we are going
		 * to log anyway)
		 */
		snprintf(err_buf, sizeof(err_buf), "om_db2_write: %i "
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

		dprintf(DPRINTF_INFORMATIVE2)("om_db2_write: query [%s]\n",
		    query);

		if ((c->mysql_query)(c->h, query) < 0)
			return (-1);
	}

/*
  date,
  time,
  text, text,
  INT,
  text, text, text, text, text, text, text, text, text, text, text, text, text, text, text, text, text, text,
  INT, INT, INT, INT, INT, INT, INT, INT,
  text, text, text, text, text, text, text
2001-01-15;09:43:36;00001203:10008F4BE695;Cjp47292.AUDITORIA.BOSTON;58;Active connection;;0;;;;;;;BOSTON.BANCO-BOSTON_AR;;;;;;;;;;;;;
;;;;;;;;;;;;;;;;;;;;;;;;;;;
123456789012345678901234567890
*/
	for (p = msg, fieldcount = 1; *p && i < (sizeof(query) - 10);p++) {
		if (*p == ';') {
			query[i++] = '\'';
			query[i++] = ',';
			query[i++] = '\'';
			fieldcount++;
			continue; /* check zeros and stuff */
		}
		query[i++] = *p;
	}
	
#if 0
	/* put message escaping special SQL characters */
	i += to_sql(query + i, msg, sizeof(query) - i);
#endif

	if (fieldcount < 38)
		for (;fieldcount < 38 && i < (sizeof(query) - 10);) {
			query[i++] = '\'';
			query[i++] = ',';
			query[i++] = '\'';
			fieldcount++;
		}

	/* finish it with "')" */
	query[i++] =  '\'';
	query[i++] =  ')';
	if (i < sizeof(query))
		query[i]   =  '\0';
	else
		query[sizeof(query) - 1] = '\0';

	dprintf(DPRINTF_INFORMATIVE2)("om_db2_write: query [%s]\n", query);

	i = ((c->mysql_query)(c->h, query));
	dprintf(DPRINTF_INFORMATIVE)("om_db2_write: resultado %d\n", i);
	return (i < 0 ? -1 : 1);
} else {
	/* table, yyyy-mm-dd, hh:mm:ss, host, msg  */ 
	i = snprintf(query, sizeof(query), "INSERT INTO %s"
	    " VALUES('%.4d-%.2d-%.2d', '%.2d:%.2d:%.2d','','','',"
	    "'%s','','','%s','','','','','','','','','','','','','','','',"
	    "'','','','','','','','','','','','','','')", c->table,
	    f->f_tm.tm_year + 1900, f->f_tm.tm_mon + 1, f->f_tm.tm_mday,
	    f->f_tm.tm_hour, f->f_tm.tm_min, f->f_tm.tm_sec, msg, f->f_prevhost);

        dprintf(DPRINTF_INFORMATIVE2)("om_mysql_write: query [%s]\n",
            query);

        return ((c->mysql_query)(c->h, query) < 0 ? -1 : 1);
} /* isdigit  *msg */

/* NOT REACHED */
return(1);

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
om_db2_init(int argc, char **argv, struct filed *f, char *prog, void **c,
    char **status)
{
	struct om_db2_ctx	*ctx;
	char	*p, err_buf[256], statbuf[1024];
	int	ch;

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
	    c == NULL || *c != NULL)
		return (-1);

	dprintf(DPRINTF_INFORMATIVE)("om_db2_init: alloc context\n");
	/* alloc context */
	if ((*c = (void *) calloc(1, sizeof(struct om_db2_ctx))) == NULL)
		return (-1);
	ctx = (struct om_db2_ctx *) *c;

	if ((ctx->lib = dlopen("libmysqlclient.so", DLOPEN_FLAGS)) == NULL) {
		dprintf(DPRINTF_SERIOUS)("om_db2_init: Error loading"
		    " api library, %s\n", dlerror());
		free(ctx);  
		return (-1);
	}

	if ( !(ctx->mysql_ping = dlsym(ctx->lib, SYMBOL_PREFIX "mysql_ping"))
	    || !(ctx->mysql_init = dlsym(ctx->lib, SYMBOL_PREFIX "mysql_init"))
	    || !(ctx->mysql_real_connect = dlsym(ctx->lib, SYMBOL_PREFIX
	    "mysql_real_connect"))
	    || !(ctx->mysql_query = dlsym(ctx->lib, SYMBOL_PREFIX
	    "mysql_query"))) {
		dprintf(DPRINTF_SERIOUS)("om_db2_init: Error resolving"
		    " api symbols, %s\n", dlerror());
		free(ctx);  
		return (-1);
	}

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
		default:
			goto om_db2_init_bad;
		}
	}

	if (ctx->user == NULL || ctx->db == NULL || ctx->port == 0 ||
	    ctx->host == NULL || ctx->table == NULL)
		goto om_db2_init_bad;

	/* connect to the database */
	if (! (ctx->h = (ctx->mysql_init)(NULL)) ) {

		snprintf(err_buf, sizeof(err_buf), "om_db2_init: Error "
		    "initializing handle");
		logerror(err_buf);
		goto om_db2_init_bad;
	}

	dprintf(DPRINTF_INFORMATIVE)("om_db2_init: mysql_init returned %p\n",
	    ctx->h);

	dprintf(DPRINTF_INFORMATIVE)("om_db2_init: params %p %s %s %s %s %i \n",
	    ctx->h, ctx->host, ctx->user, "<passwd>", ctx->db, ctx->port);

	snprintf(statbuf, sizeof(statbuf), "om_db2: forwarding to host %s, "
	    "database %s, table %s", ctx->host, ctx->db, ctx->table);
	*status = strdup(statbuf);

	if (!((ctx->mysql_real_connect)(ctx->h, ctx->host, ctx->user, ctx->passwd,
	    ctx->db, ctx->port, NULL, 0)) ) {

		snprintf(err_buf, sizeof(err_buf), "om_db2_init: Error "
		    "connecting to db server [%s:%i] user [%s] db [%s]",
		    ctx->host, ctx->port, ctx->user, ctx->db);
		logerror(err_buf);
		return (1);
	}

	return (1);

om_db2_init_bad:
	if (ctx) {
		om_db2_close(f, ctx);
		free(ctx);
		*c = NULL;
	}

	return(-1);

}


int
om_db2_close(struct filed *f, void *ctx)
{
	struct om_db2_ctx *c;

	c = (struct om_db2_ctx*) ctx;

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
	if (c->lib)
		dlclose(c->lib);

	return (0);
}