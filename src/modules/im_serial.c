/*	$Netarx: im_serial.c,v 1.12 2002/03/01 07:31:02 phreed Exp $	*/

/*
 * Copyright (c) 2001, Netarx Inc. USA
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither name of the Netarx Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * im_serial -- read from a log serial being written by other program
 *
 * Author: Fred Eisele for Netarx Inc.
 *
 */

#include "config.h"

#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../modules.h"
#include "../syslogd.h"

void printline(char *, char *, size_t, int);

struct im_serial_ctx {
	int   start;       /* start position of timestamp in the message */
	char  *timefmt;    /* the format to use in extracting the timestamp */
	char  *path;       /* the pathname of the serial to open */
	char  *name;       /* the alias for the serial (hostname) */
	struct stat stat;  /* the serial statistics (useful in determining if reg serial, serial, or whatever */
	char  saveline[MAXLINE + 3];   /* if the message is not complete, preserve it */
};

/*
 * initialize serial input
 */

int
im_serial_init(struct i_module *im, char **argv, int argc)
{
	struct im_serial_ctx	*c;
	int    ch, argcnt;

	m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_init: Entering\n");


	if ((im->im_ctx = malloc(sizeof(struct im_serial_ctx))) == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "im_serial_init: error allocating context!\n");
return (-1);
	}
	c = (struct im_serial_ctx *) im->im_ctx;
	c->start = 0;
	c->saveline[0] = '\0';

	/* parse command line */
	for ( argcnt = 1;	/* skip module name */
        (ch = getxopt(argc, argv,
          "f!serial: n!program: t!timeformat: s!startpos:", &argcnt)) != -1;
        argcnt++ )
  {
		switch (ch) {
		case 'f':
			/* serial to read */
			c->path = argv[argcnt];
			break;
		case 'n':
      /* create a alternate name, it will act as the hostname */
			c->name = strdup(argv[argcnt]);
			break;
		case 's':
			c->start = strtol(argv[argcnt], NULL, 10);
			break;
		case 't':
			/* time format to use */
			c->timefmt = strdup(argv[argcnt]);
			break;
		default:
			m_dprintf(MSYSLOG_SERIOUS,
          "im_serial_init: command line error, at arg %c [%s]\n", ch,
			    argv[argcnt]? argv[argcnt]: "");
return (-1);
		}
	}

	if (c->start && c->timefmt == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "im_serial_init: start specified for time string but no format!\n");
return (-1);
	}

	if (c->path == NULL) {
		m_dprintf(MSYSLOG_SERIOUS, "im_serial_init: no serial to read\n");
return (-1);
	}

	if (stat( c->path, &c->stat )) {
  	m_dprintf(MSYSLOG_SERIOUS,
        "im_serial_init: could not stat serial [%s] due to [%s]\n", 
        strerror( errno ), c->path);
return (-1);
 	}

	if (S_ISREG(c->stat.st_rdev)) {
    /* regular serial cannot be successfully polled */
    /* a separate mechanism will need to be used */
		m_dprintf(MSYSLOG_SERIOUS, "im_serial_init: regular serials not supported: [%s]\n", c->path);
return (-1);
	}

	if (c->name == NULL) c->name = c->path;	/* no name specified */

	watch_fd_input('p', im->im_fd , im);

 	m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_init: completed\n");
return (1);
}

/*
 * poll serial descriptor
 *  the im_serial_poll subroutine knows how to poll its own serial descriptor.
 */
/* int
im_serial_read(struct i_module *im)
{
return( 0 );
}
*/
/*
 * read message
 */

