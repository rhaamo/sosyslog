/*      $Id: hash.h,v 1.4 2000/05/03 22:00:53 claudio Exp $
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

extern char *mac (int, const unsigned char*, int, const unsigned char*, int, int*);
extern int   gethash (char*);
extern char *strkey (const char*);
extern int   mac2 (char*, int, char*, char*);
extern char *strmac (const char*);

#endif

