/*      $CoreSDI: im_doors.c,v 1.8 2001/02/08 18:01:53 alejo Exp $   */

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
 * im_doors -- use syslog doors for a syslog helper
 *      
 * Author: Ari Edelkind (11/02/2000)
 *    
 */

#include "../../config.h"

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
 * get message
 *
 */

int
im_doors_read(struct i_module *im, int infd, struct im_msg *ret)
{
	return(0);
}

/*
 * initialize doors input
 *
 */

int
im_doors_init(struct i_module *I, char **argv, int argc)
{
	char *door_path = DEFAULT_DOOR;
	int fd;

	dprintf(DPRINTF_INFORMATIVE)("im_doors_init: Entering\n");

	if (argc < 1 || argc > 2) {
		dprintf(DPRINTF_SERIOUS)("doors usage: -i doors[:path]\n\n");
		return (-1);
	}

	if (argc == 2) {
		door_path = argv[1];
	}

	if (unlink (door_path) == -1) {
		if (errno != ENOENT) {
			dprintf(DPRINTF_SERIOUS)("im_doors: unlink(%s): %s\n",
			    door_path, strerror (errno));
			return (-1);
		}
		dprintf(DPRINTF_INFORMATIVE)("%s didn't exist; it will be "
		    "created\n", door_path);
	}

	if ((fd = open (door_path, O_CREAT | O_RDWR, 00644)) == -1) {
		dprintf(DPRINTF_SERIOUS)("im_doors: open(%s): %s\n", door_path,
				strerror (errno));
		return (-1);
	}
	if (close(fd) == -1) {
		/* if close() fails here, there's probably an fs error */
		dprintf(DPRINTF_SERIOUS)("im_doors: close(%s): %s\n",
		    door_path, strerror (errno));
		return (-1);
	}

	if ((fd = door_create (im_door_func, NULL, 0)) == -1) {
		dprintf(DPRINTF_SERIOUS)("im_doors: door_create: %s\n",
		    strerror (errno));
		return (-1);
	}

	if (fattach (fd, door_path) == -1) {
		dprintf(DPRINTF_SERIOUS)("im_doors: fattach(%s): %s\n",
		    door_path, strerror (errno));
		return (-1);
	}

        return(1);
}


/*
 * the following function is not mandatory, you can omit it
 */
int
im_doors_close (struct i_module *im) 
{
        return(1);
}


/* door function */
void im_door_func (cookie, dataptr, datasize, descptr, ndesc)
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

		dprintf(DPRINTF_INFORMATIVE)("door connection from uid %lu",
		    (unsigned long)dcred.dc_euid);

		if (dcred.dc_euid != dcred.dc_ruid)
			dprintf(DPRINTF_INFORMATIVE) (" (%lu)",
			    (unsigned long)dcred.dc_ruid);

		dprintf(DPRINTF_INFORMATIVE) ("\n");
	}

	/* this function does absolutely nothing except return */
	door_return (NULL, 0, NULL, 0);

	/* if control reaches here, something went wrong */
	snprintf (logbuf, LB_SIZE, "door_return failed: %s\n",
			strerror(errno));
	logerror (logbuf);
}