int
im_serial_read(struct i_module *im, int infd, struct im_msg  *ret)
{
	char *thisline = NULL;
	char *nextline = NULL;
	char *endmark = NULL;
	char *ptr = NULL;
	char *qtr = NULL;
  int wchr;
	int nx;
	struct im_serial_ctx	*c = (struct im_serial_ctx *) (im->im_ctx);
#ifdef IGNORE_PIPES
	RETSIGTYPE (*sigsave)(int);
#endif

 	m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: entering...\n");

#ifdef IGNORE_PIPES
	/* ignore sigserials */
	sigsave = place_signal(SIGPIPE, SIG_IGN);
#endif

  /* read a complete message converting non printable characters into 'X' */
	nx = read(im->im_fd, im->im_buf, sizeof(im->im_buf) - 1);

  if (nx < 0 && errno != EINTR) {
 	  m_dprintf(MSYSLOG_SERIOUS, "im_serial_read: error: [%d]\n", errno);
		logerror("im_serial_read");
#ifdef IGNORE_PIPES
	  place_signal(SIGPIPE, sigsave);
#endif
return -1;
	}

	endmark = im->im_buf + nx;
	*endmark = '\0';
  m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: bytes: [%d] [%s]\n", nx, im->im_buf);

  /* step through the lines read */
  for( thisline = im->im_buf; nextline < endmark; thisline = nextline ) {

    /* seek the end of the current line, a.k.a. the start of the nextline */
    for( nextline = thisline; nextline < endmark; nextline++ ) {

      /* a new line indicates a complete message */
      if (*nextline == '\n')  {
        *nextline = '\0';
    break;
      }

      /* a carriage return indicates then end of a message */
      if (*nextline == '\r')  {
        *nextline = '\0';
    break;
      }

      /* change non printable chars to 'X', as you go */
      if (! isprint((unsigned int) *nextline)) {
        *nextline = 'X';
    continue;
       }
    }
	 	m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: bytes remaining B: [%d]\n", (endmark - nextline));

    if ( nextline < endmark ) {
      m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: current line: [%s]\n", thisline);
      nextline++;
    }
    else {
      /* then end of the read buffer was reached */
      if ( strlen(thisline) == 0 ) {
        m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: end of transmission\n");
      }
      else {
        /* Preseve any partial message */
        int position;
        int save_end = strlen(c->saveline);

        m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: partial message: [%s] [%s]\n",
                        thisline, c->saveline );
        for( position = 0; (thisline + position) < endmark; position++ )
        {
           c->saveline[save_end + position] = thisline[position];
           if (save_end + position >= sizeof(c->saveline)) {
             m_dprintf(MSYSLOG_SERIOUS, "im_serial_read: partial message too long: [%d] [%s]\n",
                             sizeof(c->saveline), thisline);
           }
        }
        c->saveline[save_end + position] = '\0';
        m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: partial message: [%s]\n", c->saveline);
      }
return (0);
    }

    /* append the current line with any prior partial line */
    if (c->saveline != NULL && c->saveline[0] != '\0') {
      m_dprintf(MSYSLOG_INFORMATIVE,
          "im_serial_read: append current line with prior partial message: [%s] [%s]\n",
          c->saveline, thisline);
      strncat(c->saveline, thisline, sizeof(c->saveline) - 1);
      c->saveline[sizeof(c->saveline) - 1] = '\0';
      thisline = c->saveline;
    }

    /* skip empty lines */
    if (*thisline == '\0') {
      m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: empty line\n");
  continue;
    }

    { /* process the message */
			struct tm tm;
			char   *start, *end;

      m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: current message: [%s]\n", thisline);

      /* presume it does not contain a timestamp nor a host */
	  	ret->im_flags = ADDDATE | ADDHOST;
  		ret->im_pri = DEFSPRI;

			c = (struct im_serial_ctx *) im->im_ctx;

      if (c->timefmt) {  /* reform/insert the timestamp */
        if ((end = strptime((thisline + c->start), c->timefmt, &tm)) == NULL)
        {
			  	m_dprintf(MSYSLOG_WARNING,
              "im_serial_read: error locating timestamp from [%s] using format [%s]!\n",
              thisline, c->timefmt);
		  	}
        else {
          /* remove old timestamp from thisline */
		  		for (start = thisline + c->start; *end != '\0';) *start++ = *end++;
		  		*start = '\0';

				  if (strftime(ret->im_msg, sizeof(ret->im_msg) - 1, "%b %e %H:%M:%S ", &tm) == 0)
          {
				   	m_dprintf(MSYSLOG_WARNING, "im_serial_read: error rewriting timestamp!\n");
		 		  }
          else {
				  	ret->im_flags &= !ADDDATE;
          }
				}
			}
      /* generate the priority value, 0 by default */
      if (*thisline == '<') {
		  	ret->im_pri = 0;
		  	for ( ptr = thisline; isdigit((int)*ptr); ptr++ ) {
          ret->im_pri = 10 * ret->im_pri + (*ptr - '0');
        }
		  	if (*ptr == '>') ++ptr;
		  }

      /* put the hostname into the message */
		  strncat(ret->im_msg, c->name, sizeof(ret->im_msg) - 1);
	  	strncat(ret->im_msg, ":", sizeof(ret->im_msg) - 1);
	  	m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: reformed header: [%s]\n", ret->im_msg);

	  	if (ret->im_pri &~ (LOG_FACMASK|LOG_PRIMASK)) ret->im_pri = DEFSPRI;
	  	qtr = ret->im_msg + strlen(ret->im_msg);
	  	while (*ptr != '\0' && (wchr = *ptr++) != '\n' &&
	      	    qtr < &ret->im_msg[sizeof(ret->im_msg) - 1]) {
		  	*qtr++ = wchr;
		  }
		  *qtr = '\0';
		  ret->im_host[0] = '\0';
		  ret->im_len = strlen(ret->im_msg);

      printline( ret->im_host, thisline, strlen(thisline), im->im_flags );
	  	m_dprintf(MSYSLOG_INFORMATIVE, "im_serial_read: bytes remaining: [%d]\n", (endmark - nextline));
		}
	}

#ifdef IGNORE_PIPES
	/* restore previous SIGPIPE handler */
	place_signal(SIGPIPE, sigsave);
#endif
	return (0);
}

/*
 * close the serial descriptor and release all resources
 */

int
im_serial_close( struct i_module *im)
{
	struct im_serial_ctx	*c = (struct im_serial_ctx *) (im->im_ctx);

	if (c->timefmt != NULL) free(c->timefmt);
	if (c->name != NULL) free(c->name);
	if (c->path != NULL) free(c->path);

	if (im->im_ctx != NULL) free(im->im_ctx);
	if (im->im_fd >= 0) close(im->im_fd);

return (0);
}
