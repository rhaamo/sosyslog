/* Copyright (c) 1998-2002 Netarx Inc. All rights reserved. */

/*
 * om_refract -- Filter messages based on firing history
 *
 * Refraction is the property of neurons to not refire within some
 * limited timeperiod.
 * One mechanism is to specify the minimum amount of time between firings.
 * The following example will prevent the rule from firing within
 * 32 second refraction_period from the time of last firing.
 *
 * *.notice   %refract -p 32 -r 2 %classic /var/log/notices
 *
 */

#include "config.h"

#if TIME_WITH_SYS_TIME

# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <sys/param.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "../modules.h"
#include "../syslogd.h"

/* current time from syslogd */

struct om_refract_ctx {
  time_t refraction_end_time;
  int fired_count;
  int refraction_period;

#define  OM_FILTER_PERIOD  0x01  /* FILTER BASED ON TIME */
#define  OM_FILTER_RULES   0x02  /* FILTER BASED ON PRIOR RULES */
#define  OM_FILTER_HURST   0x04  /* FILTER BASED ON CHANGE IN FRACTAL DIMENSION */
  int filter_bitstring;          /* BITWISE 'OR' OF ABOVE FILTER TYPES */

/* FUTURE STUFF -- NOT CURRENTLY USED
  short    flags; */
};


/*
 *  INIT -- Initialize om_refract
 *
 *   -p <seconds> flag indicates that the rule may not fire within <seconds> seconds of its last firing.
 *   -h use the Hurst exponent to determine a change in dimension.
 *
 */
int
om_refract_init(
    int argc,              /* Argumemt count */
    char* *argv,           /* Argumemt array, like main() */
    struct filed* f,       /*  Our filed structure */ 
    struct global* global, /* Global variable */  
    void* *context,        /* Our context */     
    char* *status)         /* status */     
{

  struct om_refract_ctx *ctx;
  char statbuf[1024];
  char ch;
  char *endptr;

  int argcnt;

  /* FOR DEBUGGING PURPOSES */
  m_dprintf(MSYSLOG_INFORMATIVE, "om_refract_init: starting\n");

  if (argc < 2 || argv == NULL || argv[1] == NULL) {
    snprintf( statbuf, sizeof(statbuf), "om_refract_init: error on initialization" );
    m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
    *status = strdup(statbuf);
    return (-1);
  }

  snprintf( statbuf, sizeof(statbuf), "om_refract: filtering" );
  m_dprintf(MSYSLOG_INFORMATIVE, "%s\n", statbuf);

  *context = (void *) calloc(1, sizeof(struct om_refract_ctx));
  if (*context == NULL) return (-1);
  ctx = (struct om_refract_ctx *) *context;
  ctx->refraction_period = 0;  /* a zero indicates no time need pass until next firing */
  ctx->refraction_end_time = 0;
  ctx->fired_count = -1;  /* a negative indicates not refracted on firing */


  /*
   * Parse options with getopt(3)
   */
  optind = 1;
#ifdef HAVE_OPTRESET
  optreset = 1;
#endif

  /* 'h' OPTION NOT YET IMPLEMENTED */
  for(  argcnt = 1 ; ( ch = getxopt(argc, argv, "p!period: c!count:", &argcnt) ) != -1 ; ++argcnt  ) {
    switch (ch) {
  
      case 'p':
        ctx->refraction_period = strtol(argv[argcnt], &endptr, 0);
        if (endptr == NULL || endptr == argv[argcnt]) { 
          snprintf(statbuf, sizeof(statbuf), "om_refract_init: "
           "bad argument to -p option [%s], should be numeric refraction period", argv[argcnt]);
          m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
          *status = strdup(statbuf);
          free(*context);
return(-1);
        }
  
        ctx->filter_bitstring |= OM_FILTER_PERIOD;
    break;

      case 'r':
          ctx->fired_count = strtol(argv[argcnt], &endptr, 0);
          if (endptr == NULL || endptr == argv[argcnt]) { 
            snprintf(statbuf, sizeof(statbuf), "om_refract_init: "
              "bad argument to -r option [%s], should be numeric, rules fired count", argv[argcnt]);
            m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
            *status = strdup(statbuf);
            free(*context);
            return(-1);
          }
          ctx->filter_bitstring |= OM_FILTER_RULES;
    break;
  
      case 'c':
        ctx->fire_count = strtol(argv[argcnt], &endptr, 0);
        if (endptr == NULL || endptr == argv[argcnt]) { 
          snprintf(statbuf, sizeof(statbuf), "om_refract_init: "
           "bad argument to -c option [%s], should be numeric firing count", argv[argcnt]);
          m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
          *status = strdup(statbuf);
          free(*context);
return(-1);
        }
    break;
  
      default:
        snprintf(statbuf, sizeof(statbuf), "om_refract_init: bad option [%c]", ch);
        m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
        *status = strdup(statbuf);
        free(*context);
return (-1);
    }
  }

  snprintf(statbuf, sizeof(statbuf), "om_refract_init: initialization completed");
  m_dprintf(MSYSLOG_INFORMATIVE, "%s\n", statbuf);
  m_dprintf(MSYSLOG_INFORMATIVE, "refraction period is %d\n", ctx->refraction_period);

  *status = strdup(statbuf);
  return (1);
}

/*
 *  WRITE -- Write om_refract
 *
 * return: 
 *  -1  error
 *   1  success ->  forward the message to the next filter_bitstring 
 *   0  failure ->  drop the message on the floor, do not forward it.
 *
 */

int
om_refract_write(
  /* Current filed struct   */ struct filed* fil,
  /* Flags for this message */ int flags,
  /* The message string   */   struct m_msg* msg,
  /* Our context      */       void* context
) {

  struct om_refract_ctx *ctx;
  int filter_bitstring;  /* MODIFIABLE COPY OF filter_bitstring IN om_refract_ctx STRUCTURE */
  int ix;


  if (msg == NULL || msg->msg == NULL || !strcmp(msg->msg, "")) {
    logerror("om_refract_write: no message!");
    return (-1);
  }

  ctx = (struct om_refract_ctx *) context;

  /* DETERMINE IF AT LEAST ONE REFRACTION CONSTRAINT IS MET */

  filter_bitstring = ctx->filter_bitstring;
  for (ix = 1 ; filter_bitstring ; ix <<= 1) {

    if ( (filter_bitstring & ix) == 0 ) continue;
    filter_bitstring &= ~ix;  /* 0 THE BIT IN filter_bitstring */

    switch (ix) {
      case OM_FILTER_PERIOD:
        if ( fil->f_time < ctx->refraction_end_time ) 
return (0);
    break;

      case OM_FILTER_RULES:
        if ( msg->fired >= ctx->fired_count )
return (0);
    break;

      case OM_FILTER_HURST:
    break;
    }
  }

  /* I guess this module fires */
  ctx->refraction_end_time = fil->f_time + ctx->refraction_period;
return (1);
}

/*
  CLOSE:
  return:
  1  OK
   -1  BAD
 */

int
om_refract_close (struct filed *f, void *context) {
/*  struct om_refract_context *ctx = (struct om_refract_context *) context; */
  m_dprintf(MSYSLOG_INFORMATIVE, "om_refract_close: Not implemented\n");
  return (1);
}

/*
  FLUSH:
  return:
  1  OK
   -1  BAD
 */
int
om_refract_flush (struct filed *f, void *context) {
/*  struct om_refract_context *ctx = (struct om_refract_context *) context; */
  m_dprintf(MSYSLOG_INFORMATIVE, "om_refract_flush: Not implemented\n");
  return (1);
}
