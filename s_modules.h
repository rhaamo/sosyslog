/* agregar licencia y acknowledge */ 

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
extern int	InetInuse;		/* if we are using network */

