dnl	$CoreSDI: aclocal.m4,v 1.11.2.5.2.2.4.1 2000/10/09 22:47:30 alejo Exp $


dnl
dnl MSYSLOG_GREP patt file comm
dnl 	Searches for a pattern on a file and executes command if match
dnl
AC_DEFUN(MSYSLOG_GREP, [
if test -e "$2"
then
	if test "`grep $1 $2`"
	then
		$3
	fi
fi
])


dnl
dnl Check given library install directory
dnl
AC_DEFUN(MSYSLOG_CHECK_MSYSLOG_LIBDIR, [
AC_ARG_WITH(msyslog-libdir,
	[--with-msyslog-libdir=DIR        specify msyslog library dir],
	[MSYSLOG_LIBDIR="$withval"],
	[MSYSLOG_LIBDIR="/usr/local/lib/alat"]
)])


dnl
dnl Configure MYSQL includedir and libdir
dnl if mysql is not installed exits
dnl
AC_DEFUN(MSYSLOG_CHECK_MYSQL, [
AC_ARG_WITH(mysql-lib,
	[--with-mysql-lib=DIR        specify mysql library dir],
	[MYSQL_CPPFLAGS="$MYSQL_CPPFLAGS -L$withval"],
	[
for i in /usr/local/lib /usr/local/lib/mysql /usr/lib/mysql /usr/lib no
do
	if test "$i" = "no"
	then
		echo "Need mysqlclient library to enable mysql module"
		exit
	elif test "`ls $i 2>/dev/null | grep mysqlclient`"
	then
		break
	fi
done
MYSQL_CPPFLAGS="$MYSQL_CPPFLAGS -L$i"
	])

CPPFLAGS_save="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $MYSQL_CPPFLAGS"
AC_CHECK_LIB(mysqlclient, mysql_real_connect, [
		MYSQL_LIBS="-lmysqlclient"
		AC_DEFINE_UNQUOTED(HAVEmysqlclient)
		] , [
		echo "Need mysqlclient library to enable mysql module"
		exit ])
CPPFLAGS="$CPPFLAGS_save"

AC_ARG_WITH(mysql-inc,
	    [--with-mysql-inc=DIR        specify mysql include dir],
	    [MYSQL_CPPFLAGS="$MYSQL_CPPFLAGS -I$withval"],
	    [
for i in /usr/local/include/mysql /usr/local/mysql/include \
	 /usr/local/include /usr/include/mysql /usr/include no
do
	if test -e "$i/mysql.h"
	then
		break
	fi
done
if test "$i" = "no"
then
	echo "Need mysql.h header file to enable mysql output module"
	exit
fi
MYSQL_CPPFLAGS="$MYSQL_CPPFLAGS -I$i"
	    ])
CPPFLAGS="$CPPFLAGS_save"
])


dnl
dnl Configure PGSQL includedir and libdir
dnl if pgsql is not installed exits
dnl
AC_DEFUN(MSYSLOG_CHECK_PGSQL, [
AC_ARG_WITH(pgsql-lib,
	    [--with-pgsql-lib=DIR        specify pgsql library dir],
	    [PGSQL_CPPFLAGS="$PGSQL_CPPFLAGS -L$withval"],
	    [
for i in /usr/local/pgsql/lib /usr/local/lib/pgsql /usr/lib/pgsql /usr/lib no
do
	if test "$i" = "no"
	then
		echo "Need pq library to enable pgsql module"
		exit
	elif test "`ls $i 2>/dev/null | grep libpq`"
	then
		break
	fi
done
PGSQL_CPPFLAGS="$PGSQL_CPPFLAGS -L$i"
	    ])

CPPFLAGS_save="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $PGSQL_CPPFLAGS"
AC_CHECK_LIB(pq, PQsetdbLogin, [
		PGSQL_LIBS="-lpq"
		AC_DEFINE_UNQUOTED(HAVEpq)
		] , [
		echo "Need pq library to enable pgsql module"
		exit ])

AC_ARG_WITH(pgsql-inc,
	    [--with-pgsql-inc=DIR        specify pgsql include dir],
	    [PGSQL_CPPFLAGS="$PGSQL_CPPFLAGS -I$withval"],
	    [
for i in /usr/local/pgsql/include /usr/local/include/pgsql \
	 /usr/local/include /usr/include/pgsql /usr/include no
do
	if test -e "$i/libpq-fe.h"
	then
		break
	fi
done
if test "$i" = "no"
then
	echo "Need libpq-fe.h header file to enable pgsql output module"
	exit
fi
PGSQL_CPPFLAGS="$PGSQL_CPPFLAGS -I$i"
	    ])
CPPFLAGS="$CPPFLAGS_save"
])


dnl
dnl Begin modules configuration
dnl
AC_DEFUN(MSYSLOG_BEGIN_MODULES_CONF, [
STATIC_MODULES="src/static_modules"
MODULES_DIR="src/modules"
FIRST_ADD_OUTPUT=1
FIRST_ADD_INPUT=1
SMODULES_HEADER=""
SMODULES_LOAD=""

SSRCS=""
UNIX=""
UDP=""
BSD=""
LINUX=""
CLASSIC=""
PEO=""
MYSQL=""
PGSQL=""
REGEX=""
eval `cat ./modules.conf`
])


