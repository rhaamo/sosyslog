/* $CoreSDI: om_oracle8i.c,v 1.7 2003/01/31 19:01:14 jkohen Exp $	*/

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
 * om_oracle8i -- Oracle8i database support Module
 *
 * Author: Javier Kohen for Core-SDI SA
 *         Based on om_mysql from Alejo Sanchez
 *
 * Future enhancements:
 *         Move the creation of the query together with OCIStmtPrepare to
 *         om_oracle8i_init. Add binding code to om_oracle8i_write just
 *         before OCIStmtExecute.
 */

#warning This module need testings

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
#include <syslog.h>
#include <unistd.h>
#include <dlfcn.h>
#include "../modules.h"
#include "../syslogd.h"
#include "sql_misc.h"

/* size of  query buffer */
#define MAX_QUERY	8192

/*
 * ARGH! Redeclaring this is painful, man.
 * Declarations and definitions taken from Oracle8i 8.1.5's oci.h.
 */

typedef int (* OCIHandleAlloc_t) (const void *parenth, void **hndlpp,
				  uint32_t type, size_t xtramem_sz,
				  void **usrmempp);

typedef int (* OCIHandleFree_t) (void *hndlp, uint32_t type);

typedef int (* OCILogon_t) (void *envhp, void *errhp, void **svchp,
			    const char *username, uint32_t uname_len,
			    const char *password, uint32_t passwd_len,
			    const char *dbname, uint32_t dbname_len);

typedef int (* OCILogoff_t) (void *svchp, void *errhp);

typedef int (* OCIEnvCreate_t) (void **envhpp, uint32_t mode,
				const void *ctxp,
				const void *(*malocfp) (void *ctxp,
							size_t size),
				const void *(*ralocfp) (void *ctxp,
							void *memptr,
							size_t newsize),
				const void (*mfreefp) (void *ctxp,
						       void *memptr),
				size_t xtramemsz, void **usrmempp);

typedef int (* OCIStmtPrepare_t) (void *stmtp, void *errhp,
				  const char *stmt, uint32_t stmt_len,
				  uint32_t language, uint32_t mode);

typedef int (* OCIStmtExecute_t) (void *svchp, void *stmtp, void *errhp,
				  uint32_t iters, uint32_t rowoff,
				  const void *snap_in, void *snap_out,
				  uint32_t mode);

typedef int (* OCIErrorGet_t) (void *hndlp, uint32_t recordno,
			       void *sqlstate, int32_t *errcodep,
			       char *bufp, uint32_t bufsiz, uint32_t type);
typedef int (* OCIBindByName_t) (void *stmtp, void **bindpp, void *errhp,
				 const char *placeholder, uint32_t placeh_len,
				 void *valuep, uint32_t value_sz,
				 uint16_t dty, void *indp, uint16_t *alenp,
				 uint16_t *rcodep, uint32_t maxarr_len,
				 uint32_t *curelep, uint32_t mode);

#define OCI_SUCCESS 0
#define OCI_ERROR -1
#define OCI_DEFAULT 0x00
#define OCI_COMMIT_ON_SUCCESS 0x20

#define OCI_HTYPE_ENV 1
#define OCI_HTYPE_ERROR 2
#define OCI_HTYPE_STMT 4

#define OCI_NTV_SYNTAX 1

#define SQLT_STR 5

struct om_oracle8i_ctx {
	int             lost;
	char           *table;
	char           *user;
	char           *passwd;
	char           *db;
	void           *lib;
	int             flags;
	int             loggedon;

	void           *env;
	void           *err;
	void           *svc;
	void           *stmt;

	OCIEnvCreate_t  OCIEnvCreate;
	OCIHandleAlloc_t OCIHandleAlloc;
	OCIHandleFree_t OCIHandleFree;
	OCILogon_t      OCILogon;
	OCILogoff_t     OCILogoff;
	OCIStmtPrepare_t OCIStmtPrepare;
	OCIStmtExecute_t OCIStmtExecute;
	OCIErrorGet_t   OCIErrorGet;
	OCIBindByName_t OCIBindByName;
};

#define	OM_ORACLE8I_FACILITY		0x4
#define	OM_ORACLE8I_PRIORITY		0x8

static
int             parse_arguments(struct om_oracle8i_ctx *ctx,
				int argc, char **argv);
