/* $Copyright (c) 1998-2002 Netarx Inc. All rights reserved. $ */
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
 *  om_queue -- 
 *    write a message to a directory being used as a queue
 *
 * Author: Fred Eisele for Netarx Inc., USA
 *
 */

/* Get system information */
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
#define MAXTARGET 128
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

struct om_queue_element_node {
  int element_len;
  char type;  /* header(h) or footer(f) */
  char* element;  /* the element */
  char* attribute;  /* pointer to the attributes of the element */
  char* payload;  /* pointer to the payload (in the element) */
  struct om_queue_element_node* element_node;    /* the next element's node */
};

struct om_queue_context {
  int key_len;
  char* key;  /* the key for the match */

  int target_len;
  char* target;  /* the directory path */

  int directory_len;
  char* directory;  /* the directory path */

  char outtype;  /* only the first character is used */
  int semaphore;

  int namespace_len;
  char* namespace;  /* the directory path */

  struct om_queue_element_node *header_node;    /* the first header's node */
  struct om_queue_element_node *footer_node;    /* the first footer's node */
};


/*  
  WRITE -- Write to the directory queue
   return:
       1  successfull
       0  stop logging it (used by filters)
      -1  error logging (but let other modules process it)
*/

int
om_queue_write(
  /* Current filed struct   */ struct filed* fil,
  /* Flags for this message */ int flags,
  /* The message string     */ struct m_msg* msg,
  /* Our context            */ void* context) {

  struct om_queue_context *ctx;
  struct om_queue_element_node *node;
  char* filename = NULL;
  int filename_len = 0;
  int filedes = -1;
  FILE* filehandle = NULL;
  time_t timer = 0;
  time_t time_rc = 0;
  int ix = 0;
  msg->fired++;

  m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_write: Entering\n");

  /* always check, just in case ;) */
  if (msg == NULL || msg->msg == NULL || !strcmp(msg->msg, "")) {
    m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_write: no message!");
return (-1);
  }

  ctx = (struct om_queue_context *) context;

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
    m_dprintf(MSYSLOG_SERIOUS, "om_queue_write: cannot open file\n");
return (-1);
    }
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_write: "
         "filename (%s)\n", filename);

  /* XML */
    filehandle = fdopen( filedes, "w" );
    if (! filehandle) {
      m_dprintf(MSYSLOG_SERIOUS, "om_queue_write: filehandle not created\n");
return (-1);
    }
    /* the target is separated from the payload with the RS(record separator) character */
    fprintf( filehandle, "send\n%s\x1e", ctx->target);

    fprintf( filehandle, "<?xml version=\"1.0\"?>");
    m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_write: xml header\n");

    fprintf( filehandle, "\n<ticket xmlns=\"%s\">", 
        (ctx->namespace ? ctx->namespace : "urn:Netarx:Sensor/Basic") );

    for( node = ctx->header_node; node != 0; node = node->element_node ) {
    if (! node->element) continue;
      fprintf( filehandle, 
          ((*node->payload == '\0') ? "\n<%s%s%s/>" : "\n<%s%s%s>%s</%s>"), 
          node->element, (*node->attribute == '\0' ? "":" "), node->attribute,
          node->payload, node->element );
    } 
    fprintf( filehandle, "\n<patient>%s</patient>", fil->f_prevhost );

    fprintf( filehandle, "\n<timestamp>%d</timestamp>", (int)timer );

    if (ctx->key) fprintf( filehandle, "\n<key>%s</key>", ctx->key );

    fprintf( filehandle, "\n<message facility=\"%d\" priority=\"%d\">%s</message>",
                      msg->fac, msg->pri, msg->msg );

    for( node = ctx->footer_node; node != 0; node = node->element_node ) {
    if (! node->element) continue;
      fprintf( filehandle, 
          ((*node->payload == '\0') ? "\n<%s%s%s/>" : "\n<%s%s%s>%s</%s>"), 
          node->element, (*node->attribute == '\0' ? "":" "), node->attribute,
          node->payload, node->element );
    } 

    { fprintf( filehandle, "\n</ticket>\n" ); }
  fclose( filehandle );

  /* unlock the file by changing its permission to allow reading */
  chmod( filename, S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH );

  /* semaphore updated to indicate a new message has been added */
  semctl( ctx->semaphore, DATA_PRESENT_LOCK, SETVAL, QUEUE_UNLOCKED );
  m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_write: semaphore update\n");

