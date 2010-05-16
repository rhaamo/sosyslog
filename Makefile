TOP=	.

SUBDIR=	${WITH_PEO_DIR}	\
	src/modules	\
	src		\
	src/man

all:		all-subdir
install:	install-subdir
deinstall:	deinstall-subdir
clean:		clean-subdir
cleandir:	cleandir-subdir
depend:		depend-subdir

gen-include:
	rm -f src/config.h
	touch src/config.h
	@echo "!!!WARNING!!!"
	@echo "ADDING #include \"TODO.h\""
	@echo "!!!WARNING!!!"
	echo "#include \"../TODO.h\"" >> src/config.h
	for i in `find config -type f`; do	\
	    echo "#include \"../$$i\"" >> src/config.h ;	\
	done

reconfigure:
	rm configure
	cat configure.in | mkconfigure > configure
	chmod 755 configure

include ${TOP}/mk/build.subdir.mk
