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
		{"mysql", {"mysqlclient", NULL} },
		{"pgsql", {"pq", NULL} },
		{ NULL, { NULL } }
	};
