/*	$CoreSDI: im_mymodule.c,v 1.10 2001/03/07 21:35:13 alejo Exp $	*/

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
 * im_mymodule -- Give some description
 *      
 * Author: Alejo Sanchez for Core SDI S.A.
 *    
 */

/* Get system information */
#include "config.h"

#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#include "modules.h"
#include "syslogd.h"

/*
 * get message
 *
 */

int
im_mymodule_read(struct i_module *im, int index, struct im_msg *ret)
{

	dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_read: Entering\n");

	/* read from input */

	dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_read: Leaving\n");

	return (1);
}

/*
 * initialize mymodule input
 *
 */

int
im_mymodule_init (struct i_module *I, char **argv, int argc)
{
	dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_init: Entering\n");

	/* initialize */

	dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_init: Leaving\n");

	add_fd_input(I->im_fd , I, 0);

        return (1);
}


/*
 * the following function is not mandatory, you can omit it
 */
int
im_mymodule_close (struct i_module *im) 
{
	dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_close: Entering\n");

	/* close */

	dprintf(MSYSLOG_INFORMATIVE, "om_mymodule_close: Leaving\n");

        return (1);
}
