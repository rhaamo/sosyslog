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
};



/*
 * get messge
 *
 */

int
im_bsd_getmsg(buf, size, c)
	char   *buf;
	int   size;
	struct im_header  *c;
{
	struct 
}

struct IModules {
	char	*im_name;
	short	im_type;
	int	fd;      			/*  for use with select() */
	int	(*im_getmsg) (char *, int); 	/* buf, bufsize */ 
	int	(*im_init) (int, char **, struct im_header **);
	int	(*im_close) (struct im_header *);
} IModules[MAX_N_IMODULES];

/*
 * get messge
 *
 */

int
im_bsd_init(i, line, c)
	char   *buf;
	int   size;
	struct im_header  **c;
{
	struct 
}


/*
 * get messge
 *
 */

int
im_bsd_getmsg(buf, size, c)
	char   *buf;
	int   size;
	struct im_header  *c;
{
	struct 
}


