/*      $CoreSDI: im_doors.c,v 1.12 2001/03/07 21:35:14 alejo Exp $   */

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
 * im_doors -- use syslog doors for a syslog helper
 *      
 * Author: Ari Edelkind (11/02/2000)
 *    
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <syslog.h>
#include <stropts.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <door.h>
#include <time.h>
#include "../modules.h"
#include "../syslogd.h"


void im_door_func ();

#define DEFAULT_DOOR "/etc/.syslog_door"
#define DOOR_MODE 00644                    /* Drw-r--r-- */
#define LB_SIZE 128                        /* log buffer */


/*
 * initialize doors input
 *
 */

int
im_doors_init(struct i_module *I, char **argv, int argc)
{
	char *door_path = DEFAULT_DOOR;
	int fd;

	dprintf(MSYSLOG_INFORMATIVE, "im_doors_init: Entering\n");

	if (argc < 1 || argc > 2) {
		dprintf(MSYSLOG_SERIOUS, "doors usage: -i doors[:path]\n\n");
		return (-1);
	}

	if (argc == 2)
		door_path = argv[1];

	if (unlink(door_path) == -1) {
		if (errno != ENOENT) {
			dprintf(MSYSLOG_SERIOUS, "im_doors: unlink(%s): %s\n",
			    door_path, strerror (errno));
			return (-1);
		}
		dprintf(MSYSLOG_INFORMATIVE, "%s didn't exist; it will be "
		    "created\n", door_path);
	}

	if ((fd = open (door_path, O_CREAT | O_RDWR, 00644)) == -1) {
		dprintf(MSYSLOG_SERIOUS, "im_doors: open(%s): %s\n", door_path,
		    strerror (errno));
		return (-1);
	}

	if (close(fd) == -1) {
		/* if close() fails here, there's probably an fs error */
		dprintf(MSYSLOG_SERIOUS, "im_doors: close(%s): %s\n",
		    door_path, strerror (errno));
		return (-1);
	}

	if ((fd = door_create(im_door_func, NULL, 0)) == -1) {
		dprintf(MSYSLOG_SERIOUS, "im_doors: door_create: %s\n",
		    strerror (errno));
		return (-1);
	}

	if (fattach(fd, door_path) == -1) {
		dprintf(MSYSLOG_SERIOUS, "im_doors: fattach(%s): %s\n",
		    door_path, strerror (errno));
		return (-1);
	}

        return(1);
}


/* door function */
void im_door_func(cookie, dataptr, datasize, descptr, ndesc)
	void *cookie;
	char *dataptr;
	size_t datasize;
	door_desc_t *descptr;
	size_t ndesc;
{
	struct door_cred dcred;
	char logbuf[LB_SIZE];

	if (door_cred(&dcred) == -1) {
		snprintf (logbuf, LB_SIZE, "door_cred failed: %s\n",
		    strerror (errno));
		logerror (logbuf);
	} else {

		dprintf(MSYSLOG_INFORMATIVE, "door connection from uid %lu",
		    (unsigned long)dcred.dc_euid);

		if (dcred.dc_euid != dcred.dc_ruid)
			dprintf(MSYSLOG_INFORMATIVE) (" (%lu)",
			    (unsigned long)dcred.dc_ruid);

		dprintf(MSYSLOG_INFORMATIVE) ("\n");
	}

	/* this function does absolutely nothing except return */
	door_return(NULL, 0, NULL, 0);

	/* if control reaches here, something went wrong */
	snprintf(logbuf, LB_SIZE, "door_return failed: %s\n",
	    strerror(errno));
	logerror(logbuf);
}

