#!/bin/sh
#	$CoreSDI: modules_conf.sh,v 1.1 2000/06/26 23:32:10 claudio Exp $


########################
# MODULE FUNCTIONS
########################
function unix
{
SRCS="modules/im_unix.c"
TYPE="input"
}

function bsd
{
SRCS="modules/im_bsd.c"
TYPE="input"
}

function linux
{
SRCS="modules/im_linux.c"
TYPE="input"
}

function udp
{
SRCS="modules/im_udp.c"
TYPE="input"
}

function classic
{
SRCS="modules/om_classic.c"
TYPE="output"
}

function peo
{
SRCS="modules/om_peo.c"
EXTRA="peo/hash.c"
if test `uname` != "OpenBSD"
then
	EXTRA="$EXTRA peo/md5c.c peo/rmd160.c peo/sha1.c"
fi
TYPE="output"
}

function regex
{
SRCS="modules/om_regex.c"
TYPE="output"
}

function mysql
{
SRCS="modules/mysql.c"
EXTRA="modules/sql_misc.c"
TYPE="output"
}

function pgsql
{
SRCS="modules/pgsql.c"
EXTRA="modules/sql_misc.c"
TYPE="output"
}


########################
# INTERNAL FUNCTIONS
########################
FIRST_ADD_OUTPUT=yes
function add_output
{
printf "\
extern int om_%s_init (int, char**, struct filed*, char*,\
struct om_hdr_ctx**);\n\
extern int om_%s_doLog (struct filed*, int, char*, struct om_hdr_ctx*);\n\
extern int om_%s_flush (struct filed*, struct om_hdr_ctx*);\n\
extern int om_%s_close (struct filed*, struct om_hdr_ctx*);\n" \
$NAME $NAME $NAME $NAME >> $STATIC_MODULES.h

printf "\
\tif ( (om = (struct omodule*)calloc(1, sizeof(struct omodule))) == NULL)\n\
\t\treturn(-1);\n" >> $STATIC_MODULES.c

if test "$FIRST_ADD_OUTPUT"
then
	printf "\tolast = omodules = om;\n" >> $STATIC_MODULES.c
	FIRST_ADD_OUTPUT=""
else
	printf "\tolast->om_next = om;\n\tolast = om;\n" >> $STATIC_MODULES.c
fi

printf "\
\tolast->om_name=\"%s\";\n\
\tolast->om_init=om_%s_init;\n\
\tolast->om_doLog=om_%s_doLog;\n\
\tolast->om_flush=om_%s_flush;\n\
\tolast->om_close=om_%s_close;\n\n" \
$NAME $NAME $NAME $NAME $NAME >> $STATIC_MODULES.c
}

FIRST_ADD_INPUT=yes
function add_input
{
printf "\
extern int im_%s_init (struct i_module*, char**, int);\n\
extern int im_%s_getLog (struct i_module*, struct im_msg*);\n\
extern int im_%s_close (struct i_module*);\n" \
$NAME $NAME $NAME >> $STATIC_MODULES.h

printf "\
\tif ( (im = (struct imodule*)calloc(1, sizeof(struct imodule))) == NULL)\n\
\t\treturn(-1);\n" >> $STATIC_MODULES.c

if test "$FIRST_ADD_INPUT"
then
	printf "\tilast = imodules = im;\n" >> $STATIC_MODULES.c
	FIRST_ADD_INPUT=""
else
	printf "\tilast->im_next = im;\n\tilast = im;\n" >> $STATIC_MODULES.c
fi

printf "\
\tilast->im_name=\"%s\";\n\
\tilast->im_init=im_%s_init;\n\
\tilast->im_getLog=im_%s_getLog;\n\
\tilast->im_close=im_%s_close;\n\n" \
$NAME $NAME $NAME $NAME >> $STATIC_MODULES.c
}


########################
#	MAIN 
########################
STATIC_MODULES=src/static_modules
OUTPUT=""

cp -f $STATIC_MODULES.c.in $STATIC_MODULES.c
printf "\
/*\n\
 * Automatically generated static modules header file\n\
 */\n\n" > $STATIC_MODULES.h

while read NAME TYPE MODE
do
	FIRST_CHAR=`test $NAME && expr $NAME : '\(.\)'`
	if test "$FIRST_CHAR" != "#" -a "$FIRST_CHAR"
	then
		if test "x$MODE" = "xstatic" -o "x$MODE" = "xdynamic"
		then
			OUTPUT="$OUTPUT $NAME=$MODE;"
			if test "x$MODE" = "xstatic"
			then
				if test "$TYPE" = "input"
				then
					add_input
				else
					add_output
				fi
			fi
		fi
	fi
done

echo "$OUTPUT"
printf "\treturn(0);\n}\n" >> $STATIC_MODULES.c
exit 0

