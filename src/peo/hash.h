/*      $Id: hash.h,v 1.10 2000/05/06 00:58:22 claudio Exp $
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

extern int		 mac (int, const char*, int, const char*, int, char*);
extern int		 mac2 (const char*, int, const char*, int, char*);
extern int		 gethash (const char*);
extern char		*strdot (char*);
extern char		*strallocat (const char*, const char*);
extern char		*strmac (const char*);
extern char		*strkey0 (const char*);
extern unsigned char	*asc2bin (unsigned char*, const unsigned char*);
extern unsigned char	*bin2asc (unsigned char*, const unsigned char*, int);
extern int		 getrandom (char*, int);

#endif

