/* $CoreSDI: typedefs.h,v 1.4 2001/03/27 22:50:26 alejo Exp $
 */

#ifndef PEO_TYPEEFS_H
#define PEO_TYPEEFS_H 1

#ifndef HAVE_U_INT32_T
# ifdef HAVE_UINT32_T
   typedef uint32_t u_int32_t;
# elif defined(HAVE___UINT32_T)
   typedef uint32_t __uint32_t;
# else
#  error Could not determine unsigned int 32 typedef
# endif
#endif

#ifndef HAVE_U_INT64_T
# ifdef HAVE_UINT64_T
   typedef uint64_t u_int64_t;
# elif defined(HAVE___UINT64_T)
   typedef __uint64_t __uint64_t;
# else
#  error Could not determine unsigned int 64 typedef
# endif
#endif

/* if __P isn't already defined... */
#ifdef __STDC__
# ifndef __P
#  define       __P(p)  p
# endif
#else
# ifndef __P
#  define       __P(p)  ()
# endif
#endif /* __STDC__ */

#endif /* ifdef PEO_TYPEEFS_H */
