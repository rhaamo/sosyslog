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


/* standard input module header variables in context */
struct im_bsd_ctx {
	short	flags;
#define M_FLAG_INITIALIZED 0x1
#define M_FLAG_ERROR 0x2
	int	size;
	int	fd;
};



/*
 * initialize BSD input
 *
 */

extern int nfunix;
extern char *funixn[];
extern int *funix[];

int
im_bsd_init(I, argc, argv, c)
	struct i_module *I;
	int   argc;
	char   *argv[];
	struct im_header_ctx  **c;
{
	if ((I->fd = open(_PATH_KLOG, O_RDONLY, 0)) < 0) {
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
	}
	
        return(I->fd);
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
	char *p, line[MAXLINE + 1], outLine[MAXLINE + 1], *q, *lp;
        int i, c;

	(void)strcpy(outLine, _PATH_UNIX);
	(void)strcat(outLine, ": ");
	lp = line + strlen(outLine);

	i = read(im->fd, line, sizeof(line) - 1);
	if (i > 0) {
		line[i] = '\0';
		ret->len = i;
		for (p = line; *p != '\0'; ) {
		        /* fsync file after write */
		        ret->flags = SYNC_FILE | ADDDATE;
		        ret->pri = DEFSPRI;
		        if (*p == '<') {
		            ret->pri = 0;
		            while (isdigit(*++p))
		                ret->pri = 10 * ret->pri + (*p - '0');
		            if (*p == '>')
		                    ++p;
		        } else {
		                /* kernel printf's come out on console */
		                ret->flags |= IGN_CONS;
		        }
		        if (ret->pri &~ (LOG_FACMASK|LOG_PRIMASK))
		         ret->pri = DEFSPRI;
		}

		q = lp;
		while (*p != '\0' && (c = *p++) != '\n' &&
		        q < &line[MAXLINE])
		    *q++ = c;
		*q = '\0';
		ret->msg = strdup(outLine);
                ret->host = strdup(LocalHostName);
	} else if (i < 0 && errno != EINTR) {
		logerror("im_bsd_getLog");   
		im->fd = -1;
	}


	return(im->fd == -1 ? -1: 1);
}

int
im_bsd_close(im)
	struct i_module *im;
{
	return(close(im->fd));
}
