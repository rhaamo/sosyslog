/*      $Id: hash.h,v 1.6 2000/05/04 00:39:17 claudio Exp $
 *
 * hash -- some things used by both peo output module and peochk
 *
 * Author: Claudio Castiglia for Core-SDI SA
 *
 */

#ifndef HASH_H
#define HASH_H

enum {
	MD5,
	RMD160,
	SHA1,

	LAST_HASH
};

extern char *default_keyfile;

extern int   mac (int, const char*, int, const char*, int, char*);
extern int   gethash (const char*);
extern char *strkey (const char*);
extern int   mac2 (const char*, int, const char*, int, char*);
extern char *strmac (const char*);

#endif

