/* Copyright (c) 1998-2002 Netarx Inc. All rights reserved. */
/*
 * Copyright (c) 2001, Netarx Inc., USA
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
 *  om_directory -- 
 *    write a message to a directory being used as a queue
 *
 * Author: Fred Eisele for Netarx Inc., USA
 *
 */

/* Get system information */
#include "../config.h"

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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>

#define MAXKEY 32
#define MAXDIRECTORY 256
#define MAXELEMENT 32
#define MAXMSG 2048

#define DATA_PRESENT_LOCK 0

/*
  This module makes use of a semaphore-set having three semaphores

    DATA_PRESENT_LOCK: flag indicating that there is data present in the queue

  The principle semaphore is DATA PRESENT, it acts as a flag indicating
    that there are items in the queue. 
*/

#define QUEUE_LOCKED 0
#define QUEUE_LOCK -1
#define QUEUE_UNLOCKED 1

struct om_directory_context {
  int directory_len;
  char* directory;  /* the directory path */
  int semaphore;
};

/*  
  WRITE -- Write to the directory queue
   return:
       1  successfull
       0  stop logging it (used by filters)
      -1  error logging (but let other modules process it)
*/

int
om_directory_write(
  /* Current filed struct   */ struct filed* fil,
  /* Flags for this message */ int flags,
  /* The message string     */ struct m_msg* msg,
  /* Our context            */ void* context) {

  struct om_directory_context *ctx;
  char* filename = NULL;
  int filename_len = 0;
  int filedes = -1;
  time_t timer = 0;
  time_t time_rc = 0;
  int ix = 0;

  m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_write: Entering\n");

  /* always check, just in case ;) */
  if (msg == NULL || msg->msg == NULL || !strcmp(msg->msg, "")) {
    m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_write: no message!");
return (-1);
  }

  ctx = (struct om_directory_context *) context;

/* 
  The file is created write only, (00200), so that another process
   will not inadvertently be inclined to examine it 
   until the writing is done. (mkstemp does not work (drat))
*/
  filename_len = ctx->directory_len + sizeof("/1234567890.1234567890");
  filename = (char*) malloc( filename_len+1 ); 

  time_rc = time( &timer );
  for( ix=0; filedes < 0; ix++ ) {
    snprintf( filename, filename_len, "%s/%010d.%010d",
              ctx->directory, (int) timer, ix );
    filedes = open( filename, O_WRONLY|O_CREAT|O_EXCL, S_IWUSR );
  if (ix > 26) {
    m_dprintf(MSYSLOG_SERIOUS, "om_directory_write: cannot open file\n");
return (-1);
    }
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_write: "
         "filename (%s)\n", filename);

  /* write to the file */
  write( filedes, msg->msg, strnlen(msg->msg, MAXMSG) ); 
  close( filedes );

  /* unlock the file by changing its permission to allow reading */
  chmod( filename, S_IRUSR|S_IRGRP );

  /* semaphore updated to indicate a new message has been added */
  semctl( ctx->semaphore, DATA_PRESENT_LOCK, SETVAL, QUEUE_UNLOCKED );
  m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_write: semaphore update\n");

return (1);
}


/*  
  INIT -- Initialize om_directory
   return:
       1  OK
      -1  something went wrong
*/

