/*  $Id: iolib.c,v 1.5 2000/04/28 20:39:36 alejo Exp $
 */
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <signal.h>
#include <stdio.h>
#include <syslog.h>
#include "syslogd.h"

/*
 *  AF_UNIX and PIPE functions
 */

int
path_open(path, mode)
	char *path;
	int mode;
{
	return(-1);
}

int
path_close()
{
	return(-1);
}

int
get_credentials()
{
	return(-1);
}

/*
 *  TCP functions
 */


int
tcp_connect()
{
	return(-1);
}

int
tcp_listen()
{
	return(-1);
}

/*
 *  UDP functions
 */

int
udp_client()
{
	return(-1);
}

int
udp_server()
{
	return(-1);
}

int
udp_connect()
{
	return(-1);
}

int
udp_close()
{
	return(-1);
}

int
udp_read()
{
	return(-1);
}

int
udp_write()
{
	return(-1);
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

