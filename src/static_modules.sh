#!/bin/sh
#
#  Read modules configuration file (standard input)
#  and create apropiates .c and .h files
#

STATIC_MODULES=static_modules


FIRST_ADD_OUTPUT="yes"
function add_output() 
{
printf "\
extern int om_%s_init (int, char**, struct filed*, char*, struct om_hdr_ctx**);\n\
extern int om_%s_doLog (struct filed*, int, char*, struct om_hdr_ctx*);\n\
extern int om_%s_flush (struct filed*, struct om_hdr_ctx*);\n\
extern int om_%s_close (struct filed*, struct om_hdr_ctx*);\n" $NAME $NAME $NAME $NAME >> $STATIC_MODULES.h

printf "\
\tif ( (om = (struct omodule*)calloc(1, sizeof(struct omodule))) == NULL)\n\
\t\treturn(-1);\n" >> $STATIC_MODULES.c

if [ $FIRST_ADD_OUTPUT ]; then
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
\tolast->om_close=om_%s_close;\n\n" $NAME $NAME $NAME $NAME $NAME>> $STATIC_MODULES.c
}

FIRST_ADD_INPUT="yes"
function add_input()
{
printf "\
extern int im_%s_init (struct i_module*, char**, int);\n\
extern int im_%s_getLog (struct i_module*, struct im_msg*);\n\
extern int im_%s_close (struct i_module*);\n" $NAME $NAME $NAME >> $STATIC_MODULES.h

printf "\
\tif ( (im = (struct imodule*)calloc(1, sizeof(struct imodule))) == NULL)\n\
\t\treturn(-1);\n" >> $STATIC_MODULES.c

if [ $FIRST_ADD_INPUT ]; then
	printf "\tilast = imodules = im;\n" >> $STATIC_MODULES.c
	FIRST_ADD_INPUT=""
else
	printf "\tilast->im_next = im;\n\tilast = im;\n" >> $STATIC_MODULES.c
fi

printf "\
\tilast->im_name=\"%s\";\n\
\tilast->im_init=im_%s_init;\n\
\tilast->im_getLog=im_%s_getLog;\n\
\tilast->im_close=im_%s_close;\n\n" $NAME $NAME $NAME $NAME >> $STATIC_MODULES.c
}


# Create .h and .c static modules files
cp -f $STATIC_MODULES.c.in $STATIC_MODULES.c
printf "\
/*\n\
 * Automatically generated static modules header file\n\
 */\n\n" > $STATIC_MODULES.h

LINE=0
while read NAME TYPE MODE
do
	LINE=`expr $LINE + 1`
	FIRST_CHAR=`test $NAME && expr $NAME : '\(.\)'`
	if [ "$FIRST_CHAR" != "#" ] && [ $NAME ]; then
		if [ "$TYPE" = "output" ] && [ "$MODE" = "static" ]; then
			add_output
			printf "%s " om_$NAME.o

 		elif [ "$TYPE" = "input" ] && [ "$MODE" = "static" ]; then
			add_input
			printf "%s " im_$NAME.o

 		elif ( [ "$TYPE" != "input" ] && [ "$TYPE" != "output" ] ) ||
 		     ( [ "$MODE" != "static" ] && [ "$MODE" != "dynamic" ] ); then
			echo Error reading configuration file on line $LINE: $NAME $TYPE $MODE
			exit -1
		fi
	fi
done

printf "\treturn(0);\n}\n" >> $STATIC_MODULES.c
exit 0

