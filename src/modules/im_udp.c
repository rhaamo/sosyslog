/*
 *  im_udp -- classic behaviour module for BDS like systems
 *      
 * Author: Alejo Sanchez for Core-SDI SA
 *         form syslogd.c by Eric Allman and Ralph Campbell
 *    
 */


#include <syslog.h>
#include "modules.h"
#include "syslogd.h"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>

void    logerror __P((char *));
void    die __P((int));


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
	struct sockaddr_in frominet;
	struct hostent *hent;
	int slen;

	if (ret == NULL) {
		dprintf("im_udp: arg is null\n");
		return (-1);
	}

	ret->im_pid = -1;
	ret->im_pri = -1;
	ret->im_flags = 0;

	slen = sizeof(frominet);
	ret->im_len = recvfrom(finet, ret->im_msg, MAXLINE, 0,
		(struct sockaddr *)&frominet, &slen);
	if (SecureMode) {
		/* silently drop it */
	} else {
		if (ret->im_len > 0) {
			ret->im_msg[ret->im_len] = '\0';
			hent = gethostbyaddr((char *) &frominet.sin_addr, frominet.sin_len,
					frominet.sin_family);
			strncpy(ret->im_host, hent->h_name, sizeof(ret->im_host));
		} else if (ret->im_len < 0 && errno != EINTR)
			logerror("recvfrom inet");
	}

	return(-1);

}

/*
 * initialize udp input
 *
 */

extern int nfunix;
extern char *funixn[];
extern int *funix[];

int
im_udp_init(I, argv, argc)
	struct i_module *I;
	char   **argv;
	int   argc;
{
	struct sockaddr_in sin;
	struct servent *sp;

        I->im_fd = socket(AF_INET, SOCK_DGRAM, 0);

	sp = getservbyname("syslog", "udp");
	if (sp == NULL) {
		errno = 0;
		logerror("syslog/udp: unknown service");
		die(0);
	}
	memset(&sin, 0, sizeof(sin));
#ifndef HAVE_LINUX
	sin.sin_len = sizeof(sin);
#endif
	sin.sin_family = AF_INET;
	sin.sin_port = LogPort = sp->s_port;
	if (bind(finet, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		logerror("bind");
		if (!Debug)
		die(0);
	} else {
                        InetInuse = 1;
	}

        I->im_type = IM_UDP;
        I->im_name = "udp";
        return(1);
}
