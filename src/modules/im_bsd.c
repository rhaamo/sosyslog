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
im_bsd_getLog(im)
	struct i_module im;
{
	char line[MSGBSIZE + 1];
	int i;

	i = read(im->fd, line, sizeof(line) - 1);
	if (i > 0) {
		line[i] = '\0';
		printsys(line);
	} else if (i < 0 && errno != EINTR) {
		logerror("klog");   
		fklog = -1;
	}







printsys(msg)
        char *msg;
{
        int c, pri, flags;
        char *lp, *p, *q, line[MAXLINE + 1];

        (void)strcpy(line, _PATH_UNIX);
        (void)strcat(line, ": ");
        lp = line + strlen(line);
        for (p = msg; *p != '\0'; ) {
                flags = SYNC_FILE | ADDDATE;    /* fsync file after write */
                pri = DEFSPRI;
                if (*p == '<') {
                        pri = 0;
                        while (isdigit(*++p))
                                pri = 10 * pri + (*p - '0');
                        if (*p == '>')
                                ++p;
                } else {
                        /* kernel printf's come out on console */
                        flags |= IGN_CONS;
                }
                if (pri &~ (LOG_FACMASK|LOG_PRIMASK))
                        pri = DEFSPRI;
                q = lp;
                while (*p != '\0' && (c = *p++) != '\n' &&
                    q < &line[MAXLINE])
                        *q++ = c;
                *q = '\0';
                logmsg(pri, line, LocalHostName, flags);
        }
}






	
}


