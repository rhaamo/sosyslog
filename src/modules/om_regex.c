/*  $Id: om_regex.c,v 1.50 2003/04/04 18:38:26 alejo Exp $	*/

/*
 * Copyright (c) 2001, Core SDI S.A., Argentina
 * All rights reserved
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither name of the Core SDI S.A. nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * om_regex -- Filter messages using regular exressions
 *
 * Author: Alejo Sanchez for Core-SDI SA
 *         Idea of Emiliano Kargieman
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
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <regex.h>

#include "../modules.h"
#include "../syslogd.h"

/* current time from syslogd */

#define min(x,y) ( (x) < (y) ? (x) : (y) )


struct om_regex_ctx {
  short flags;
  int size;

  int filters;
#define  OM_FILTER_MESSAGE  0x01
#define  OM_FILTER_HOST     0x02
#define  OM_FILTER_INVERSE  0x04
#define  OM_FILTER_SUBST    0x08

  regex_t msg_exp;
  int msg_no_subexp;

  int msg_no_subst;
  char **msg_non_subst;
  int *msg_subexp_no;

  regex_t host_exp;
  int host_no_subexp;

  int host_no_subst;
  char **host_non_subst;
  int *host_subexp_no;
};


static int count_subexp(const regex_t *, const char *);
static void parse_subst(const regex_t *, const char *, int *, char ***, int **);
static char *unbackslash(char *);
static int max_subexp(const int *, int);

/*
 *  INIT -- Initialize om_regex
 *
 */
