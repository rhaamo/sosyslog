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
 * get messge
 *
 */

int
im_bsd_getmsg(buf, size, c, r)
	char   *buf;
	int   size;
	struct im_header_ctx  *c;
	struct im_ret     *r;
{
	struct 
}

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
im_bsd_getmsg(c)
	struct im_header_ctx  *c;
{
	ctx = (struct im_bsd_ctx *) c;

                        i = read(fklog, line, sizeof(line) - 1);
                        if (i > 0) {
                                line[i] = '\0';
                                printsys(line);
                        } else if (i < 0 && errno != EINTR) {
                                logerror("klog");   
                                fklog = -1;
                        }

	
}


