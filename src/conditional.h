/*	$CoreSDI: conditional.h,v 1.2 2000/11/06 18:14:35 alejo Exp $	*/

/*
 * Needed vars for Solaris machines
 */

/*
 * Coded by Ari <edelkind@phri.nyu.edu>
 */

#ifndef HAVE_CONDITIONAL_H
#define HAVE_CONDITIONAL_H

#if (__svr4__ && __sun__)
/* solaris doesn't have u_int_32 or u_int_64 */
typedef uint32_t u_int32_t;
typedef uint64_t u_int64_t;
#elif sgi
/* irix has u_int32_t, but not u_int64_t or socklen_t */
typedef __uint64_t u_int64_t;
typedef __uint32_t socklen_t;
#endif

/* if __P isn't already defined... */
#ifdef __STDC__
#ifndef __P
#define	__P(p)  p
#endif
#else
#ifndef __P
#define	__P(p)  ()
#endif
#endif /* __STDC__ */


/* syslog code taken mostly from linux */
#if (__svr4__ && __sun__)
# ifdef SYSLOG_NAMES

# ifndef LOG_MAKEPRI
#  define LOG_MAKEPRI(fac, pri) (((fac) << 3) | (pri))
# endif

# ifndef LOG_PRI
#  define LOG_PRI(p) ((p) & LOG_PRIMASK)
# endif

# ifndef LOG_FAC
#  define LOG_FAC(p) (((p) & LOG_FACMASK) >> 3)
# endif

# define INTERNAL_NOPRI 0x10
# define INTERNAL_MARK  LOG_MAKEPRI(LOG_NFACILITIES, 0) 

typedef struct _code {
	char *c_name;
	int  c_val;
} CODE;

CODE prioritynames[] =
  {
    { "alert", LOG_ALERT },
    { "crit", LOG_CRIT },
    { "debug", LOG_DEBUG },
    { "emerg", LOG_EMERG },
    { "err", LOG_ERR },
    { "error", LOG_ERR },		/* DEPRECATED */
    { "info", LOG_INFO },
    { "none", INTERNAL_NOPRI },		/* INTERNAL */
    { "notice", LOG_NOTICE },
    { "panic", LOG_EMERG },		/* DEPRECATED */
    { "warn", LOG_WARNING },	   	/* DEPRECATED */
    { "warning", LOG_WARNING },
    { NULL, -1 }
  };

CODE facilitynames[] =
  {
    { "auth", LOG_AUTH },
    { "cron", LOG_CRON },
    { "daemon", LOG_DAEMON },
    { "kern", LOG_KERN },
    { "lpr", LOG_LPR },
    { "mail", LOG_MAIL },
    { "mark", INTERNAL_MARK },		/* INTERNAL */
    { "news", LOG_NEWS },   
    { "security", LOG_AUTH },		/* DEPRECATED */ 
    { "syslog", LOG_SYSLOG },
    { "user", LOG_USER },
    { "uucp", LOG_UUCP },
    { "local0", LOG_LOCAL0 },
    { "local1", LOG_LOCAL1 },
    { "local2", LOG_LOCAL2 },
    { "local3", LOG_LOCAL3 },
    { "local4", LOG_LOCAL4 },
    { "local5", LOG_LOCAL5 },
    { "local6", LOG_LOCAL6 },
    { "local7", LOG_LOCAL7 },
    { NULL, -1 }
  };
    
# endif /* SYSLOG_NAMES */
#endif /* solaris */
    
    
/* if _PATH_UTMP isn't defined, define it here... */
#ifndef _PATH_UTMP
# ifdef UTMP_FILE
#  define _PATH_UTMP UTMP_FILE
# else  /* if UTMP_FILE */
#  define _PATH_UTMP "/var/adm/utmp"
# endif /* if UTMP_FILE */
#endif

/* if _PATH_DEVNULL isn't defined, define it here... */
#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

   
#endif /* !HAVE_CONDITIONAL_H */
