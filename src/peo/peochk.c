/*      $Id: peochk.c,v 1.26 2000/05/09 21:10:04 claudio Exp $
 *
 * peochk - syslog -- Initial key generator and integrity log file checker
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 *
 * peochk [-f logfile] [-g] [-i key0file] [-k keyfile] [-l] [-m hash_method] [-q] [logfile]
 *
 * supported hash_method values:
 *			md5
 *			rmd160
 *			sha1
 *
 * defaults:
 *	logfile: 	/var/log/messages
 *	keyfile:	/var/ssyslog/key.var.log.messages
 *	hash_method:	sha1
 *
 * NOTES:
 *	1) When logfile is specified without the -f switch, the data is
 *	   readed from the standard input 
 *	2) If logfile is specified using both -f switch and without it,
 *	   the -f argument is used and data is readed from that file
 *	3) If logfile is specified but not the keyfile, this will be
 *	   /var/ssyslog/key.xxx where xxx is the logfile with all '/'
 *	   replaced by '.'
 *	4) If -l switch is specified, peochk detects the line number
 *	   corrupted on logfile
 *	5) -q means quiet mode
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslog.h>
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
#include "../syslogd.h"

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
		"    peochk [-f logfile] [-i key0file] [-k keyfile]"
		"[-m hash_method] [logfile]\n\n"
		"Initial key generator mode:\n"
		"    peochk -g [-k keyfile] [-m hash_method] [logfile]\n\n"
		"hash_method options:\n"
		"\tmd5, rmd160, sha1\n\n"
		"When no logfile is specified or it is without the -f switch "
		"the data is read\n"
		"from the standard input.\n"
 		"If logfile is specified using both -f switch and without "
		"it, the -f argument\n"
		"is used and data is readed from that file.\n"
		"If the logfile is specified but not the keyfile, the default key"
		"file is\n"
		"/var/ssyslog/key.xxx where xxx is the logfile with all '/' "
		"replaced by '.'\n\n"
		"Default values:\n"
		"\tlogfile    : /var/log/messages\n"
		"\tkeyfile    : /var/ssyslog/key.var.log.messages\n"
		"\tkey0file   : strcat(keyfile, \"0\");\n"
		"\thash_method: sha1\n\n"
		"If -l switch is specified, the mac'ed log file is "
		"strcat(keyfile, \".mac\");\n");
	exit(-1);
}


/*
 * readline
 */
int
readline (fd, buf, len)
	int     fd;
	char   *buf;
	size_t  len;
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
 * qerr:
 *	err and errx functions with quiet mode support
 */
enum {
	ERR,
	ERRX
};

void
qerr (int quiet, int op, int eval, char *fmt, ...)
{
	va_list	ap;

	if (quiet)
		exit(eval);

	va_start(ap, fmt);
	switch(op) {
		case ERR:  /* err */
			verr(eval, fmt, ap);
		case ERRX: /* errx */
		default:
			verrx(eval, fmt, ap);
	}
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
		qerr(actionf & QUIET, ERR, -1, logfile);

	/* open macfile */
	if (macfile)
		if ( (mfd = open(macfile, O_RDONLY, 0)) == -1)
			qerr(0, ERR, -1, macfile);

	/* read initial key (as ascii string) and tranlate it to binary */
	if ( (i = open(key0file, O_RDONLY, 0)) == -1)
		qerr(0, ERR, -1, key0file);
	if ( (keylen = read(i, key, 40)) == -1)
		qerr(0, ERR, -1, key0file);
	if (!keylen)
		qerr(actionf & QUIET, ERRX, 1, "%s: %s\n", key0file, corrupted);
	key[keylen] = 0;
	asc2bin(key, key);
	keylen >>= 1;
	close(i);

	/* read last key */
	if ( (i = open(keyfile, O_RDONLY, 0)) == -1)
		qerr(0, ERR, -1, keyfile);
	if ( (lastkeylen = read(i, lastkey, 20)) == -1)
		qerr(0, ERR, -1, keyfile);
	if (!lastkeylen)
		qerr(actionf & QUIET, ERRX, 1, "%s: %s\n", keyfile, corrupted);
	close(i);

	/* test both key lenghts */
	if (lastkeylen != keylen)
		qerr(actionf & QUIET, ERRX, 1, "%s and/or %s %s\n", key0file, keyfile, corrupted);

	/* check it */
	line = 1;
	while( (msglen = readline(input, msg, MAXLINE)) > 0) {
		if (macfile) {
			if ( (mkey1len = mac2(key, keylen, msg, msglen, mkey1)) < 0)
				qerr(0, ERR, -1, macfile);
			if ( (mkey2len = read(mfd, mkey2, mkey1len)) < 0)
				qerr(0, ERR, -1, macfile);
			if ((mkey2len != mkey1len) || memcmp(mkey2, mkey1, mkey1len))
				qerr(actionf & QUIET, ERRX, line, "%s %s on line %i\n", logfile, corrupted, line);
			line++;
		}
		if ( (keylen = mac(method, key, keylen, msg, msglen, key)) == -1)
			qerr(0, ERR, -1, "fatal");
	}

	if (macfile)
		close(mfd);

	if (i < 0)
		qerr(0, ERRX, -1, "error reading logs form %s", (actionf & ST_IN) ? "standard input" : logfile);

	if (memcmp(lastkey, key, keylen)) 
		qerr(actionf & QUIET, ERRX, 1, "%s %s\n", logfile, corrupted);

	if (!(actionf & QUIET))
		fprintf(stderr, "%s file is ok\n", logfile);
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
		qerr(0, ERR, -1, "getrandom");
	}
	if ( (keylen = mac(method, NULL, 0, randvalue, 20, key)) == -1) {
		release();
		qerr(0, ERR, -1, "fatal");
	}
	if ( (kfd = open(keyfile, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1) {
		release();
		qerr(0, ERR, -1, keyfile); 
	}
	if ( (k0fd = open(key0file, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1) {
		unlink(keyfile);
		close(kfd);
		release();
		qerr(0, ERR, -1, key0file);
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
main (argc, argv)
	int argc;
	char **argv;
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
				if ( (logfile = strdup(optarg)) == NULL) {
					release();
					qerr(0, ERR, -1, optarg);
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
					qerr(0, ERR, -1, optarg);
				}
				break;
			case 'k':
				/* keyfile */
				if (keyfile != default_keyfile)
					free(keyfile);
				if ( (keyfile = strdup(optarg)) == NULL) {
					release();
					qerr(0, ERR, -1, optarg); 
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
		if ( (logfile = strdup(argv[argc-1])) == NULL) {
			release();
			qerr(0, ERR, -1, argv[argc-1]);
		}

	/* if keyfile was not specified converted logfile is used instead */
	if (keyfile == default_keyfile && logfile != default_logfile) {
		if ( (keyfile = strallocat("/var/log/ssyslog/key.", logfile)) == NULL) {
			release();
			qerr(0, ERR, -1, "buffer for keyfile");
		}
		strdot(&keyfile[17]);
	}

	/* if key0file was not specified create one */
	if (key0file == NULL)
		if ( (key0file = strkey0(keyfile)) == NULL) {
		release();
		qerr(0, ERR, -1, "creating key0 file");
		}

	/* create macfile */
	if (mac)
		if ( (macfile = strmac(keyfile)) == NULL) {
			release();
			qerr(0, ERR, -1, "creating mac file");
		}

	/* execute action */
	if (actionf & CHECK)
		check();
	else
		generate();

	release();
	return (0);
}


