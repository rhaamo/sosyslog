/*	$CoreSDI: om_mymodule.c,v 1.15 2002/03/01 07:31:02 alejo Exp $	*/

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
 *  om_mymodule -- some explanation for this baby
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *
 *
 */

/* Get system information */
#include "config.h"

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
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "syslogd.h"
#include "modules.h"

int
om_mymodule_write(struct filed *f, int flags, char *msg,
                   void *context) {
/*
 * struct filed *f;    		Current filed struct
 * int flags;          		Flags for this message
 * char *msg;          		The message string
 * void *context; 	Our context
 */

	/* always check, just in case ;) */
	if (msg == NULL || !strcmp(msg, "")) {
		logerror("om_mymodule_write: no message!");
		return (-1);
	}
  /* increment the fired rule counter if this truely an output modules */
  msg->fired++;

	/*  here you must do your loggin
	    take care with repeats and if message was repeated
	    increase f->f_prevcount, else set f->f_prevcount to 0.
	 */

	/* return:
			 1  successfull
			 0  stop logging it (used by filters)
			-1  error logging (but let other modules process it)
	 */

	return (1);
}


/*
 *  INIT -- Initialize om_mymodule
 *
 */
int
om_mymodule_init (
  int argc,              /* Argumemt count */
  char **argv,           /* Argumemt array, like main() */
  struct filed *f,       /* Our filed structure */
  struct global *global, /* Global information */
	void **context)        /* Our context */
{
	char	*myArg;
	int	argcnt;

	/* for debugging purposes */
	m_dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_init: Entering\n");

	/*
	 * Parse your options with getxopt()
	 *
	 * we give an example for a -s argument
	 *
	 *
	 */

	argcnt = 1; /* skip module name */

	while ((ch = getxopt(argc, argv, "s!myargname:", &argcnt)) != -1) {
		switch (ch) { 
			case 's':
				myArg = argv[argcnt];
				break;
			default:
				m_dprintf(MSYSLOG_INFORMATIVE, "om_mymodule: "
				    "error on arguments\n");
				return (-1);
		}
		argcnt++;
	}


	/* open files, connect  to database, initialize algorithms,
	   etc. Save them in your context if necesary.
 	 */   

	/* return:
			 1  OK
			-1  something went wrong
	*/

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_init: Leaving ok\n");
	return (1);
}


/*
 * xx_close and xx_flush functions are not mandatory, you can omit them
 */
int
om_mymodule_close (struct filed *f, void *ctx) {

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_close: Entering\n");

	/* flush any buffered data and close this output */

	/* return:
			 1  OK
			-1  BAD
	 */

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_close: Leaving ok\n");

	return (ret);
}

int
om_mymodule_flush (struct filed *f, void *context) {

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_flush: Entering\n");
	/* flush any pending output */

	/* return:
			 1  OK
			-1  BAD
	 */

	m_dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_flush: Leaving ok\n");

	return (1);
}