int om_regex_init(
 int argc,
 char **argv,
 struct filed *f,
 struct global* global,
 void **ctx,
 char **status
) {

  struct om_regex_ctx *c;

  char statbuf[1048];
  int statbuf_len;

  char *p;
  int argcnt;

  int ch;

  char opt_recent = 0;    /* MOST RECENT OPTION ('h' OR 'm') TO WHICH 's' OPTION PERTAINS */
  char m_processed = 0,
       h_processed = 0;   /* SO MULTIPLE 'm' AND 'h' OPTIONS ARE IGNORED */

  /* subexp_regex LOOKS INDICATION OF SUB-EXPRESSION (UNESCAPED LEFT  */
  /* PARENTHESES '(') IN A MESSAGE OR HOST REGULAR EXPRESSION (REGEX) */ 
  char subexp_regex[] = "(^|[^\\])(\\\\\\\\)*\\\("; /* LOTS 'O '\'s (STRING INTERP THEN REGEX INTERP) */
  regex_t subexp_regex_comp;  /* COMPILED subexp_regex */

  /* subst_regex LOOKS FOR INDICATION OF SUBSTITUTION VARIABLE (UNESCAPED '$'     */
  /* FOLLOWED BY ONE OR MORE DIGITS) IN A MESSAGE OR HOST SUBSTITUTION EXPRESSION */
  char subst_regex[] = "(^|[^\\])(\\\\\\\\)*(\\$[0-9]+)"; /* LOTS 'O '\'s (STRING INTERP THEN REGEX INTERP) */
#define SUB_SUBEXP 4         /* NUMBER OF SUBEXPRESSIONS IN subst_regex (WHOLE + 3 CLUSTERS) */
  regex_t subst_regex_comp;  /* COMPILED subst_regex */


  /* COMPILE subexp_regex */
  if ( regcomp(&subexp_regex_comp, subexp_regex, REG_EXTENDED) != 0 ) {
    m_dprintf(MSYSLOG_CRITICAL, "om_regex: error compiling "
     "substitute-expression regular expression [%s] for message\n", subexp_regex);
    return (-1);
  }

  /* COMPILE subst_regex */
  if ( regcomp(&subst_regex_comp, subst_regex, REG_EXTENDED) != 0 ) {
    m_dprintf(MSYSLOG_CRITICAL, "om_regex: error compiling "
     "substitute-expression regular expression [%s] for message\n", subst_regex);
    return (-1);
  }

  /* for debugging purposes */
  m_dprintf(MSYSLOG_INFORMATIVE, "om_regex init\n");

  if (argc < 2 || argv == NULL || argv[1] == NULL) {
    m_dprintf(MSYSLOG_SERIOUS, "om_regex: error on initialization\n");
    return (-1);
  }

  /* ctx IS WHAT WILL BE "RETURNED" TO THE CALLING ROUTINE */
  /* TO PASS ON TO OTHER FUNCTIONS OF THIS MODULE          */
  if (   (  *ctx = calloc( 1, sizeof(struct om_regex_ctx) )  ) == NULL   ) return -1;

  /* c POINTS TO THE SAME STRUCTURE AS *ctx, EXCEPT IT IS A TYPED (I.E. NON-void) POINTER */
  c = *ctx;
  c->size = sizeof(struct om_regex_ctx);

  statbuf_len = snprintf(statbuf, sizeof(statbuf), "om_regex: filtering");

  /*
   * Parse options with getopt(3)
   *
   * we give an example for a -s argument
   * -v flag means INVERSE matching
   * -m flag match message
   * -h flag match host
   * -s flag substitute for message or host, whichever option (m or h) is most recent
   *
   */

  for(argcnt = 1; (ch = getxopt(argc, argv, "v!reverse!inverse m!message: "
	    "h!host: s!subst: i!icase", &argcnt)) != -1; argcnt++) {

	p = NULL;

	switch (ch) {

	case 'v':
		if (c->filters & OM_FILTER_SUBST) {
			m_dprintf(MSYSLOG_WARNING, "om_regex: error compiling "
			    "inverse regular expression incompatible with "
			    "substitution\n");
			break;
		}
		c->filters |= OM_FILTER_INVERSE;
		statbuf_len += snprintf(statbuf + statbuf_len,
		    sizeof(statbuf) - statbuf_len, ", inverse");
		break;

	case 'm':
		if (m_processed) {
			m_dprintf(MSYSLOG_WARNING, "om_regex: m option "
			    "specified more than once, only first accepted\n");
			break;
		}
		c->filters |= OM_FILTER_MESSAGE;

		if (regcomp(&c->msg_exp, argv[argcnt], REG_EXTENDED) != 0) {

			m_dprintf(MSYSLOG_SERIOUS, "om_regex: error compiling "
			    "regular expression [%s] for message\n",
			    argv[argcnt]);
			free(*ctx);
			return -1;
		}

		c->msg_no_subexp = count_subexp(&subexp_regex_comp,
		    argv[argcnt]);
		p = ", message";
		opt_recent = 'm';
		m_processed = 1;
		break;

	case 'h':
		if (h_processed) {
			m_dprintf(MSYSLOG_WARNING, "om_regex: h option "
			    "specified more than once, only first accepted\n");
			break;
		}
		c->filters |= OM_FILTER_HOST;

		if (regcomp(&c->host_exp, argv[argcnt], REG_EXTENDED) != 0) {

			m_dprintf(MSYSLOG_SERIOUS, "om_regex: error compiling "
			    "regular expression [%s] for message\n",
			    argv[argcnt]);
			free(*ctx);
			return -1;
		}

		c->host_no_subexp = count_subexp(&subexp_regex_comp,
		    argv[argcnt]);
		p = ", host";
		opt_recent = 'h';
		h_processed = 1;
		break;

	case 's':
		if (c->filters & OM_FILTER_INVERSE) {
			m_dprintf(MSYSLOG_WARNING, "om_regex: error compiling "
			    "substitution incompatible with inverse regular "
			    "expression\n");
		}
		if (opt_recent == 0) {
			m_dprintf(MSYSLOG_WARNING, "om_regex: s option "
			    "specified with no matching m or h option\n");
			break;
		}

		{
		int no_subexp;
		int max_subexp_no;
		int *no_subst;
		char ***non_subst;
		int **subexp_no;

		if (opt_recent == 'm') {
			no_subexp = c->msg_no_subexp;
			no_subst = &c->msg_no_subst;
			non_subst = &c->msg_non_subst;
			subexp_no = &c->msg_subexp_no;
		} else {    /* if (opt_recent == 'h')  */
			no_subexp = c->host_no_subexp;
			no_subst = &c->host_no_subst;
			non_subst = &c->host_non_subst;
			subexp_no = &c->host_subexp_no;
		}
		parse_subst(&subst_regex_comp, argv[argcnt], no_subst,
		    non_subst, subexp_no);

		max_subexp_no = max_subexp(*subexp_no, *no_subst);

		if (max_subexp_no > no_subexp) {
			m_dprintf(MSYSLOG_SERIOUS, "om_regex: substitution "
			    "pattern references sub-expression [%d], max is "
			    "[%d]\n", max_subexp_no, no_subexp);
			return -1;
		}
		}

		opt_recent = 0;

		c->filters |= OM_FILTER_SUBST;
		p = ", subst";
		break;

	default:
		m_dprintf(MSYSLOG_SERIOUS, "om_regex: unknown parameter [%c]\n",
		    ch);
		free(*ctx);
		return -1;

	}

	if (p)
		statbuf_len += snprintf(statbuf + statbuf_len,
		    sizeof(statbuf) - statbuf_len, " %s [%s]", p, argv[argcnt]);

  } /* END for(getxopt) */

  *status = strdup(statbuf);

  return 1;
}


