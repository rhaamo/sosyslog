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


#define MAX_MODULE_NAME_LEN 255

/* standard output module header variables in context */
struct om_header_ctx {
	short	flags;
#define M_FLAG_INITIALIZED 0x1
#define M_FLAG_ERROR 0x2
#define M_FLAG_LOCKED 0x4
#define M_FLAG_ROTATING 0x8
	int	size;
};

/* standard input module header variables in context */
struct im_header_ctx {
	short	flags;
#define M_FLAG_INITIALIZED 0x1
#define M_FLAG_ERROR 0x2
	int	size;
};

/*
 * This structure represents main details for the output modules
 */

struct o_module {
	struct	o_module *om_next;
	short	om_type;
	struct  om_header_ctx	*context;
};

/*
 * This structure represents main details for the input modules
 */

struct i_module {
	struct	i_module *im_next;
	short	im_type;
	int	fd;	/*  for use with select() */
	char	*im_name;
	char	*im_arg;
	struct  im_header_ctx	*context;
};

/*
 * This structure represents the return of the input modules
 */

struct im_msg {
	int	pid;
	int	pri;
	int	flags;
	int	len;
#define  SYSLOG_IM_PID_CHECKED	0x01
#define  SYSLOG_IM_HOST_CHECKED	0x02
	char	*msg;
	char	*host;
};


#endif
