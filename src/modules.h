/* agregar licencia y acknowledge */ 

#ifndef SYSLOG_MODULES_H
#define SYSLOG_MODULES_H

extern int	Debug;			/* debug flag */
extern char	LocalHostName[];	/* our hostname */
extern char	*LocalDomain;		/* our local domain name */
extern int	InetInuse;		/* non-zero if INET sockets are
					   being used */
extern int	finet;			/* Internet datagram socket */
extern int	LogPort;		/* port number for INET connections */
extern int	Initialized;		/* set when we have initialized
					   ourselves */
extern int	MarkInterval;		/* interval between marks in seconds */
extern int	MarkSeq;		/* mark sequence number */
extern int	SecureMode;		/* when true, speak only unix domain
					   socks */
extern int	ModulesActive;		/* are we using modules */

extern char	*TypeNames[];		/* names for f_types */

extern char	*ConfFile;
extern char	*PidFile;
extern char	ctty[];

extern int	repeatinterval[];

/*
 * All module functions
 */

int om_classic_doLog(struct filed *, int , char *, struct om_header *);
int om_classic_init(int, char **, struct filed *, char *, struct om_header **);
int om_classic_close(struct filed*, struct om_header **);
int om_classic_flush(struct filed*, struct om_header *);

int om_mysql_doLog(struct filed *, int , char *, struct om_header *);
int om_mysql_init(int, char **, struct filed *, char *, struct om_header **);
int om_mysql_close(struct filed*, struct om_header **);
int om_mysql_flush(struct filed*, struct om_header *);


#endif
