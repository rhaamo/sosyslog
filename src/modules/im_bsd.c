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
	struct im_bsd_ctx *ctx;

	*c = (struct im_header_ctx *) calloc(1, sizeof(struct im_bsd_ctx));
	ctx = (struct im_bsd_ctx *) *c;


#ifndef SUN_LEN
#define SUN_LEN(unp) (strlen((unp)->sun_path) + 2)
#endif
        for (i = 0; i < nfunix; i++) {
                (void)unlink(funixn[i]);

                memset(&sunx, 0, sizeof(sunx));
                sunx.sun_family = AF_UNIX;
                (void)strncpy(sunx.sun_path, funixn[i], sizeof(sunx.sun_path));
                funix[i] = socket(AF_UNIX, SOCK_DGRAM, 0);
                if (funix[i] < 0 ||
                    bind(funix[i], (struct sockaddr *)&sunx, SUN_LEN(&sunx)) < 0 ||
                    chmod(funixn[i], 0666) < 0) {
                        (void) snprintf(line, sizeof line, "cannot create %s",
                            funixn[i]);
                        logerror(line);
                        dprintf("cannot create %s (%d)\n", funixn[i], errno);
                        if (i == 0)
                                die(0);
                }
        }
        finet = socket(AF_INET, SOCK_DGRAM, 0);
        if (finet >= 0) {
                struct servent *sp;

                sp = getservbyname("syslog", "udp");
                if (sp == NULL) {
                        errno = 0;
                        logerror("syslog/udp: unknown service");
                        die(0);
                }
                memset(&sin, 0, sizeof(sin));
                sin.sin_len = sizeof(sin);
                sin.sin_family = AF_INET;
                sin.sin_port = LogPort = sp->s_port;
                if (bind(finet, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
                        logerror("bind");
                        if (!Debug)
                                die(0);
                } else {
                        InetInuse = 1;
                }
        }
 


	
}


/*
 * get messge
 *
 */

char *
im_bsd_getmsg(c)
	struct im_header_ctx  *c;
{
	struct im_bsd_ctx *ctx;

	ctx = (struct im_bsd_ctx *) c;

	
}


