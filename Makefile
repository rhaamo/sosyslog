TOP=	.

SUBDIR=	src/peo		\
	src/modules

#SUBDIR=	src/peo		\
#	src/modules	\
#	src		\
#	src/man

MAN3=	${MANPAGES3}
MAN5=	${MANPAGES5}
MAN8=	${MANPAGES8}

all:		all-subdir
install:	install-subdir
deinstall:	deinstall-subdir
clean:		clean-subdir
cleandir:	cleandir-subdir
depend:		depend-subdir

re-configure:
	rm configure
	cat configure.in | mkconfigure > configure
	chmod 755 configure

include ${TOP}/mk/build.subdir.mk
