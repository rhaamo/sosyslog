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
static char rcsid[] = "$NetBSD: syslogd.c,v 1.5 1996/01/02 17:48:41 perry Exp $";
#endif /* not lint */

/*
 *  m_mysql -- MySQL database syupport Module
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

#include "syslogd.h"
#include "modules.h"

#include "/usr/local/include/mysql/mysql.h"

int
m_mysql_printlog(f, flags, msg, context)
	struct filed *f;
	int flags;
	char *msg;
	void *context;
{
	return (-1);
}

/*
 *  INIT -- Initialize m_mysql
 *
 *  Parse options and connect to database
 *
 *  params:
 *         -s <host>:<port>
 *         -u <user>
 *         -p <password>
 *         -c			create table
 *         -d <date_column>
 *         -p <program_name_column>
 *         -h <host_name>
 *         -m <message_column>
 *
 */

int
m_mysql_init(line, f, prog, context)
	char *line;
	struct filed *f;
	char *prog;
	void *context;
{
	/* parse line */
	return (0);
	return (0);
}

int
m_mysql_close(f, context)
	struct filed *f;
	void *context;
{
	return (-1);
}

int
m_mysql_flush(f, context)
	struct filed *f;
	void *context;
{
	return (-1);
}