static int count_subexp(const regex_t *regex_comp, const char *argv) {

  regmatch_t pmatch;
  const char *start = argv;
  int count = 0;

  while ( regexec(regex_comp, start, 1, &pmatch, 0) == 0 ) {
    start += pmatch.rm_eo;
    ++count;
  }

  return count;
}


static void parse_subst(const regex_t *regex_comp, const char *argv, int *no_subst, char ***non_subst, int **subexp_no) {

  regmatch_t pmatch[SUB_SUBEXP];
  const char *start = argv;

  int current_element = 0;
  int current_bound = 0;
  const int realloc_growth = 10;


  *non_subst = NULL;
  *subexp_no = NULL;

  while ( regexec(regex_comp, start, SUB_SUBEXP, pmatch, 0) == 0 ) {
    if (current_element >= current_bound) {
      current_bound += realloc_growth;
      *non_subst = realloc( *non_subst, current_bound * sizeof(char *) );
      *subexp_no = realloc( *subexp_no, current_bound * sizeof(int) );
    }
    (*non_subst)[current_element] = malloc(pmatch[3].rm_so);
    strncpy((*non_subst)[current_element], start, pmatch[3].rm_so - 1);
    (*non_subst)[current_element][pmatch[3].rm_so - 1] = '\0';
    unbackslash((*non_subst)[current_element]);

    (*subexp_no)[current_element] = atoi(&start[ pmatch[3].rm_so + 1 ]); 
    ++current_element;

    start += pmatch[3].rm_eo;
  }

  if (current_element >= current_bound) {
    *non_subst = realloc( *non_subst, (current_bound += realloc_growth) * sizeof(char *) );
  }
  (*non_subst)[current_element] = unbackslash( strdup(start) ); /* LAST NON-SUB */

  *no_subst = current_element;

  return;
}


static char *unbackslash(char *string) {

  char *to, *from;
  to = from = string;

  while(*from) {
    if (*from == '\\') ++from;
    *to++ = *from++;
  }
  *to = '\0';

  return string;
}


static int max_subexp(const int *array, int no_elem) {

  int ix;
  int max_subexp_no = 0;
  for(ix = 0 ; ix < no_elem ; ++ix) {
    if (*array > max_subexp_no) max_subexp_no = *array;
    ++array;
  }

  return max_subexp_no;
}


/* return:
   -1  error
    1  match  -> successfull
    0  nomatch -> stop logging it
 */

