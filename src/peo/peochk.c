/*
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
 *	2) If logfile is specified but not the keyfile, this will be
 *	   /var/ssyslog/xxx where xxx is the logfile with all '/'
 *	   replaced by '.'
 *
 */

#include <sys/types.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <md5.h>
#include <rmd160.h>
#include <sha1.h>
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
if (logfile != default_logfile)
	free(logfile);
free(key0file);
}


/*
 * usage
 */
void
usage()
{
	fprintf (stderr,
		"Usage:\n"
		"Audit mode:\n"
		"\tpeochk [-f logfile] [-i key0file] [-k keyfile] [-m hash_method]\n"
		"\tpeochk [-i key0file] [-k keyfile] [-m hash_method] [logfile]\n\n"
		"Initial key generator mode:\n"
		"peochk -g [-k keyfile] [-m hash_method] logfile\n\n"
		"hash_method options:\tmd5, sha1, rmd160\n\n"
		"When no logfile is specified or it is without the -f switch\n"
		"the data is readed from the standard input.\n"
		"If the logfile is specified but not the keyfile, this will\n"
		"be /var/ssyslog/xxx where xxx is the logfile whit all '.'\n"
		"replaced by ','\n\n"
		"Default values:\n"
		"\tlogfile    : /var/log/messages\n"
		"\tkeyfile    : /var/ssyslog/.var.log.messages.key\n"
		"\tkey0file   : strcat(keyfile, \"0\");\n"
		"\thash_method: sha1\n\n");
	exit(1);
}


/*
 * Audit: reads logfile 
 */
void
Audit()
{
}


/*
 * Generate: generate initial key and write it on keyfile and key0file
 */
void
Generate()
{
	char	*ct;
	char	*newkey[41];
	int	 len;
	int	 fkey;
	int	 fkey0;
	time_t	 t;

	time(&t);
	len = hash(method, NULL, 0, ctime(&t), newkey);
	if ( (fkey = open(keyfile, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR) == -1) ||
	     (fkey0 = open(key0file, O_WRONLY|O_CREAT|O_EXCL, S_IRUSR|S_IWUSR) == -1)) {
		if (fkey) {
			unlink(keyfile);
			close(fkey);
		}
		if (fkey0) {
			unlink(key0file);
			close(fkey0);
		}
		
		
	

	
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

	/* intrusion detection */
	action = 0;

	/* default values */
	use_stdin = 1;
	logfile = default_logfile;
	keyfile = default_keyfile;
	method = SHA1;
	if ( (key0file = strkey(keyfile)) == NULL)
		err(1, NULL);

	/* parse command line */
	while ( (ch = getopt(argc, argv, "f:gk:m:")) != -1) {
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
				if (strcasecmp("md5", optarg) == NULL)
					method = MD5;
				else if (strcasecmp("rmd160", optarg) == NULL)
					method = RMD160;
				else if (strcasecmp("sha1", optarg) == NULL)
					method = SHA1;
				else {
					release();
					usage();
				}
				break;
			default:
				release();
				usage();
			}

	}

	/* audit logfile */
	if (action == 0)
		Audit();
	/* generate new key and save it */
	else 
		Generate();

	release();
	return (0);
}