dnl
dnl End modules configuration
dnl
AC_DEFUN(MSYSLOG_END_MODULES_CONF, [
AC_SUBST(SMODULES_HEADER)
AC_SUBST(SMODULES_LOAD)
AC_SUBST(SSRCS)
])


dnl
dnl MSYSLOG_ADD_om module_name  main_src
dnl	adds a new static output module
dnl
AC_DEFUN(MSYSLOG_ADD_om, [ 
SMODULES_HEADER="$SMODULES_HEADER \
int om_$1_init (int, char**, struct filed*, char*, void **); \
int om_$1_doLog (struct filed*, int, char*, void *);"

SMODULES_LOAD="$SMODULES_LOAD \
if ( (om = (struct omodule*)calloc(1, sizeof(struct omodule))) == NULL) \
	return(-1);"

if test "$FIRST_ADD_OUTPUT"
then
	SMODULES_LOAD="$SMODULES_LOAD \
		olast = omodules = om;"
        FIRST_ADD_OUTPUT=""
else
        SMODULES_LOAD="$SMODULES_LOAD \
		olast->om_next = om; \
		olast = om;"
fi

SMODULES_LOAD="$SMODULES_LOAD \
	olast->om_name=\"$1\"; \
	olast->om_init=om_$1_init; \
	olast->om_doLog=om_$1_doLog;"

MSYSLOG_GREP(om_$1_flush, "$MODULES_DIR/$2", [
SMODULES_HEADER="$SMODULES_HEADER \
	int om_$1_flush (struct filed*, void *);" 
SMODULES_LOAD="$SMODULES_LOAD \
	olast->om_flush=om_$1_flush;"
]) 

MSYSLOG_GREP(om_$1_close, "$MODULES_DIR/$2", [
SMODULES_HEADER="$SMODULES_HEADER \
	int om_$1_close (struct filed*, void *);"
SMODULES_LOAD="$SMODULES_LOAD \
	olast->om_close=om_$1_close;" ])
])


dnl
dnl MSYSLOG_ADD_im  module_name  main_src
dnl	adds a new static input module
dnl
AC_DEFUN(MSYSLOG_ADD_im, [
SMODULES_HEADER="$SMODULES_HEADER \
int im_$1_init (struct i_module*, char**, int); \
int im_$1_getLog (struct i_module*, struct im_msg*);"

SMODULES_LOAD="$SMODULES_LOAD \
if ( (im = (struct imodule*)calloc(1, sizeof(struct imodule))) == NULL) \
	return(-1);"

if test "$FIRST_ADD_INPUT"
then
        SMODULES_LOAD="$SMODULES_LOAD \
	ilast = imodules = im;"
        FIRST_ADD_INPUT=""
else
	SMODULES_LOAD="$SMODULES_LOAD \
        ilast->im_next = im; \
	ilast = im;"
fi

SMODULES_LOAD="$SMODULES_LOAD \
	ilast->im_name=\"$1\"; \
	ilast->im_init=im_$1_init; \
	ilast->im_getLog=im_$1_getLog;"

MSYSLOG_GREP(im_$1_close, "$MODULES_DIR/$2", [
SMODULES_HEADER="$SMODULES_HEADER \
	int im_$1_close (struct i_module*);"
SMODULES_LOAD="$SMODULES_LOAD \
	ilast->im_close=im_$1_close;" ])

MSYSLOG_GREP(im_$1_timer, "$MODULES_DIR/$2", [
SMODULES_HEADER="$SMODULES_HEADER \
	int im_$1_timer (struct i_module*, struct im_msg*);"
SMODULES_LOAD="$SMODULES_LOAD \
	ilast->im_timer=im_$1_timer;" ])
])


dnl
dnl MSYSLOG_CHECK_MODULE module_name static|dynamic|whatever im|om main_src srcs
dnl	check and configure a module
dnl
AC_DEFUN(MSYSLOG_CHECK_MODULE, [
AC_MSG_CHECKING([configuration for $1 module])
if test "x$2" = "xstatic"
then
	SSRCS="$SSRCS $4 $5" 
	MSYSLOG_ADD_$3($1, $4)
	AC_MSG_RESULT([static]);
elif test "x$2" = "xdynamic"
then
	D$1_SRCS="$4 $5"
	D$1="libmsyslog_$3_$1.so.$MSYSLOG_VERSION"
	D$1_PLAIN="libmsyslog_$3_$1.so"
	AC_SUBST(D$1)
	AC_SUBST(D$1_PLAIN)
	AC_SUBST(D$1_SRCS)
	AC_MSG_RESULT([dynamic]);
else
	AC_MSG_RESULT([not activated])
fi
])


dnl ### Checking for typedefs on some header file


dnl AC_CHECK_CODE_TYPE()
AC_DEFUN(AC_CHECK_TYPE_HDR,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for CODE)
AC_CACHE_VAL(ac_cv_type_CODE,
[AC_EGREP_CPP(dnl
changequote(<<,>>)dnl
<<(^|[^a-zA-Z_0-9])$1[^a-zA-Z_0-9]>>dnl
changequote([,]), [#define SYSLOG_NAMES
#include <sys/syslog.h>
], ac_cv_type_CODE=yes, ac_cv_type_CODE=no)])dnl
AC_MSG_RESULT($ac_cv_type_CODE)
if test $ac_cv_type_CODE = yes; then
  AC_DEFINE(HAVE_CODE_TYPEDEF, 1)
fi
])