int om_regex_write(struct filed *f, int flags, struct m_msg *m, void *ctx)
{
  struct om_regex_ctx *c;
  int i, iflag;
  static char new_host[MAXLINE+2];
  static char new_msg[MAXLINE+2];

  c = ctx;

  if (m == NULL || m->msg == NULL || !strcmp(m->msg, "")) {
    logerror("om_regex_write: no message!");
return (-1);
  }

  iflag = ( (c->filters & OM_FILTER_INVERSE) != 0 );

  for (i = 1; i < OM_FILTER_INVERSE; i <<= 1) {

  if ( (c->filters & i) == 0 ) continue;

    {
      char *string, *new_string;
      regex_t *regex_comp;
      int no_subst;
      int no_subexp;
      char **non_subst;
      int *subexp_no;

      switch (i) {
        case OM_FILTER_MESSAGE:
          memset(new_msg, '\0', MAXLINE+2);

          string = m->msg;			new_string = new_msg;
          regex_comp = &c->msg_exp;		no_subexp = c->msg_no_subexp;
          no_subst = c->msg_no_subst;		non_subst = c->msg_non_subst;
          subexp_no = c->msg_subexp_no;
      break;

        default:  /* case OM_FILTER_HOST: */
          memset(new_host, '\0', MAXLINE+2);

          string = f->f_prevhost;		new_string = new_host;
          regex_comp = &c->host_exp;		no_subexp = c->host_no_subexp;
          no_subst = c->host_no_subst;		non_subst = c->host_non_subst;
          subexp_no = c->host_subexp_no;
      break;
      }

      m_dprintf(MSYSLOG_INFORMATIVE, "om_regex_write: type[%d]: [%s]\n", i, string);
      {
        regmatch_t pmatch[no_subexp + 1];

        if (  ( regexec(regex_comp, string, no_subexp + 1, pmatch, 0) != 0 )  ^  iflag  )
return (0);

        if (non_subst != NULL) {

          int ix;

#warning FIX THIS
          strncpy(new_string, *non_subst++, MAXLINE+1);

          for(ix = 0 ; ix < no_subst ; ++ix) {
            char length = pmatch[*subexp_no].rm_eo - pmatch[*subexp_no].rm_so;
            char *start = string + pmatch[*subexp_no++].rm_so;

#warning FIX THIS
            strncat(  new_string,  start,  min( length, MAXLINE+1 - strlen(new_string) )  );
#warning FIX THIS
            strncat( new_string, *non_subst++, MAXLINE+1 - strlen(new_string) );
          }

          m_dprintf(MSYSLOG_INFORMATIVE, "om_regex_write: type[%d]: replacing [%s] with [%s]\n", i, string, new_string);

          switch (i) {
            case OM_FILTER_MESSAGE:
              m->msg = new_string;
          break;

            case OM_FILTER_HOST:
              memset(f->f_prevhost, '\0', MAXHOSTNAMELEN);
#warning FIX THIS
              strncpy(f->f_prevhost, new_string, MAXHOSTNAMELEN - 1);
          break;
          }

        }

      }
    }
  }
return (1);
}


int om_regex_flush(struct filed *f, void *ctx) {

  return (1);
}


/* 
 * Free the compiled regex
 */
int om_regex_close(struct filed *f, void *ctx) {

  struct om_regex_ctx *c;

  c = ctx;

  if (c->msg_no_subst) {
    int ix;
    char *string = *(c->msg_non_subst);
    for(ix = c->msg_no_subst ; ix >= 0 ; --ix)
      free(string++);
    free(c->msg_non_subst);
  }
  if (c->msg_subexp_no) free(c->msg_subexp_no);

#warning FIX THIS
  if (c->host_non_subst) {
#if 0
    int ix;
    char *string;
    for(ix = c->host_no_subst ; ix >= 0 ; --ix)
      free(string);
#endif
    free(c->host_non_subst);
  }
  if (c->host_subexp_no) free(c->host_subexp_no);

  return (0);
}

