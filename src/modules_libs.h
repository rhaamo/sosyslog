/*
 *
 * Here are declared needed module libs when dynamic
 *
 */

struct {
	char *name;
	char **req; /* list of required libs */
} mlibs [] = {
		{ "mysql", {"libmysqlclient", NULL} },
		{ "pgsql", {"libpg", NULL} },
		NULL, NULL
};

