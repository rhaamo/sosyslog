dnl
dnl Both of this were taken from arla project!!!
dnl
dnl $Id: aclocal.m4,v 1.31 2001/03/10 00:26:05 alejo Exp $
dnl

AC_DEFUN(AC_HAVE_TYPES, [
for i in $1; do
        AC_HAVE_TYPE($i)
done
: << END
changequote(`,')dnl
@@@funcs="$funcs $1"@@@
changequote([,])dnl
END
])

dnl $Id: aclocal.m4,v 1.31 2001/03/10 00:26:05 alejo Exp $
dnl
dnl check for existance of a type

dnl AC_HAVE_TYPE(TYPE,INCLUDES)
AC_DEFUN(AC_HAVE_TYPE, [
AC_REQUIRE([AC_HEADER_STDC])
cv=`echo "$1" | sed 'y%./+- %__p__%'`
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL([ac_cv_type_$cv],
AC_TRY_COMPILE(
[#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#include <stddef.h>
#endif
#define SYSLOG_NAMES
#include <syslog.h>
$2],
[$1 foo;],
eval "ac_cv_type_$cv=yes",
eval "ac_cv_type_$cv=no"))dnl
AC_MSG_RESULT(`eval echo \\$ac_cv_type_$cv`)
if test `eval echo \\$ac_cv_type_$cv` = yes; then
  ac_tr_hdr=HAVE_`echo $1 | sed 'y%abcdefghijklmnopqrstuvwxyz./- %ABCDEFGHIJKLMNOPQRSTUVWXYZ____%'
`
dnl autoheader tricks *sigh*
define(foo,translit($1, [ ], [_]))
: << END
@@@funcs="$funcs foo"@@@
END
undefine([foo])
  AC_DEFINE_UNQUOTED($ac_tr_hdr, 1)
fi
])


dnl AC_CHECK_CODE_TYPE()
AC_DEFUN(AC_CHECK_CODE_TYPE,
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
  AC_DEFINE(HAVE_STRUCT_CODE, 1)
fi
])


dnl This kludge is from AC_CHECK_SIZEOF()
dnl Should be changed to something better.

dnl AC_CHECK_SIZEOF_DEFINE(TYPE [, CROSS-SIZE])
AC_DEFUN(AC_CHECK_SIZEOF_DEFINE,
[changequote(<<, >>)dnl
dnl The name to #define.
define(<<AC_TYPE_NAME>>, translit(sizeof_$1, [a-z *], [A-Z_P]))dnl
dnl The cache variable name.
define(<<AC_CV_NAME>>, translit(ac_cv_sizeof_$1, [ *], [_p]))dnl
changequote([, ])dnl
AC_MSG_CHECKING(size of $1)
AC_CACHE_VAL(AC_CV_NAME,
[AC_TRY_RUN([#include <stdio.h>
#include <sys/param.h>
#include <netdb.h>
main()
{
  FILE *f=fopen("conftestval", "w");
  if (!f) exit(1);
  fprintf(f, "%d\n", $1);
  exit(0);
}], AC_CV_NAME=`cat conftestval`, AC_CV_NAME=0, ifelse([$2], , , AC_CV_NAME=$2))])dnl
AC_MSG_RESULT($AC_CV_NAME)
AC_DEFINE_UNQUOTED(AC_TYPE_NAME, $AC_CV_NAME)
undefine([AC_TYPE_NAME])dnl
undefine([AC_CV_NAME])dnl
])

