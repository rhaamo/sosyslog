/*
 *  im_bsd -- classic behaviour module for BDS like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         form syslogd.c by Eric Allman and Ralph Campbell
 *    
 */


#include <modules.h>



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
im_bsd_init(argc, argv, c)
	int   argc;
	char   *argv[];
	struct im_header_ctx  **c;
{
	if (((*c)->fd = open(_PATH_KLOG, O_RDONLY, 0)) < 0) {
		dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);
	}
	
        return((*c)->fd);
}


/*
 * get messge
 *
 */

int
im_bsd_getLog(im, ret)
	struct i_module im;
        struct im_ret  *ret;
{
	char *p, line[MSG_BSIZE + 1];
        int i;

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

	} else if (i < 0 && errno != EINTR) {
		logerror("im_bsd_getLog");   
		im->fd = -1;
	}

	return(ret->fd == -1 ? -1: 1);
}






	
}


