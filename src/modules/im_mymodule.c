/*	$CoreSDI: im_mymodule.c,v 1.1 2000/05/27 02:03:59 alejo Exp $	*/

/*
 * im_mymodule -- Give some description
 *      
 * Author: Alejo Sanchez for Core SDI S.A.
 *         from syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

/* include all you need, no more ;) */
#include <sys/types.h>
#include <errno.h>
#include <syslog.h>
#include "modules.h"
#include "syslogd.h"

extern int      finet;         /* Internet datagram socket */
extern int      LogPort;       /* port number for INET connections */


/* standard input module header variables in context */
struct im_udp_ctx {
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
im_udp_getLog(im, ret)
	struct i_module	*im;
	struct im_msg	*ret;
{
	return(1);
}

/*
 * initialize udp input
 *
 */

int
im_udp_init(I, argv, argc)
	struct i_module *I;
	char   **argv;
	int   argc;
{
        return(1);
}

int
im_udp_close(im) 
        struct i_module *im;
{

        return(1);
}
