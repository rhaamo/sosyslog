/*	$CoreSDI$	*/

/*
 *  im_linux -- input module to log linux kernel messages
 *      
 * Author: Claudio Castiglia, Core-SDI SA
 *    
 */

#include "config.h"

#ifdef HAVE_PATHS_H
#	include <paths.h>
#else
#	define _PATH_KLOG	"/proc/kmsg"
#endif


#include <sys/types.h>
#include <sys/uio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include "syslogd.h"


/*
 * initialize linux kernel input
 */

int
im_linux_init(I, argv, argc)
	struct i_module *I;
	char	**argv;
	int	argc;
{
	if ((I->im_fd = open(_PATH_KLOG, O_RDONLY, 0)) >= 0)
        	I->im_path = strdup(_PATH_KLOG);
	else if (errno == ENOENT) {
		I->im_path = NULL;
		I->im_fd = 0;
	} else 
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
	
        I->im_type = IM_LINUX;
        I->im_name = strdup("linux");
        I->im_flags |= IMODULE_FLAG_KERN;
        return(I->im_fd);
}


/*
 * get kernel messge
 * Take input line from /proc/kmsg or syslog(2), split and format similar to syslog().
 */

int
im_linux_getLog(im, ret)
	struct i_module *im;
	struct im_msg  *ret;
{
/*
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

*/
	/* if ok return (2) wich means already logged */
/*
	return(im->im_fd == -1 ? -1: 2);
*/
	return(-1);
}


/*
 * close linux input module
 */

int
im_linux_close(im)
	struct i_module *im;
{
	if (im->im_name != NULL)
		free(im->im_name);

	if (im->path != NULL) {
		free(im->im_path);
		return(close(im->im_fd));
	}

	return(0);
}
