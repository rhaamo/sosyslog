/*	$CoreSDI: im_unix.c,v 1.17 2000/05/26 18:31:39 fgsch Exp $	*/

/*
 *  im_unix -- classic behaviour module for BDS like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         form syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <syslog.h>
#include "modules.h"
#include "syslogd.h"


char   *cvthname __P((struct sockaddr_in *));


/*
 * get messge
 *
 */

int
im_unix_getLog(im, ret)
	struct i_module *im;
	struct im_msg  *ret;
{
	struct sockaddr_un fromunix;
	int slen;

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(fromunix);
	ret->im_len = recvfrom(im->im_fd, ret->im_msg, MAXLINE, 0,
			(struct sockaddr *)&fromunix, &slen);
	if (ret->im_len > 0) {
		ret->im_msg[ret->im_len] = '\0';
		strncpy(ret->im_host, LocalHostName, sizeof(ret->im_host));
	} else if (ret->im_len < 0 && errno != EINTR) {
		logerror("recvfrom unix");
		ret->im_msg[0] = '\0';
		ret->im_len = 0;
		ret->im_host[0] = '\0';
		return(-1);
	}

	return(1);
}

/*
 * initialize unix input
 *
 */


int
im_unix_init(I, argv, argc)
	struct i_module *I;
	char **argv;
	int argc;
{
	struct sockaddr_un sunx;

	if (I == NULL || argv == NULL || argc != 2)
	    return(-1);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	(void) unlink(argv[1]);

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void)strncpy(sunx.sun_path, argv[1], sizeof(sunx.sun_path));
	I->im_fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (I->im_fd < 0 ||
	    bind(I->im_fd, (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(argv[1], 0666) < 0) {
		(void) snprintf(I->im_buf, sizeof(I->im_buf), "cannot create %s",
		    argv[1]);
		logerror(I->im_buf);
		dprintf("cannot create %s (%d)\n", argv[1], errno);
		return (-1);
	}
	I->im_type = IM_UNIX;
	I->im_name = strdup("unix");
	I->im_path = strdup(argv[1]);
	return(1);
}



int
im_unix_close(im)
	struct i_module *im;
{
	int ret;


	if (im->im_path) {
		ret = unlink(im->im_path);
		free(im->im_path);
	}

	if (im->im_name)
		free(im->im_name);

	return(ret);
}


#if 0
/*
 * get user credentials
 */

ssize_t
getcred(fd, fcred)
	int	fd;
	struct f_cred *fcred;
{
	struct msg_hdr	msg;
	struct iovec	iov[1];
	ssize_t		n;
	union {
	    struct cmsghdr cm;
	    char   control[CMSGSPACE(sizeof(struct fcred))]
	} control_un;
	struct cmsghdr  *cmptr;

	msg.msg_control = control_un.control;
	msg.msg_controllen = sizeof(control_un.control);

	msg.msg_name = NULL;
	msg.msg_nmelen = 0;

	iov[0].io_base = NULL;
	iov[0].io_len  = 0;
	msg.msg_iov = iov;
	msg.msg_iovlen = 1;

	if ((n = recvmsg(fd, &msg, 0)) < 0)k
	
}

#endif

