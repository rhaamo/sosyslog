/*
 * peochk - syslog -- Initial key generator and integrity log file checker
 *
 *
 * peochk [-f logfile] [-g] [-i key0file] [-k keyfile] [-m hash_method] [logfile]
 *
 * supported hash_method values:
 *			md5
 *			sha1
 *			rmd160
 *
 * defaults:
 *	logfile: 	/var/log/messages
 *	keyfile:	/var/ssyslog/.var.log.messages.key
 *	hash_method:	sha1
 *
 * NOTE:
 *	1) When logfile is specified without the -f switch, the data is
 *	   readed from the standard input 
 *	2) If logfile is specified using both -f switch and without it,
 *	   the -f argument is used and data is not read from the standard input
 *	3) If logfile is specified but not the keyfile, this will be
 *	   /var/ssyslog/xxx where xxx is the logfile with all '/'
 *	   replaced by '.'
 *
 */

#include <sys/types.h>
#include <sys/stat.h>
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

char	*default_logfile = "/var/log/messages";
char	*keyfile;
char	*key0file;
char	*logfile;
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
	"\tmd5, sha1, rmd160\n\n"
	"When no logfile is specified or it is without the -f switch "
	"the data is readed\n"
	"from the standard input.\n"
 	"If logfile is specified using both -f switch and without"
	"it, the -f argument\n"
	"is used and data is not read from the standard input.\n"
	"If the logfile is specified but not the keyfile, this will "
	"be /var/ssyslog/xxx\n"
	"where xxx is the logfile with all '.' replaced by ','\n\n"
	"Default values:\n"
	"\tlogfile    : /var/log/messages\n"
	"\tkeyfile    : /var/ssyslog/.var.log.messages.key\n"
	"\tkey0file   : strcat(keyfile, \"0\");\n"
	"\thash_method: sha1\n\n");
	exit(1);
}


/*
 * check: read logfile and check it
 */
void
check()
{
	int input;

	if (use_stdin)
		input = STDIN_FILENO;

	
}


/*
 * generate: generate initial key and write it on keyfile and key0file
 */
void
generate()
{
	char	*newkey[41];
	int	 len;
	int	 fkey;
	int	 fkey0;
	time_t	 t;

	time(&t);
	len = hash(method, NULL, 0, ctime(&t), newkey);
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
	write (fkey, newkey, len);
	write (fkey0, newkey, len);
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
	logfile = default_logfile;
	keyfile = default_keyfile;
	key0file = NULL;
	method = SHA1;

	/* parse command line */
	while ( (ch = getopt(argc, argv, "f:ghi:k:m:")) != -1) {
		switch (ch) {
			case 'f':
				/* log file (intrusion detection mode) */
				if (logfile != default_logfile)
					free(logfile);
				if ( (logfile = strdup(optarg)) == NULL) {
					release();
					err(1, NULL);
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
					err(1, NULL);
				}
				break;
			case 'k':
				/* keyfile */
				if (keyfile != default_keyfile)
					free(keyfile);
				if ( (keyfile = strdup(optarg)) == NULL) {
					release();
					err(1, NULL); 
				}
				break;
			case 'm':
				/* hash method */
				if (strcasecmp("md5", optarg) == 0)
					method = MD5;
				else if (strcasecmp("rmd160", optarg) == 0)
						method = RMD160;
				else if (strcasecmp("sha1", optarg) == 0)
						method = SHA1;
				else {
					release();
					usage();
				}
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
	if (argc && use_stdin)
		if ( (logfile = strdup(argv[argc-1])) == NULL) {
			release();
			err(1, NULL);
		}

	/* if keyfile was not specified converted logfile is used instead */
	if (keyfile == default_keyfile && logfile != default_logfile) {
		char *tmp;
		if ( (tmp = strkey(logfile)) == NULL) {
			release();
			err(1, NULL);
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
			err(1, NULL);
		}
		strcpy(key0file, keyfile);
		strcat(key0file, "0");
	}


	/* check logfile */
	if (action == 0)
		check();
	/* generate new key */
	else generate();

	release();
	return (0);
}