return (1);
}


/*  
  INIT -- Initialize om_queue
   return:
       1  OK
      -1  something went wrong
*/

int
om_queue_init (
    int argc,              /* Argumemt count */
    char* *argv,           /* Argumemt array, like main() */
    struct filed* f,       /*  Our filed structure */ 
    struct global* global, /* Global variable */  
    void* *context,        /* Our context */     
    char* *status)         /* status */     
{

  char* mark;
  char ch;
  struct om_queue_context *ctx;
  int directory_stat_rc;
  struct stat directory_stat;
  key_t semkey = 0;
  struct om_queue_element_node *node = 0;
/*  unsigned short int seminit[] = {0, 1, 0}; */
  int ix = 0;
  char statbuf[1024];

  if ((*context = (void *) calloc(1, sizeof(struct om_queue_context))) == NULL) {
    snprintf(statbuf, sizeof(statbuf), "om_queue: " "cannot allocate context");
    m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
    *status = strdup(statbuf);
return (-1);
  }

  ctx = (struct om_queue_context *) *context;
  ctx->key_len = 0;
  ctx->key = NULL;

  ctx->target_len = 0;
  ctx->target = NULL;

  ctx->directory_len = 0;
  ctx->directory = NULL;

  ctx->semaphore = 0;
  ctx->outtype = 'r';

  ctx->namespace_len = 0;
  ctx->namespace = NULL;

  ctx->header_node = NULL;
  ctx->footer_node = NULL;

  /* for debugging purposes */
  m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: Entering\n");

  /* Parse your options with getopt(3) */

   optind = 1;
#ifdef HAVE_OPTRESET 
  optreset = 1;
#endif
/* SUMMARY
 queue -o (xml|raw) -d <semaphore> -k <key> [-n <namespace>] [-h <header)]* [-f <footer)]*
*/
  while ((ch = getopt(argc, argv, "o:t:d:k:n:h:f:")) != -1) {
    switch (ch) { 
      case 'o':  /* output type [xml,text] */
        ctx->outtype = optarg[0];
        m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "output type (%s) specified\n", ((ctx->outtype == 'x') ? "xml":"raw"));
        break;
      case 't':  /* target  (where you want the ticket to go) */
        if ((ctx->target_len = strlen(optarg)) > MAXTARGET)
        	ctx->target_len = MAXTARGET;
        ctx->target = (char*) malloc( ctx->target_len+1 );
        strncpy( ctx->target, optarg, ctx->target_len );
        ctx->target[ctx->target_len] = '\0';
        break;
      case 'd':  /* semaphore (a directory path) */
        if ((ctx->directory_len = strlen(optarg)) > MAXDIRECTORY)
        	ctx->directory_len = MAXDIRECTORY;
        ctx->directory = (char*) malloc( ctx->directory_len+1 );
        strncpy( ctx->directory, optarg, ctx->directory_len );
        ctx->directory[ctx->directory_len] = '\0';
        break;
      case 'k':  /* key for the message */
        if ((ctx->key_len = strlen(optarg)) > MAXKEY)
        	ctx->key_len = MAXKEY;
        ctx->key = (char*) malloc( ctx->key_len+1 );
        strncpy( ctx->key, optarg, ctx->key_len );
        ctx->key[ctx->key_len] = '\0';
        m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "key (%s) specified\n", ctx->key);
        break;
      case 'n':  /* namespace for the message */
        if ((ctx->namespace_len = strlen(optarg)) > MAXELEMENT)
	        ctx->namespace_len = MAXELEMENT;
        ctx->namespace = (char*) malloc( ctx->namespace_len+1 );
        strncpy( ctx->namespace, optarg, ctx->namespace_len );
        ctx->namespace[ctx->namespace_len] = '\0';
        m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "namespace (%s) specified\n", ctx->key);
        break;
      case 'h':  /* header element */
      case 'f':  /* footer element */
        /* add a new node ... */
        node = (struct om_queue_element_node*) 
           calloc( 1, sizeof( struct om_queue_element_node) );

        node->type = ch;
        node->payload = NULL;
        if ((node->element_len = strlen(optarg)) > MAXELEMENT)
        	node->element_len = MAXELEMENT;
        node->element = (char*) malloc( node->element_len+1 );
        strncpy( node->element, optarg, node->element_len );
        node->element[node->element_len] = '\0';

        mark = node->element;

        node->attribute = strpbrk( mark, ":" );
        if (node->attribute == NULL) {
          node->attribute = "\0";
        } else {
          *(node->attribute) = '\0';
          node->attribute++;
          mark = node->attribute;
        }

        node->payload = strpbrk( mark, ":" );
        if (node->payload == NULL) {
          node->payload = "\0";
        } else {
          *(node->payload) = '\0';
          node->payload++;
          mark = node->payload;
        }

        m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "element (%s:%s) specified\n", node->element, node->payload);

        /* ... to the beginning of the element node list */
        switch (ch) { 
        case 'h':  /* header element */
          node->element_node = ctx->header_node;
          ctx->header_node = node;
          break;
        case 'f':  /* footer element */
          node->element_node = ctx->footer_node;
          ctx->footer_node = node;
          break;
        }
        break;
      default :
        snprintf(statbuf, sizeof(statbuf), "om_queue: "
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
      snprintf(statbuf, sizeof(statbuf), "om_queue: "
            "directory does not exist");
      m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
      *status = strdup(statbuf);
return (-1);
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "directory (%s) exists\n", ctx->directory);

  semkey = ftok( ctx->directory, '\0' );  
  if (semkey < 0) {
    snprintf(statbuf, sizeof(statbuf), "om_queue: "
            "semaphore key for directory (%s) could not be generated", ctx->directory);
    m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
    *status = strdup(statbuf);
return(-1);
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
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
      m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "errno(%d) attempt to get an existing semaphore\n", errno);
    break;
    case ENOENT:
      ctx->semaphore = semget( semkey, 1, IPC_CREAT | IPC_EXCL | S_IRWXU | S_IRWXG );  
      m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "errno(%d) attempting to create an semaphore\n", errno);
    break;
    default:
      m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "errno(%d) attempting to create an semaphore\n", errno);
    }
  if (ctx->semaphore >= 0) break;
    if (ix > 5) {
      snprintf(statbuf, sizeof(statbuf), "om_queue: "
            "semaphore set for key (0x%x) NOT created", semkey);
      m_dprintf(MSYSLOG_SERIOUS, "%s\n", statbuf);
      *status = strdup(statbuf);
return (-1);
    }
    m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "semaphore set for key (0x%x) NOT obtained, trying again\n", semkey);
  }
  m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_init: "
            "semaphore set (%d) obtained\n", ctx->semaphore);

  snprintf(statbuf, sizeof(statbuf), "om_queue_init: Leaving ok");
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
om_queue_close (struct filed *f, void *context) {
   /*  struct om_queue_context *ctx = (struct om_queue_context *) context; */
   m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_close: Not implemented\n");
return (1);
}

/*
 *   FLUSH:
 *   return:
 *      1  OK
 *     -1  BAD
 */
int
om_queue_flush (struct filed *f, void *context) {
   /*  struct om_queue_context *ctx = (struct om_queue_context *) context; */
   m_dprintf(MSYSLOG_INFORMATIVE, "om_queue_flush: Not implemented\n");
return (1);
}

