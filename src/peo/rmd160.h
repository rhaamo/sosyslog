/*	$OpenBSD: rmd160.h,v 1.3 1998/03/23 12:49:28 janjaap Exp $	*/

/********************************************************************\
 *
 *      FILE:     rmd160.h
 *
 *      CONTENTS: Header file for a sample C-implementation of the
 *                RIPEMD-160 hash-function. 
 *      TARGET:   any computer with an ANSI C compiler
 *
 *      AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *      DATE:     1 March 1996
 *      VERSION:  1.0
 *
 *      Copyright (c) Katholieke Universiteit Leuven
 *      1996, All Rights Reserved
 *
\********************************************************************/

#ifndef  _RMD160_H	/* make sure this file is read only once */
#define  _RMD160_H
#include <sys/cdefs.h>
#include <stdio.h>	/* CCRYPT */

/********************************************************************/

/* structure definitions */

typedef struct {
	u_int32_t state[5];	/* state (ABCDE) */
	u_int32_t length[2];	/* number of bits */
	u_char	bbuffer[64];    /* overflow buffer */
	u_int32_t buflen;	/* number of chars in bbuffer */
} RMD160_CTX;

/********************************************************************/

/* function prototypes */

extern
void RMD160Init (RMD160_CTX *context);

extern
void RMD160Transform (u_int32_t state[5], const u_int32_t block[16]);

extern
void RMD160Update (RMD160_CTX *context, const u_char *data, u_int nbytes);

extern
char *RMD160End (RMD160_CTX *, char *);

#endif  /* _RMD160_H */

/*********************** end of file rmd160.h ***********************/
