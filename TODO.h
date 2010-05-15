/*
 * File: TODO.h
 * Description: things we need to handle via configure.h
 */

/* Define as the return type of signal handlers (int or void).  */
#define RETSIGTYPE void

/* Define if you have the vprintf function.  */
#define HAVE_VPRINTF 1

/* Define if you don't have vprintf but do have _doprnt.  */
/* #undef HAVE_DOPRNT */

/* Define if you can safely include both <sys/time.h> and <time.h>.  */
#define TIME_WITH_SYS_TIME 1

#define HAVE_SOCKLEN_T 1
#define HAVE_UINT32_T 1
#define HAVE_UINT64_T 1
#define HAVE_U_INT32_T 1
#define HAVE___UINT32_T 1
#define HAVE_U_INT64_T 1
#define HAVE___UINT64_T 1

/* Define if you have the <paths.h> header file.  */
#define HAVE_PATHS_H 1

#define HAVE_GETADDRINFO 1

#define HAVE_INET_NTOP 1
#define HAVE_INET_ATON 1
#define HAVE_INET_ADDR 1

#define HAVE_CODE 1

#define HAVE_SYS_WAIT_H 1