static
int             load_symbols(struct om_oracle8i_ctx *ctx);
static
int             prepare_query(struct om_oracle8i_ctx *ctx);

int             om_oracle8i_close(struct filed *, void *);

int
om_oracle8i_write(struct filed *f, int flags, struct m_msg *m, void *ctx)
{
	struct om_oracle8i_ctx *c;
	/* These should be preserved between calls, so they can be reused. */
	static void    *bnd_prio = NULL,
		       *bnd_fac = NULL,
		       *bnd_tstamp = NULL,
		       *bnd_host = NULL,
		       *bnd_msg = NULL;
	char            err_buf[512];
	char            facility[16],
	                priority[16],
		        tstamp[100],
	               *p;
	int             err;

	m_dprintf(MSYSLOG_INFORMATIVE, "om_oracle8i_write: entering [%s] [%s]\n",
		m->msg, f->f_prevline);

	c = (struct om_oracle8i_ctx *)ctx;

	if (!c->loggedon) {
		err = c->OCILogon(c->env, c->err, &c->svc,
				  c->user, strlen(c->user),
				  c->passwd, strlen(c->passwd),
				  c->db, strlen(c->db));
		if (OCI_SUCCESS != err) {
			return -1;
		} else {
			c->loggedon = 1;
		}
	}


	/* Bind the placeholders in the INSERT statement.
	 */

	if ((c->flags & OM_ORACLE8I_PRIORITY
	     && (err = c->OCIBindByName(c->stmt, &bnd_prio, c->err, ":PRIORITY",
					-1, priority, sizeof priority, SQLT_STR,
					NULL, NULL, NULL, 0, NULL, OCI_DEFAULT)))
	    || (c->flags & OM_ORACLE8I_FACILITY
		&& (err = c->OCIBindByName(c->stmt, &bnd_fac, c->err, ":FACILITY",
					   -1, facility, sizeof facility, SQLT_STR,
					   NULL, NULL, NULL, 0, NULL, OCI_DEFAULT)))
	    || (err = c->OCIBindByName(c->stmt, &bnd_tstamp, c->err, ":TIMESTAMP",
				       -1, tstamp, sizeof tstamp, SQLT_STR,
				       NULL, NULL, NULL, 0, NULL, OCI_DEFAULT))
	    || (err = c->OCIBindByName(c->stmt, &bnd_host, c->err, ":HOST", -1,
				       f->f_prevhost, strlen(f->f_prevhost)+1, SQLT_STR,
				       NULL, NULL, NULL, 0, NULL, OCI_DEFAULT))
	    || (err = c->OCIBindByName(c->stmt, &bnd_msg, c->err, ":MESSAGE",
				       -1, m->msg, strlen(m->msg)+1, SQLT_STR,
				       NULL, NULL, NULL, 0, NULL, OCI_DEFAULT))) {
		c->OCIErrorGet(c->err, 1, NULL, &err,
			       err_buf, sizeof err_buf, OCI_HTYPE_ERROR);
		m_dprintf(MSYSLOG_SERIOUS, "om_oracle8i_write: Error "
			 "binding the placeholders for [%s]\n", err_buf);
		return -1;
	}


	/* Assign the proper values to the bound variables.
	 */

	if (c->flags & OM_ORACLE8I_FACILITY) {
		if ((p = decode_val(m->fac << 3, CODE_FACILITY)) != NULL)
			snprintf(facility, sizeof (facility), "'%s',", p);
		else
			snprintf(facility, sizeof (facility), "'%d',",
				 m->fac << 3);
	}

	if (c->flags & OM_ORACLE8I_PRIORITY) {
		if ((p = decode_val(m->pri, CODE_PRIORITY)) != NULL)
			snprintf(priority, sizeof (priority), "'%s',", p);
		else
			snprintf(priority, sizeof (priority), "'%d',",
				 m->pri);
	}

	snprintf(tstamp, sizeof tstamp, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d",
		 f->f_tm.tm_year + 1900, f->f_tm.tm_mon + 1,
		 f->f_tm.tm_mday, f->f_tm.tm_hour, f->f_tm.tm_min,
		 f->f_tm.tm_sec);
	tstamp[sizeof (tstamp) - 1] = '\0';


	/* Execute the INSERT statement.
	 */

	err = c->OCIStmtExecute(c->svc, c->stmt, c->err, 1, 0, NULL, NULL,
				OCI_COMMIT_ON_SUCCESS);
	if (OCI_SUCCESS != err) {
		c->OCIErrorGet(c->err, 1, NULL, &err,
			       err_buf, sizeof err_buf, OCI_HTYPE_ERROR);
		m_dprintf(MSYSLOG_SERIOUS, "om_oracle8i_write: Error "
			 "executing statement [%s]\n", err_buf);

		return -1;
	}

	return 1;
}