int
om_directory_init (
/* Argumemt count */                 int argc,
/* Argumemt array, like main() */    char* *argv,
/*  Our filed structure */           struct filed* f,
/*  Program name doing this log */   char* prog,
/* Our context */                    void* *context,
/* status */                         char* *status)  {

  char ch;
  struct om_directory_context *ctx;
  int directory_stat_rc;
  struct stat directory_stat;
  key_t semkey = 0;
/*  unsigned short int seminit[] = {0, 1, 0}; */
  int ix = 0;
  char statbuf[1024];

  if ((*context = (void *) calloc(1, sizeof(struct om_directory_context))) == NULL) {
    snprintf(statbuf, sizeof(statbuf), "om_directory: " "cannot allocate context");
    m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
    *status = strdup(statbuf);
return (-1);
  }

  ctx = (struct om_directory_context *) *context;

  ctx->directory_len = 0;
  ctx->directory = NULL;

  ctx->semaphore = 0;

  /* for debugging purposes */
  m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: Entering\n");

  /* Parse your options with getopt(3) */

   optind = 1;
#ifdef HAVE_OPTRESET 
  optreset = 1;
#endif
/*
  SUMMARY
   %directory -s <semaphore> 
*/
  while ((ch = getopt(argc, argv, "s:")) != -1) {
    switch (ch) { 
      case 's':  /* semaphore (a directory path) */
        ctx->directory_len = strnlen(optarg, MAXDIRECTORY);
        ctx->directory = (char*) malloc( ctx->directory_len+1 );
        strncpy( ctx->directory, optarg, ctx->directory_len );
        ctx->directory[ctx->directory_len] = '\0';
        break;
      default :
        snprintf(statbuf, sizeof(statbuf), "om_directory: "
            "error on arguments (%c)", ch);
        m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
        *status = strdup(statbuf);
return (-1);
    }
  }

  /* 
    I presume that the directory queue has been generated separately.
    The associated semaphore may not exist in which 
    case it will be created.
    I presume that they will be removed separately.
    See make-directory-queue.pl for an example.
  */
  directory_stat_rc = stat( ctx->directory, &directory_stat );
  if (directory_stat_rc && (! S_ISDIR( directory_stat.st_mode ))) {
      snprintf(statbuf, sizeof(statbuf), "om_directory: "
            "directory does not exist");
      m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
      *status = strdup(statbuf);
return (-1);
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: "
            "directory (%s) exists\n", ctx->directory);

  semkey = ftok( ctx->directory, '\0' );  
  if (semkey < 0) {
    snprintf(statbuf, sizeof(statbuf), "om_directory: "
            "semaphore key for directory (%s) could not be generated", ctx->directory);
    m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
    *status = strdup(statbuf);
return(-1);
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: "
            "semaphore key (0x%x) created\n", semkey);

  /* determine if the semaphore set already exists */
  /* if it does not then create one */
  /* otherwise use the one that exists */
  errno = 0;
  for( ix=0 ;; ix++) {
    switch (errno) {
    case 0:
    case EEXIST:
      ctx->semaphore = semget( semkey, 1, 0 );  
      m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: "
            "errno(%d) attempt to get an existing semaphore\n", errno);
    break;
    case ENOENT:
      ctx->semaphore = semget( semkey, 1, IPC_CREAT | IPC_EXCL | S_IRWXU | S_IRWXG );  
      m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: "
            "errno(%d) attempting to create an semaphore\n", errno);
    break;
    default:
      m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: "
            "errno(%d) attempting to create an semaphore\n", errno);
    }
  if (ctx->semaphore >= 0) break;
    if (ix > 5) {
      snprintf(statbuf, sizeof(statbuf), "om_directory: "
            "semaphore set for key (0x%x) NOT created", semkey);
      m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
      *status = strdup(statbuf);
return (-1);
    }
    m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: "
            "semaphore set for key (0x%x) NOT obtained, trying again\n", semkey);
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_init: "
            "semaphore set (%d) obtained\n", ctx->semaphore);

  snprintf(statbuf, sizeof(statbuf), "om_directory_init: Leaving ok");
  m_dprintf(MSYSLOG_INFORMATIVE, "%s\n", statbuf);
  *status = strdup(statbuf);
return (1);
}

/*
 *   CLOSE:
 *   return:
 *       1  OK
 *      -1  BAD
 */
int
om_directory_close (struct filed *f, void *context) {
   /*  struct om_directory_context *ctx = (struct om_directory_context *) context; */
   m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_close: Not implemented\n");
return (1);
}

/*
 *   FLUSH:
 *   return:
 *      1  OK
 *     -1  BAD
 */
int
om_directory_flush (struct filed *f, void *context) {
   /*  struct om_directory_context *ctx = (struct om_directory_context *) context; */
   m_dprintf(MSYSLOG_INFORMATIVE, "om_directory_flush: Not implemented\n");
return (1);
}

