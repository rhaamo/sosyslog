/*
 *  im_bsd -- classic behaviour module for BDS like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         form syslogd.c by Eric Allman and Ralph Campbell
 *    
 */


#include <syslog.h>
#include "syslogd.h"
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>

void    logerror __P((char *));
void    logmsg __P((int, char *, char *, int));



/*
 * initialize BSD input
 *
 */

int
im_bsd_init(I, argv, argc)
	struct i_module *I;
	char	**argv;
	int	argc;
{
	if ((I->im_fd = open(_PATH_KLOG, O_RDONLY, 0)) < 0) {
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
	}
	
        I->im_type = IM_BSD;
        I->im_name = "bsd";
        I->im_flags  ^= IMODULE_FLAG_KERN;
        return(I->im_fd);
}


/*
 * get messge
 *
 * Take a raw input line from /dev/klog, split and format similar to syslog().
 */

int
im_bsd_getLog(im, ret)
	struct i_module *im;
	struct im_msg  *ret;
{
	char *p, *q, *lp;
	int i, c;

	(void)strncpy(ret->im_msg, _PATH_UNIX, sizeof(ret->im_msg) - 3);
	(void)strncat(ret->im_msg, ": ", 2);
	lp = ret->im_msg + strlen(ret->im_msg);

	i = read(im->im_fd, im->im_buf, sizeof(im->im_buf) - 1);
	if (i > 0) {
		(im->im_buf)[i] = '\0';
		for (p = im->im_buf; *p != '\0'; ) {
			/* fsync file after write */
			ret->im_flags = SYNC_FILE | ADDDATE;
			ret->im_pri = DEFSPRI;
			if (*p == '<') {
				ret->im_pri = 0;
				while (isdigit(*++p))
				    ret->im_pri = 10 * ret->im_pri + (*p - '0');
				if (*p == '>')
				        ++p;
			} else {
				/* kernel printf's come out on console */
				ret->im_flags |= IGN_CONS;
			}
			if (ret->im_pri &~ (LOG_FACMASK|LOG_PRIMASK))
				ret->im_pri = DEFSPRI;
			q = lp;
			while (*p != '\0' && (c = *p++) != '\n' &&
					q < (ret->im_msg+sizeof ret->im_msg))
				*q++ = c;
			*q = '\0';
			strncat(ret->im_host, LocalHostName, sizeof(ret->im_host) - 1);
			ret->im_len = strlen(ret->im_msg);
			logmsg(ret->im_pri, ret->im_msg, ret->im_host, ret->im_flags);
		}

	} else if (i < 0 && errno != EINTR) {
		logerror("im_bsd_getLog");   
		im->im_fd = -1;
	}

	/* if ok return (2) wich means already logged */
	return(im->im_fd == -1 ? -1: 2);
}

int
im_bsd_close(im)
	struct i_module *im;
{
	return(close(im->im_fd));
}
