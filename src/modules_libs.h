/*
 *
 * Here are declared needed module libs when dynamic
 *
 */

struct {
	char *name;
	char **req; /* list of required libs */
} mlibs [] = {
		{ "mysql", {"mysqlclient", NULL} },
		{ "pgsql", {"pg", NULL} },
		NULL, NULL
};

