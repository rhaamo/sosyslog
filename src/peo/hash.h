/*      $Id: hash.h,v 1.3 2000/04/28 17:59:49 claudio Exp $
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

extern int   hash (int, char*, int, char*, char*);
extern int   gethash (char*);
extern char *strkey (char*);

#endif

