TOP=	.

SUBDIR=	src

#SUBDIR=	src/peo		\
#	src/modules	\
#	src		\
#	src/man

all:		all-subdir
install:	install-subdir
deinstall:	deinstall-subdir
clean:		clean-subdir
cleandir:	cleandir-subdir
depend:		depend-subdir

include ${TOP}/mk/build.subdir.mk