/*
 *  INIT -- Initialize om_oracle8i
 *
 *  Parse options and connect to database
 *
 *  params:
 *         -u <user>
 *         -p <password>
 *         -b <database_name>
 *         -t <table_name>
 *         -P
 *         -F
 *
 */

int
om_oracle8i_init(int argc, char **argv, struct filed *f, char *prog, void **c,
	      char **status)
{
	struct om_oracle8i_ctx *ctx;
	char            buf[1024];
	char            err_buf[512];
	int             err;

	if (argv == NULL || *argv == NULL || argc < 2 || f == NULL ||
	    c == NULL || *c != NULL)
		return (-1);

	m_dprintf(MSYSLOG_INFORMATIVE, "om_oracle8i_init: alloc context\n");

	if ((*c = (void *)calloc(1, sizeof (struct om_oracle8i_ctx))) == NULL)
		return (-1);
	ctx = (struct om_oracle8i_ctx *)*c;

	if (1 != load_symbols(ctx)) {
		free(ctx);
		return -1;
	}

	if (1 != parse_arguments(ctx, argc, argv)) {
		goto om_oracle8i_init_bad;
	}

	err = ctx->OCIEnvCreate(&ctx->env, OCI_DEFAULT, NULL, NULL, NULL,
				NULL, 0, NULL);
	if (OCI_SUCCESS != err) {
		ctx->OCIErrorGet(ctx->env, 1, NULL, &err,
				 err_buf, sizeof err_buf, OCI_HTYPE_ENV);
		snprintf(buf, sizeof buf, "om_oracle8i_init: Error "
			 "initializing handle [%s]", err_buf);
		logerror(buf);
		goto om_oracle8i_init_bad;
	}

	ctx->OCIHandleAlloc(ctx->env, &ctx->err, OCI_HTYPE_ERROR, 0, NULL);

	m_dprintf(MSYSLOG_INFORMATIVE, "om_oracle8i_init: params %p %s %s \n",
		ctx->env, ctx->user, ctx->db);

	if (1 != prepare_query(ctx)) {
		goto om_oracle8i_init_bad;
	}

	snprintf(buf, sizeof (buf), "om_oracle8i: sending "
		 "messages to database %s, table %s.",
		 ctx->db, ctx->table);
	*status = strdup(buf);

	/* Connect to the database.
	 */

	err = ctx->OCILogon(ctx->env, ctx->err, &ctx->svc,
			    ctx->user, strlen(ctx->user),
			    ctx->passwd, strlen(ctx->passwd),
			    ctx->db, strlen(ctx->db));
	if (OCI_SUCCESS != err) {
		ctx->OCIErrorGet(ctx->err, 1, NULL, &err,
				 err_buf, sizeof err_buf, OCI_HTYPE_ERROR);
		snprintf(buf, sizeof buf, "om_oracle8i_init: Error "
			 "connecting to db server [%s] user [%s] db [%s]",
			 err_buf, ctx->user, ctx->db);
		logerror(buf);
		/* Another Logon operation will be tried when needed. */
	} else {
		ctx->loggedon = 1;
	}

	return (1);

 om_oracle8i_init_bad:
	if (ctx) {
		om_oracle8i_close(f, ctx);
		free(ctx);
		*c = NULL;
	}

	*status = NULL;

	return (-1);
}

