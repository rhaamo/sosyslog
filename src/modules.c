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

/*
 *  syslogd generic module functions --
 *
 * Author: Alejo Sanchez for Core-SDI S.A.
 */

#include <unistd.h>
#include <ctype.h>
#include <sys/syslog.h>
#include "syslogd.h"
#include "modules.h"

/* assign module functions to generic pointer */
int modules_init ()
{
	/* initialize module function assignations */
	memset(m_functions, 0, sizeof(m_functions));

	/* classic module */
	m_functions[M_CLASSIC].m_name 		= "classic";
	m_functions[M_CLASSIC].m_type 		= M_CLASSIC;
	m_functions[M_CLASSIC].m_printlog 	= m_classic_printlog;
	m_functions[M_CLASSIC].m_init 		= m_classic_init;
	m_functions[M_CLASSIC].m_close 		= m_classic_close;
	m_functions[M_CLASSIC].m_flush 		= m_classic_flush;

#if 0
	/* mysql module */
	m_functions[M_MYSQL].m_name 		= "mysql";
	m_functions[M_CLASSIC].m_type 		= M_MYSQL;
	m_functions[M_MYSQL].m_printlog 	= m_mysql_printlog;
	m_functions[M_MYSQL].m_init 		= m_mysql_init;
	m_functions[M_MYSQL].m_close 		= m_mysql_close;
	m_functions[M_MYSQL].m_flush 		= m_mysql_flush;
#endif

}


/* close all modules of a specific filed */
int modules_close(f)
	struct filed *f;
{
	/* close all modules */
}

/* create all necesary modules for a specific filed */
int modules_create(p, f, prog)
	char *p;
	struct filed *f;
	char *prog;
{
	char	name[MAX_MODULE_NAME_LEN + 1], *b;
	char	line[LINE_MAX + 1];
	struct o_module	*m;
	struct m_functions	*mf;
	int	i;

	memset(name, 0, MAX_MODULE_NAME_LEN + 1);

	if (p == NULL)
		return (-1);

	/* create context and initialize module for logging */
	switch (*p)
	{
	case '%':
		/* get this module name */
		for (i = 0; p && i < MAX_MODULE_NAME_LEN &&
				(isalpha(*p) || *p == '_'); p++, i++)
			name[i] = *p;
		if (!i)
			return(-1);

		/* concatenate a new module node */
		for (m = f->f_mod; m;);
		m = (struct o_module *) calloc(sizeof(struct o_module));

		/* get this module function */
		for (mf = m_functions; mf; mf++) {
			if (strncmp(name, mf->m_name) == 0)
				break;
		}
		/* no functions for this module where found */
		if (!mf)
			return(-1);

		m->m_type = mf->m_type;

		/* advance to module params */
		while(isspace(*p)) p++;
		(*mf->m_init)(p, f, prog, (void *) &(m->context));

		break;
	case '@':
	case '/':
	case '*':
	default:
		/* classic style */
		/* prog is already on this filed */
		m_classic_init(p, f, NULL, NULL);
		break;
	}
}

