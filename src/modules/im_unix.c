/*
 *  im_unix -- classic behaviour module for BDS like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         form syslogd.c by Eric Allman and Ralph Campbell
 *    
 */


#include <syslog.h>
#include "modules.h"
#include "syslogd.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <errno.h>


char   *cvthname __P((struct sockaddr_in *));

extern int nfunix;
extern char *funixn[];
extern int *funix[];


/* standard input module header variables in context */
struct im_unix_ctx {
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
im_unix_getLog(buf, size, c, r)
	char   *buf;
	int   size;
	struct im_header_ctx  *c;
	struct im_msg	*r;
{
	struct sockaddr_in frominet;
	int len, i;
	char line[MAXLINE + 1];

	len = sizeof(frominet);
	i = recvfrom(finet, line, MAXLINE, 0,
		(struct sockaddr *)&frominet, &len);
	if (SecureMode) {
		/* silently drop it */
	} else {
		if (i > 0) {
			line[i] = '\0';
			printline(cvthname(&frominet), line);
		} else if (i < 0 && errno != EINTR)
			logerror("recvfrom inet");
	}

}

/*
 * initialize unix input
 *
 */

extern int nfunix;
extern char *funixn[];
extern int *funix[];

int
im_unix_init(I)
	struct i_module *I;
{
	struct im_unix_ctx *ctx;
	int i;
	char line[MAXLINE + 1];
	struct sockaddr_un frominet;

	ctx = (struct im_unix_ctx *) calloc(1, sizeof(struct im_unix_ctx));

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

                sp = getservbyname("syslog", "unix");
                if (sp == NULL) {
                        errno = 0;
                        logerror("syslog/unix: unknown service");
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

        if ((fklog = open(_PATH_KLOG, O_RDONLY, 0)) < 0)
                dprintf("can't open %s (%d)\n", _PATH_KLOG, errno);


 


	
}


/*
 * get messge
 *
 */

char *
im_unix_getLog(c)
	struct im_header_ctx  *c;
{
	struct im_unix_ctx *ctx;

	ctx = (struct im_unix_ctx *) c;




                if (finet != -1 && FD_ISSET(finet, &readfds)) {
                        len = sizeof(frominet);
                        i = recvfrom(finet, line, MAXLINE, 0,
                            (struct sockaddr *)&frominet, &len);
                        if (SecureMode) {
                                /* silently drop it */
                        } else {
                                if (i > 0) {
                                        line[i] = '\0';
                                        printline(cvthname(&frominet), line);
                                } else if (i < 0 && errno != EINTR)
                                        logerror("recvfrom inet");
                        }
                }
                for (i = 0; i < nfunix; i++) {
                        if (funix[i] != -1 && FD_ISSET(funix[i], &readfds)) {
                                len = sizeof(fromunix);
                                len = recvfrom(funix[i], line, MAXLINE, 0,
                                    (struct sockaddr *)&fromunix, &len);
                                if (len > 0) {
                                        line[len] = '\0';
                                        printline(LocalHostName, line);
                                } else if (len < 0 && errno != EINTR)
                                        logerror("recvfrom unix");
                        }
                }


	
}


/*
 * Return a printable representation of a host address.
 */
char *
cvthname(f)
	struct sockaddr_in *f;
{
	struct hostent *hp;
	sigset_t omask, nmask;
	char *p;

	dprintf("cvthname(%s)\n", inet_ntoa(f->sin_addr));

	if (f->sin_family != AF_INET) {
		dprintf("Malformed from address\n");
		return ("???");
	}
	sigemptyset(&nmask);
	sigaddset(&nmask, SIGHUP);
	sigprocmask(SIG_BLOCK, &nmask, &omask);
	hp = gethostbyaddr((char *)&f->sin_addr,
	    sizeof(struct in_addr), f->sin_family);
	sigprocmask(SIG_SETMASK, &omask, NULL);
	if (hp == 0) {
		dprintf("Host name for your address (%s) unknown\n",
			inet_ntoa(f->sin_addr));
		return (inet_ntoa(f->sin_addr));
	}
	if ((p = strchr(hp->h_name, '.')) && strcmp(p + 1, LocalDomain) == 0)
		*p = '\0';
	return (hp->h_name);
}

