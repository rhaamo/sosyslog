int Debug = 1;

#include "om_peo.c"

int main()
{
struct filed f;
struct om_header_ctx *c;
char *argv[] = {
		"myname",
		"-m",
		"rmd160",
		"-k",
		"pistola"
};

if (om_peo_init(5, &argv, &f, argv[0], &c) == -1) {
	perror("caca en init");
	return -1;
}


if (om_peo_doLog(&f, 0, "mensaje", c) != NULL) {
	perror ("problemas en dolog 1");
}
if (om_peo_doLog(&f, 0, "mensaje", c) != NULL) {
	perror ("problemas en dolog 2");
}
if (om_peo_doLog(&f, 0, "mensaje", c) != NULL) {
	perror ("problemas en dolog 3");
}

om_peo_close(&f, &c);

return 0;
}


