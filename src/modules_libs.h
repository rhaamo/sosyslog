/*
 *
 * Here are declared needed module libs when dynamic
 *
 */

#define MLIB_MAX 10

struct {
	char *name;
	char *libs[MLIB_MAX];
} mlibs[] = {
		{"mysql", {"libmysqlclient.so", NULL} },
		{"pgsql", {"libpq.so", NULL} },
		{ NULL, { NULL } }
	};
