/*      $Id: im_unix.c,v 1.9 2000/05/03 18:30:12 alejo Exp $
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
#include <signal.h>

#include <syslog.h>
#include "modules.h"
#include "syslogd.h"


char   *cvthname __P((struct sockaddr_in *));
void    logerror __P((char *));


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
	int len;
	char line[MAXLINE + 1];

	len = sizeof(fromunix);
	len = recvfrom(im->fd, line, MAXLINE, 0,
	    (struct sockaddr *)&fromunix, &len);
	if (len > 0) {
		line[len] = '\0';
		ret->msg = strdup(line);
		ret->len = strlen(line);
		ret->host = strdup(LocalHostName);
		ret->pid = -1;
		ret->pri = -1;
		ret->flags = 0;
	} else if (len < 0 && errno != EINTR) {
		logerror("recvfrom unix");
		ret->msg = NULL;
		ret->len = 0;
		ret->host = NULL;
		ret->pid = -1;
		ret->pri = -1;
		ret->flags = 0;
		return(-1);
	}

	return(1);

}

/*
 * initialize unix input
 *
 */


int
im_unix_init(I, arg)
	struct i_module *I;
	char *arg;
{
	int i;
	char line[MAXLINE + 1];
	struct sockaddr_un sunx;

	if (I == NULL)
	    return(-1);

#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
	(void) unlink(I->fd);

	memset(&sunx, 0, sizeof(sunx));
	sunx.sun_family = AF_UNIX;
	(void)strncpy(sunx.sun_path, arg, sizeof(sunx.sun_path));
	I->fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (I->fd < 0 ||
	    bind(I->fd, (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
	    chmod(I->fd, 0666) < 0) {
		(void) snprintf(line, sizeof line, "cannot create %s",
		    I->fd);
		logerror(line);
		dprintf("cannot create %s (%d)\n", I->fd, errno);
		return (-1);
	}
	return(1);
}



int
im_unix_close(im)
	struct i_module *im;
{
	return(close(im->fd));
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

