/*	$CoreSDI: im_udp.c,v 1.27 2000/05/26 21:28:49 fgsch Exp $	*/

/*
 * im_udp -- input from INET using UDP
 *      
 * Author: Alejo Sanchez for Core SDI S.A.
 *         from syslogd.c by Eric Allman and Ralph Campbell
 *    
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <errno.h>
#include <netdb.h>
#include <syslog.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "modules.h"
#include "syslogd.h"

void    die __P((int));

extern int      finet;                  /* Internet datagram socket */
extern int      LogPort;                /* port number for INET connections */


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
	if (ret->im_len > 0) {
		ret->im_msg[ret->im_len] = '\0';
		hent = gethostbyaddr((char *) &frominet.sin_addr,
		    sizeof(frominet.sin_addr), frominet.sin_family);
		if (hent)
			strncpy(ret->im_host, hent->h_name,
			    sizeof(ret->im_host));
		else
			strncpy(ret->im_host, inet_ntoa(frominet.sin_addr),
			    sizeof(ret->im_host));
	} else if (ret->im_len < 0 && errno != EINTR)
		logerror("recvfrom inet");

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
	struct sockaddr_in sin;
	struct servent *sp;

        if (finet > -1) {
		dprintf("im_udp_init: already opened!\n");
		return(-1);
        }
        finet = socket(AF_INET, SOCK_DGRAM, 0);

	sp = getservbyname("syslog", "udp");
	if (sp == NULL) {
		errno = 0;
		logerror("syslog/udp: unknown service");
		die(0);
	}
	memset(&sin, 0, sizeof(sin));
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
        I->im_name = strdup("udp");
        I->im_path = "";
        I->im_fd   = finet;
        return(1);
}

int
im_udp_close(im) 
        struct i_module *im;
{
        if (im->im_path)
		free(im->im_path);

        if (im->im_name)
		free(im->im_name);

        return(close(im->im_fd));
}
