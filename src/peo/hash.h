/*      $Id: hash.h,v 1.2 2000/04/27 00:50:46 claudio Exp $
 * hash -- some things used by both peo output module and peochk
 */

#ifndef HASH_H
#define HASH_H

enum {
	MD5,
	RMD160,
	SHA1
};

extern char *default_keyfile;

extern int   hash (int, char*, int, char*, char*);
extern char *strkey (char*);

#endif

