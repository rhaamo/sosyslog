#!/bin/sh
#
#  Read modules configuration file (standard input)
#  and create apropiates .c and .h files
#

STATIC_MODULES=static_modules

# Create .h and .c static modules files
cp -f $STATIC_MODULES.c.in $STATIC_MODULES.c
printf "\
/*\n\
 * Automatically generated static modules header file\n\
 */\n\n" > $STATIC_MODULES.h

IMODULES_COUNT=0
OMODULES_COUNT=0
OPREVIOUS=""
IPREVIOUS=""
LINE=0
while read NAME TYPE MODE
do
	LINE=`expr $LINE + 1`
	FIRST_CHAR=`test $NAME && expr $NAME : '\(.\)'`
	if [ "$FIRST_CHAR" != "#" ] && [ $NAME ]; then
		if [ "$TYPE" = "output" ] && [ "$MODE" = "static" ]; then
			printf "extern int om_%s_init (int, char**, struct filed*, char*, struct om_hdr_ctx**);\n"\
				 $NAME >> $STATIC_MODULES.h
			printf "extern int om_%s_doLog (struct filed*, int, char*, struct om_hdr_ctx*);\n"\
				 $NAME >> $STATIC_MODULES.h
			printf "extern int om_%s_flush (struct filed*, struct om_hdr_ctx*);\n" $NAME >> $STATIC_MODULES.h
			printf "extern int om_%s_close (struct filed*, struct om_hdr_ctx*);\n" $NAME >> $STATIC_MODULES.h
			if [ "$OPREVIOUS" ]; then
				printf "\t%s\n" $OPREVIOUS >> $STATIC_MODULES.c
			fi
			printf "\tomodules[%s].om_name=\"%s\";\n" $OMODULES_COUNT $NAME >> $STATIC_MODULES.c
			printf "\tomodules[%s].om_init=om_%s_init;\n" $OMODULES_COUNT $NAME >> $STATIC_MODULES.c
			printf "\tomodules[%s].om_doLog=om_%s_doLog;\n" $OMODULES_COUNT $NAME >> $STATIC_MODULES.c
			printf "\tomodules[%s].om_flush=om_%s_flush;\n" $OMODULES_COUNT $NAME >> $STATIC_MODULES.c
			printf "\tomodules[%s].om_close=om_%s_close;\n" $OMODULES_COUNT $NAME >> $STATIC_MODULES.c
			OPREVIOUS="omodules[$OMODULES_COUNT].om_next=omodules+`expr $OMODULES_COUNT + 1`;"
			OMODULES_COUNT=`expr $OMODULES_COUNT + 1` 
 		elif [ "$TYPE" = "input" ] && [ "$MODE" = "static" ]; then
			printf "extern int im_%s_init (struct i_module*, char**, int);\n" $NAME >> $STATIC_MODULES.h
			printf "extern int im_%s_getLog (struct i_module*, struct im_msg*);\n" $NAME >> $STATIC_MODULES.h
			printf "extern int im_%s_close (struct i_module*);\n" $NAME >> $STATIC_MODULES.h
			if [ "$IPREVIOUS" ]; then
				printf "\t%s\n" $IPREVIOUS >> $STATIC_MODULES.c
			fi
 			printf "\timodules[%s].im_name=\"%s\";\n" $IMODULES_COUNT $NAME >> $STATIC_MODULES.c
 			printf "\timodules[%s].im_init=im_%s_init;\n" $IMODULES_COUNT $NAME >> $STATIC_MODULES.c
 			printf "\timodules[%s].im_getLog=im_%s_getLog;\n" $IMODULES_COUNT $NAME >> $STATIC_MODULES.c
 			printf "\timodules[%s].im_close=im_%s_close;\n" $IMODULES_COUNT $NAME >> $STATIC_MODULES.c
			IPREVIOUS="imodules[$IMODULES_COUNT].im_next=imodules+`expr $IMODULES_COUNT + 1`;"
			IMODULES_COUNT=`expr $IMODULES_COUNT + 1`
 		elif [ "$TYPE" != "input" ] && [ "$TYPE" != "output" ] &&
 		     [ "$MODE" != "static" ] && [ "$MODE" != "dynamic" ]; then
			echo Error reading configuration file on line $LINE: $NAME $TYPE $MODE
			exit -1
		fi
	fi
done

printf "\treturn(0);\n}\n" >> $STATIC_MODULES.c
printf "\
\n#define INPUT_MODULES\t\t%s\
\n#define OUTPUT_MODULES\t\t%s\
\n\n" $IMODULES_COUNT $OMODULES_COUNT >> $STATIC_MODULES.h

exit 0

