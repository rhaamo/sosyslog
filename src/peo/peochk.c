/*	$CoreSDI: peochk.c,v 1.37 2000/06/22 00:53:41 claudio Exp $	*/

/*
 * Copyright (c) 2000, Core SDI S.A., Argentina
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
 * 3. Neither name of the Core SDI S.A. nor the names of its contributors
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
 * peochk - syslog -- Initial key generator and integrity log file checker
 *
 * Author: Claudio Castiglia, Core-SDI SA
 *
 *
 * peochk [-f logfile] [-g] [-i key0file] [-k keyfile] [-l]
 *        [-m hash_method] [-q] [logfile]
 *
 * supported hash_method values:
 *			md5
 *			rmd160
 *			sha1
 *
 * defaults:
 *	logfile: 	/var/log/messages
 *	keyfile:	/var/ssyslog/.var.log.messages.key
 *	hash_method:	sha1
 *
 * NOTES:
 *	1) When logfile is specified without the -f switch, the data is
 *	   readed from the standard input 
 *	2) If logfile is specified using both -f switch and without it,
 *	   the -f argument is used and data is readed from that file
 *	3) If logfile is specified but not the keyfile, this will be
 *	   /var/ssyslog/xxx.key where xxx is the logfile with all '/'
 *	   replaced by '.'
 *	4) If -l switch is specified, peochk detects the line number
 *	   corrupted on logfile
 *	5) -q means quiet mode
 *
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hash.h"
#include "syslogd.h"

#define CHECK	0x01
#define QUIET	0x02
#define ST_IN	0x04

int	 actionf;
char	*corrupted = "corrupted";
char	*default_logfile = "/var/log/messages";
char	*keyfile;
char	*key0file;
char	*logfile;
char	*macfile;
int	 method;

extern char *optarg;


/*
 * release allocated memory
 */
void
release()
{
	if (keyfile != default_keyfile)
		free(keyfile);
	if (key0file)
		free(key0file);
	if (logfile != default_logfile)
		free(logfile);
	if (macfile)
		free(macfile);
}


/*
 * usage
 */
void
usage()
{
	fprintf (stderr,
		"Usage:\n"
		"Check mode:\n"
		"    peochk [-h] [-l] [-f logfile] [-i key0file] [-k keyfile]\n"
		"           [-m hash_method] [-q] [logfile]\n\n"
		"Initial key generator mode:\n"
		"    peochk -g [-h] [-k keyfile] [-m hash_method] [logfile]"
		"\n\n");
	exit(-1);
}


/*
 * readline
 */
int
readline (int fd, char *buf, size_t len)
{
	int readed;
	int r;

	readed = 0;
	while (len) {
		if ( (r = read(fd, buf, 1)) == -1)
			return (-1);
		if (!r || *buf == '\n')
			break;
		buf++;
		readed++;
		len--;
	}
	*buf = '\0';
	return readed;
}


/*
 * exit:
 *	Prints a message on stdout and exits with a status
 */
void
eexit (int status, char *fmt, ...)
{
	va_list ap;
	if (fmt) {
		va_start (ap, fmt);
		vfprintf (stdout, fmt, ap);
		va_end(ap);
	}
	exit(status);
}
	
	
/*
 * check: read logfile and check it
 */
void
check()
{
	int   i;
	int   input;
	int   mfd;
	char  key[41];
	int   keylen;
	char  lastkey[21];
	int   lastkeylen;
	int   line;
	char  mkey1[21];
	int   mkey1len;
	char  mkey2[21];
	int   mkey2len;
	char  msg[MAXLINE];
	int   msglen;

	/* open logfile */
	if (actionf & ST_IN)
		input = STDIN_FILENO;
	else if ( (input = open(logfile, O_RDONLY, 0)) == -1)
		err(-1, logfile);

	mfd = 0;	/* shutup gcc */

	/* open macfile */
	if (macfile)
		if ( (mfd = open(macfile, O_RDONLY, 0)) == -1)
			err(-1, macfile);

	/* read initial key (as ascii string) and tranlate it to binary */
	if ( (i = open(key0file, O_RDONLY, 0)) == -1)
		err(-1, key0file);
	if ( (keylen = read(i, key, 40)) == -1)
		err(-1, key0file);
	if (!keylen) {
		if (actionf & QUIET)
			eexit(1, "1\n");
		else
			eexit(1, "(1) %s: %s\n", key0file, corrupted);
	}
	key[keylen] = 0;
	asc2bin(key, key);
	keylen >>= 1;
	close(i);

	/* read last key */
	if ( (i = open(keyfile, O_RDONLY, 0)) == -1)
		err(-1, keyfile);
	if ( (lastkeylen = read(i, lastkey, 20)) == -1)
		err(-1, keyfile);
	if (!lastkeylen) {
		if (actionf & QUIET)
			eexit(1, "1\n");
		else
			eexit(1, "(1) %s: %s\n", keyfile, corrupted);
	}
	close(i);

	/* test both key lenghts */
	if (lastkeylen != keylen) {
		if (actionf & QUIET)
			eexit(1, "1\n");
		else
			eexit(1, "(1) %s and/or %s %s\n", key0file, keyfile, corrupted);
	}

	/* check it */
	line = 1;
	while( (msglen = readline(input, msg, MAXLINE)) > 0) {
		if (macfile) {
			if ( (mkey1len = mac2(key, keylen, msg, msglen, mkey1)) < 0)
				err(-1, macfile);
			if ( (mkey2len = read(mfd, mkey2, mkey1len)) < 0)
				err(-1, macfile);
			if ((mkey2len != mkey1len) || memcmp(mkey2, mkey1, mkey1len)) {
				if (actionf & QUIET)
					eexit(1, "%i\n", line);
				else
					eexit(1, "(%i) %s %s on line %i\n", line, logfile, corrupted, line);
			}
			line++;
		}
		if ( (keylen = mac(method, key, keylen, msg, msglen, key)) == -1)
			err(-1, "fatal");
	}

	if (macfile)
		close(mfd);

	if (i < 0)
		errx(-1, "error reading logs form %s", (actionf & ST_IN) ? "standard input" : logfile);

	if (memcmp(lastkey, key, keylen)) {
		if (actionf & QUIET) 
			eexit(1, "1\n");
		else
			eexit(1, "(1) %s %s\n", logfile, corrupted);
	}
	if (actionf & QUIET)
		eexit(0, "0\n");
	else
		eexit(0, "(0) %s file is ok\n", logfile);
}