int
om_oracle8i_close(struct filed *f, void *ctx)
{
	struct om_oracle8i_ctx *c = (struct om_oracle8i_ctx *) ctx;

	if (c->svc)
		c->OCILogoff(c->svc, c->err);
	if (c->err)
		c->OCIHandleFree(c->err, OCI_HTYPE_ERROR);
	if (c->env)
		c->OCIHandleFree(c->env, OCI_HTYPE_ENV);
	if (c->stmt)
		c->OCIHandleFree(c->stmt, OCI_HTYPE_STMT);

	if (c->table)
		free(c->table);
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

int
load_symbols(struct om_oracle8i_ctx *ctx)
{
	if ((ctx->lib = dlopen("libclntsh.so", DLOPEN_FLAGS)) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "om_oracle8i_init: Error loading"
			" api library, %s\n", dlerror());
		return -1;
	}

	ctx->OCIEnvCreate = (OCIEnvCreate_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCIEnvCreate");
	ctx->OCIHandleAlloc = (OCIHandleAlloc_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCIHandleAlloc");
	ctx->OCIHandleFree = (OCIHandleFree_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCIHandleFree");
	ctx->OCILogon = (OCILogon_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCILogon");
	ctx->OCILogoff = (OCILogoff_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCILogoff");
	ctx->OCIStmtPrepare = (OCIStmtPrepare_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCIStmtPrepare");
	ctx->OCIStmtExecute = (OCIStmtExecute_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCIStmtExecute");
	ctx->OCIErrorGet = (OCIErrorGet_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCIErrorGet");
	ctx->OCIBindByName = (OCIBindByName_t)
		dlsym(ctx->lib, SYMBOL_PREFIX "OCIBindByName");

	/* Every function is required. */
	if (!ctx->OCIEnvCreate || !ctx->OCIHandleAlloc
	    || !ctx->OCIHandleFree || !ctx->OCILogon
	    || !ctx->OCILogoff || !ctx->OCIStmtPrepare
	    || !ctx->OCIStmtExecute || !ctx->OCIErrorGet
	    || !ctx->OCIBindByName) {
		m_dprintf(MSYSLOG_SERIOUS,
			"om_oracle8i_init: Some required symbols are missing");
		return -1;
	}

	return 1;
}

int
parse_arguments(struct om_oracle8i_ctx *ctx, int argc, char **argv)
{
	int             argcnt;
	int             ch;

	argcnt = 1;		/* skip module name */

	while ((ch = getxopt(argc, argv, "u!user: p!password:"
			     " d!database: t!table: P!priority F!facility",
			     &argcnt))
	       != -1) {

		switch (ch) {
		case 'u':
			ctx->user = strdup(argv[argcnt]);
			break;
		case 'p':
			ctx->passwd = strdup(argv[argcnt]);
			break;
		case 'd':
			ctx->db = strdup(argv[argcnt]);
			break;
		case 't':
			ctx->table = strdup(argv[argcnt]);
			break;
		case 'P':
			ctx->flags |= OM_ORACLE8I_PRIORITY;
			break;
		case 'F':
			ctx->flags |= OM_ORACLE8I_FACILITY;
			break;
		default:
			return -1;
		}
		argcnt++;
	}

	if (ctx->user == NULL || ctx->db == NULL || ctx->table == NULL)
		return -1;

	return 1;
}

int
prepare_query(struct om_oracle8i_ctx *ctx)
{
	char            query[MAX_QUERY];
	char            err_buf[512];
	int             err;
	int             i;

	i = snprintf(query, sizeof (query),
		     "INSERT INTO %s"
		     " (%s%s TIMESTAMP, HOST, MESSAGE)"
		     " VALUES(%s%s TO_DATE(:timestamp, 'YYYY-MM-DD HH24:MI:SS'),"
		     " :host, :message)", ctx->table,
		     (ctx->flags & OM_ORACLE8I_FACILITY)? "FACILITY, " : "",
		     (ctx->flags & OM_ORACLE8I_PRIORITY)? "PRIORITY, " : "",
		     (ctx->flags & OM_ORACLE8I_FACILITY)? ":facility, " : "",
		     (ctx->flags & OM_ORACLE8I_PRIORITY)? ":priority, " : "");

	m_dprintf(MSYSLOG_INFORMATIVE2, "om_oracle8i_write: query [%s]\n", query);

	ctx->OCIHandleAlloc(ctx->env, &ctx->stmt, OCI_HTYPE_STMT, 0, NULL);

	err = ctx->OCIStmtPrepare(ctx->stmt, ctx->err, query, i,
				OCI_NTV_SYNTAX, OCI_DEFAULT);
	if (OCI_SUCCESS != err) {
		ctx->OCIErrorGet(ctx->err, 1, NULL, &err,
			       err_buf, sizeof err_buf, OCI_HTYPE_ERROR);
		m_dprintf(MSYSLOG_SERIOUS, "om_oracle8i_write: Error "
			 "preparing statement [%s]\n", err_buf);

		return -1;
	}

	return 1;
}
