/*      $Id: peochk.c,v 1.13 2000/05/03 22:00:54 claudio Exp $
 *
 * peochk - syslog -- Initial key generator and integrity log file checker
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 *
 * peochk [-f logfile] [-g] [-i key0file] [-k keyfile] [-l] [-m hash_method] [logfile]
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
 *	   /var/ssyslog/xxx where xxx is the logfile with all '/'
 *	   replaced by '.'
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "hash.h"
#include "../syslogd.h"

char	*default_logfile = "/var/log/messages";
char	*keyfile;
char	*key0file;
char	*logfile;
char	*macfile;
int	 method;
int	 use_stdin;

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
	"/var/ssyslog/xxx where xxx is the logfile with all '/' "
	"replaced by '.'\n\n"
	"Default values:\n"
	"\tlogfile    : /var/log/messages\n"
	"\tkeyfile    : /var/ssyslog/.var.log.messages.key\n"
	"\tkey0file   : strcat(keyfile, \"0\");\n"
	"\thash_method: sha1\n\n"
	"\tIf -l switch is specified, the hashed log file is "
	"strcat(keyfile, \".msg\");\n");
	exit(1);
}


/*
 * readline
 */
int readline (fd, buf, len)
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
 * check: read logfile and check it
 */
void
check()
{
	int  i;
	int  input;
	int  mfd;
	char key[41];
	int  keylen;
	char lastkey[41];
	int  lastkeylen;
	int  line;
	char mkey1[41];
	char mkey2[41];
	char msg[MAXLINE];
	
	/* open logfile */
	if (use_stdin)
		input = STDIN_FILENO;
	else if ( (input = open(logfile, O_RDONLY, 0)) == -1)
		err(1, logfile);

	/* open macfile */
	if (macfile)
		if ( (mfd = open(macfile, O_RDONLY, 0)) == -1)
			err(1, macfile);

	/* read initial key */
	if ( (i = open(key0file, O_RDONLY, 0)) == -1)
		err(1, key0file);
	if ( (keylen = read(i, key, 40)) == -1)
		err(1, key0file);
	if (!keylen)
		errx(1, "%s: file corrupted", key0file);
	key[keylen] = 0;
	close(i);

	/* read last key */
	if ( (i = open(keyfile, O_RDONLY, 0)) == -1)
		err(1, keyfile);
	if ( (lastkeylen = read(i, lastkey, 40)) == -1)
		err(1, keyfile);
	if (!lastkeylen)
		errx(1, "%s: file corrupted", keyfile);
	lastkey[lastkeylen] = 0;
	close(i);

	/* test both keys lenght */
	if (lastkeylen != keylen)
		errx(1, "%s and/or %s files corrupted", key0file, keyfile);

	/* check it */
	line = 1;
	while( (i = readline(input, msg, MAXLINE)) > 0) {
		if (macfile) {
			mac(SHA1, key, keylen, msg, mkey1);
			if (readline(mfd, mkey2, 40) < 0)
				err(1, macfile);
			if (strncmp(mkey2, mkey1, 40))
				errx(1, "%s file corrupted on line %i\n", logfile, line);
			line++;
		}
		keylen = mac(method, key, keylen, msg, key);
	}

	if (macfile)
		close(mfd);

	if (i < 0)
		errx(1, "error reading logs form %s", (use_stdin) ? "standard input" : logfile);

	if (strcmp(lastkey, key)) 
		errx(1, "%s file corrupted\n", logfile);

	fprintf(stderr, "%s file is ok\n", logfile);
}


/*
 * generate:
 *	generate initial key and write it on keyfile and key0file
 */
void
generate()
{
	char	 newkey[41];
	int	 len;
	int	 fkey;
	int	 fkey0;
	time_t	 t;

	time(&t);
	len = mac(method, NULL, 0, ctime(&t), newkey);
	if ( (fkey = open(keyfile, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1) {
		release();
		err(1, keyfile);
	}
	if ( (fkey0 = open(key0file, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR)) == -1) {
		unlink(keyfile);
		close(fkey);
		release();
		err(1, key0file);
	}

	/* write key 0 */
	write(fkey, newkey, len);
	write(fkey0, newkey, len);
	close(fkey);
	close(fkey0);
}
	

/*
 * main
 */
int
main (argc, argv)
	int argc;
	char **argv;
{
	int	 action;
	int	 ch;

	/* integrity check mode */
	action = 0;

	/* default values */
	use_stdin = 1;
	macfile = NULL;
	mcd = 0;
	logfile = default_logfile;
	keyfile = default_keyfile;
	key0file = NULL;
	method = SHA1;

	/* parse command line */
	while ( (ch = getopt(argc, argv, "f:ghi:k:lm:?")) != -1) {
		switch (ch) {
			case 'f':
				/* log file (intrusion detection mode) */
				if (logfile != default_logfile)
					free(logfile);
				if ( (logfile = strdup(optarg)) == NULL) {
					release();
					err(1, optarg);
				}
				use_stdin = 0;
				break;
			case 'g':
				/* generate new keyfile and initial key */
				action = 1;
				break;
			case 'i':
				/* key 0 file */
				if (key0file)
					free(key0file);
				if ( (key0file = strdup(optarg)) == NULL) {
					release();
					err(1, optarg);
				}
				break;
			case 'k':
				/* keyfile */
				if (keyfile != default_keyfile)
					free(keyfile);
				if ( (keyfile = strdup(optarg)) == NULL) {
					release();
					err(1, optarg); 
				}
				break;
			case 'l':
				macfile = (char*)1;
				break;
			case 'm':
				/* hash method */
				if ( (method = gethash(optarg)) < 0) {
					release();
					usage();
				}
				break;
			case 'h':
			case '?':
			default:
				release();
				usage();
		}

	}

	/* check logfile specified without -f switch */
	argc -= optind;
	argv += optind;
	if (argc && use_stdin)
		if ( (logfile = strdup(argv[argc-1])) == NULL) {
			release();
			err(1, argv[argc-1]);
		}

	/* if keyfile was not specified converted logfile is used instead */
	if (keyfile == default_keyfile && logfile != default_logfile) {
		char *tmp;
		if ( (tmp = strkey(logfile)) == NULL) {
			release();
			err(1, logfile);
		}
		keyfile = (char*) calloc(1, 14+strlen(tmp));
		strcpy(keyfile, "/var/ssyslog/");
		strcat(keyfile, tmp);
		free(tmp);
	}

	/* if key0file was not specified create one */
	if (key0file == NULL) {
		if ( (key0file = calloc(1, strlen(keyfile)+1)) == NULL) {
			release();
			err(1, keyfile);
		}
		strcpy(key0file, keyfile);
		strcat(key0file, "0");
	}

	/* create macfile */
	if (macfile) {
		if ( (macfile = (char*)calloc(1, strlen(keyfile)+4)) == NULL) {
		release();
		err(1, "macfile");
		}
		strcpy(macfile, keyfile);
		strcat(macfile, ".msg");
	}

	/* check integrity */
	if (action == 0)
		check();
	/* generate new key */
	else generate();

	release();
	return (0);
}


