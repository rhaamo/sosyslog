/*	$Id: options.c,v 1.2 2002/09/17 05:20:26 alejo Exp $	*/
/*
   Copyright (c) 2001
  	Core-SDI S.A., all rights reserved.
  
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:
   1. Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
   3. All advertising materials mentioning features or use of this software
      must display the following acknowledgement:
  	This product includes software developed by CORE-SDI S.A.
  	and contributors.
   4. Neither the name CORE-SDI nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.
  
   THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
   ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
   FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
   DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
   OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
   HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
   LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
   OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
   SUCH DAMAGE.
 */

/*
 * Modular Syslog generic module configuration functions
 *
 * Author: Alejo Sanchez for Core-SDI
 */

#include "config.h"

#include <ctype.h>
#include <string.h>

/*
 * getxopt:
 *	ease command line parsing for modules, similar to getopt(3)
 *	use like this
 *
 *	int cnt = 0;
 *
 *	while ((ch = getxopt(argc, argv, "p!popt: n:nopt! h!help:", &cnt)) != -1) {
 *		switch (ch):
 *		case 'p':
 *			(void)strlcpy(argpval, argv[cnt], sizeof(argpval));
 *			break;
 *		case 'n':
 *			ActivateN++;
 *			break;
 *		case 'h':
 *		default:
 *			usage();
 *			break;
 *		}
 *		cnt++;
 *	}
 */

int
getxopt(int argc, char *argv[], char *opts, int *startarg)
{
	char	*a, *s, *e;	/* argument, start and  end  of option */
	int	argn, len, ch;

	argn = 0;

	/* compare all arguments...*/
	for (s = opts, argn = *startarg; argn < argc; argn++) {

		a = argv[argn];
		if (*a == '-') {
			if (*++a == '-')
				break;
		} else if (!isalnum((int) *(++a)))
			break;

		/* ...with all options */
		while (*s) {

			while (isspace ((int) *s))
				s++;
			if (isalnum((int) *s))
				ch = (int) *s; /* new option */
			else
				break;	/* no more options */

			/* find end of option */
			for (e = s; *e && *e != ':' && !isspace((int) *e); e++);

			do {
				/* find this name's length */
				for (len = 1; s[len] != '!' && &s[len] != e;
				    len++);

				/* match option length and name */
				if (strlen(a) == len &&
				    !strncmp(a, s, (size_t) len)) {

					/* we got a match! */
					*startarg = argn;
					/* needs arg ? */
					if (*e == ':')
						(*startarg)++;
					return (ch);
				}

				if (s[len] == '!')
					len++;	/* start of next name */

				s = &s[len];

			} while (s != e);

			/* avance to next option */
			if (*s != '\0')
				s++;
		}
	}

	return (-1);
}
