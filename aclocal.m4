dnl	$CoreSDI: aclocal.m4,v 1.2 2000/07/08 00:52:50 claudio Exp $

dnl
dnl grep pattern file command
dnl 	Searches for a pattern on a file and executes command if match
dnl
AC_DEFUN(GREP, [
if test -e "$2"
then
	if test "`grep $1 $2`"
	then
		$3
	fi
fi
])


dnl
dnl Configure MYSQL includedir and libdir
dnl if mysql is not installed exits
dnl
AC_DEFUN(MSYSLOG_CHECK_MYSQL, [
	for i in /usr/local/lib /usr/local/lib/mysql /usr/lib /usr/lib/mysql no
	do
		if test "$i" = "no"
		then
			echo "Need mysqlclient library to enable mysql module"
			exit
		elif test "`ls $i | grep mysqlclient`"
		then
			break
		fi
	done
	CPPFLAGS="$CPPFLAGS -L$i"
	AC_CHECK_LIB(mysqlclient, mysql_real_connect,, [
			echo "Need mysqlclient library to enable mysql module"
			exit ])

for i in /usr/local/include/mysql /usr/local/mysql/include /usr/include/mysql no
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
CPPFLAGS="$CPPFLAGS -I$i"
])


dnl
dnl Configure PGSQL includedir and libdir
dnl if pgsql is not installed exits
dnl
AC_DEFUN(CONF_PGSQL, [
printf "\n\n\n>>>>>>> PGSQL Module libraries should be defined <<<<<<\n\n\n"
])


dnl
dnl begin modules configuration
dnl
AC_DEFUN(BEGIN_MODULES_CONF, [
STATIC_MODULES="src/static_modules"
MODULES_DIR="src/modules"
FIRST_ADD_OUTPUT=1
FIRST_ADD_INPUT=1
SMODULES_HEADER=""
SMODULES_LOAD=""

SSRCS=""
unix=""
udp=""
bsd=""
linux=""
classic=""
peo=""
mysql=""
pgsql=""
regex=""
eval `cat ./modules.conf`
])


dnl
dnl end modules configuration
dnl
AC_DEFUN(END_MODULES_CONF, [
AC_SUBST(SMODULES_HEADER)
AC_SUBST(SMODULES_LOAD)
AC_SUBST(SSRCS)
])


dnl
dnl add_om  module_name
dnl	adds a new static output module
dnl
AC_DEFUN(add_om, [ 
SMODULES_HEADER="$SMODULES_HEADER \
int om_$1_init (int, char**, struct filed*, char*, struct om_hdr_ctx**, \
struct sglobals*); \
int om_$1_doLog (struct filed*, int, char*, struct om_hdr_ctx*, \
struct sglobals*);"

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

GREP(om_$1_flush, "$MODULES_DIR/$2", [
SMODULES_HEADER="$SMODULES_HEADER \
	int om_$1_flush (struct filed*, struct om_hdr_ctx*, \
struct sglobals*);" 
SMODULES_LOAD="$SMODULES_LOAD \
	olast->om_flush=om_$1_flush;"
]) 

GREP(om_$1_close, "$MODULES_DIR/$2", [
SMODULES_HEADER="$SMODULES_HEADER \
	int om_$1_close (struct filed*, struct om_hdr_ctx*, struct sglobals*);"
SMODULES_LOAD="$SMODULES_LOAD \
	olast->om_close=om_$1_close;" ])
])


dnl
dnl add_im  module_name 
dnl	adds a new static input module
dnl
AC_DEFUN(add_im, [
SMODULES_HEADER="$SMODULES_HEADER \
int im_$1_init (struct i_module*, char**, int, \
struct sglobals*); \
int im_$1_getLog (struct i_module*, struct im_msg*, \
struct sglobals*);" 

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

GREP(im_$1_close, "$MODULES_DIR/$2", [
SMODULES_HEADER="$SMODULES_HEADER \
	int im_$1_close (struct i_module*, struct sglobals*);"
SMODULES_LOAD="$SMODULES_LOAD \
	ilast->im_close=im_$1_close;" ])
])


dnl
dnl check_module  module_name static|dynamic|whatever srcs_list im|om 
dnl	check and configure a module
dnl
AC_DEFUN(CHECK_MODULE, [
AC_MSG_CHECKING([configuration for $1 module])
if test "x$2" = "xstatic"
then
	SSRCS="$SSRCS $4 $5" 
	add_$3($1, $4)
	AC_MSG_RESULT([static]);
elif test "x$2" = "xdynamic"
then
	D$1_SRC="$4 $5"
	D$1="libmsyslog_$3_$1.so.$MSYSLOG_VERSION"
	AC_SUBST(D$1)
	AC_SUBST(D$1_SRC)
	AC_MSG_RESULT([dynamic]);
else
	AC_MSG_RESULT([not activated])
fi
])


