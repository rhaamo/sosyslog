/*	$CoreSDI: modules.h,v 1.41 2001/02/19 23:42:01 alejo Exp $	*/

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

#ifndef SYSLOG_MODULES_H
#define SYSLOG_MODULES_H

/* this MUST be the same value as syslogd.h */
#define MAXLINE 1024

#define MAX_MODULE_NAME_LEN 255


/*
 * This structure represents main details for the output modules
 */

struct o_module {
	struct	o_module *om_next;
	struct	omodule *om_func; /* where are this puppy's functions? */
	struct  om_hdr_ctx	*ctx;
};

/*
 * This structure represents main details for the input modules
 */

struct i_module {
	struct	 i_module *im_next;
	struct	 imodule *im_func; /* where are this puppy's functions? */
	int	 im_fd;	/*  for use with select() */
	int	 im_flags;  /* 1 to 8 are reserved */
#define IMODULE_FLAG_KERN	0x01
#define IMODULE_FLAG_CONN	0x02
	char	*im_path;
	char	 im_buf[MAXLINE + 1];
	void	*im_ctx;
};

int	add_fd_input(int , struct i_module *); /* add this fd to array */
void	remove_fd_input(int); /* remove this fd from poll arrays */

/*
 * This structure represents the return of the input modules
 */

struct im_msg {
	int	im_pid;
	int	im_pri;
	int	im_flags;
#define  SYSLOG_IM_PID_CHECKED	0x01
#define  SYSLOG_IM_HOST_CHECKED	0x02
	char	*im_msg;
	size_t	im_mlen; /* size of im_msg buffer */
	size_t	im_len; /* size of contents of im_msg buffer */
	char	im_host[SIZEOF_MAXHOSTNAMELEN + 1];
};

#endif