/*
 * generate:
 *	generate initial key and write it on keyfile and key0file
 *	in the last file data is written as ascii string
 */
void
generate()
{
	int	 kfd;
	int	 k0fd;
	char	 key[20];
	char	 keyasc[41];
	int	 keylen;
	char	 randvalue[20];

	if (getrandom(randvalue, 20) < 0) {
		release();
		err(-1, "getrandom");
	}
	if ( (keylen = mac(method, NULL, 0, randvalue, 20, key)) == -1) {
		release();
		err(-1, "fatal");
	}
	if ( (kfd = open(keyfile, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1) {
		release();
		err(-1, keyfile); 
	}
	if ( (k0fd = open(key0file, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1) {
		unlink(keyfile);
		close(kfd);
		release();
		err(-1, key0file);
	}

	/* write key 0 */
	write(kfd, key, keylen);
	write(k0fd, bin2asc(keyasc, key, keylen), keylen << 1);
	close(kfd);
	close(k0fd);
}
	

/*
 * main
 */
int
main (int argc, char **argv)
{
	int	 ch;
	int	 mac;

	/* integrity check mode, stdin */
	actionf = CHECK | ST_IN;

	/* default values */
	logfile = default_logfile;
	keyfile = default_keyfile;
	key0file = NULL;
	mac = 0;
	macfile = NULL;
	method = SHA1;

	/* parse command line */
	while ( (ch = getopt(argc, argv, "f:ghi:k:lm:q")) != -1) {
		switch (ch) {
			case 'f':
				/* log file (intrusion detection mode) */
				if (logfile != default_logfile)
					free(logfile);
				if ( (logfile = strrealpath(optarg)) == NULL) {
					release();
					err(-1, optarg);
				}
				actionf &= ~ST_IN;
				break;
			case 'g':
				/* generate new keyfile and initial key */
				actionf &= ~CHECK;
				break;
			case 'i':
				/* key 0 file */
				if (key0file)
					free(key0file);
				if ( (key0file = strdup(optarg)) == NULL) {
					release();
					err(-1, optarg);
				}
				break;
			case 'k':
				/* keyfile */
				if (keyfile != default_keyfile)
					free(keyfile);
				if ( (keyfile = strdup(optarg)) == NULL) {
					release();
					err(-1, optarg); 
				}
				break;
			case 'l':
				mac = 1;
				break;
			case 'm':
				/* hash method */
				if ( (method = gethash(optarg)) < 0) {
					release();
					usage();
				}
				break;
			case 'q':
				/* quiet mode */
				actionf |= QUIET;
				break;
			case 'h':
			default:
				release();
				usage();
		}

	}

	/* check logfile specified without -f switch */
	argc -= optind;
	argv += optind;
	if (argc && (actionf & ST_IN))
		if ( (logfile = strrealpath(argv[argc-1])) == NULL) {
			release();
			err(-1, argv[argc-1]);
		}

	/* if keyfile was not specified converted logfile is used instead */
	if (keyfile == default_keyfile && logfile != default_logfile) {
		char *tmp;

		if ( (tmp = strallocat("/var/ssyslog/", logfile)) == NULL) {
			release();
			err(-1, "buffer for keyfile");
		}
		strdot(tmp+13);
		if ( (keyfile = strallocat(tmp, ".key")) == NULL) {
			free(tmp);
			release();
			err(-1, "buffer for keyfile");
		}
		free(tmp);
	}

	/* if key0file was not specified create one */
	if (key0file == NULL)
		if ( (key0file = strkey0(keyfile)) == NULL) {
		release();
		err(-1, "creating key0 file");
		}

	/* create macfile */
	if (mac)
		if ( (macfile = strmac(keyfile)) == NULL) {
			release();
			err(-1, "creating mac file");
		}

	/* execute action */
	if (actionf & CHECK)
		check();
	else
		generate();

	release();
	return (0);
}

