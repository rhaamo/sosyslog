#	$OpenBSD: Makefile,v 1.3 1997/09/21 11:44:29 alejo Exp $


all: peochk om_peo objs

peochk: objs
	gcc -Wall -ggdb -o peochk peochk.c hash.o rmd160.o

om_peo:
	gcc -Wall -ggdb -c om_peo.c

objs:
	gcc -Wall -ggdb -c hash.c rmd160.c
	

