int Debug = 1;

#include "om_peo.c"

int main()
{
struct filed f;
struct om_header_ctx *c;
char *argv[] = {
		"myname",
		"-l",
		"-m",
		"sha1",
		"-k",
		"/var/ssyslog/.var.log.messages.key"
};

if (om_peo_init(6, &argv, &f, argv[0], &c) == -1) {
	perror("caca en init");
	return -1;
}

//			123456789012345
strncpy(f.f_lasttime, "Tu 15 Maio 2000", 16);
strcpy(f.f_prevhost, "hostito");

if (om_peo_doLog(&f, 0, "mensaje 1", c) != NULL) {
	perror ("problemas en dolog 1");
}
if (om_peo_doLog(&f, 0, "mensaje 2", c) != NULL) {
	perror ("problemas en dolog 2");
}
if (om_peo_doLog(&f, 0, "mensaje 3", c) != NULL) {
	perror ("problemas en dolog 3");
}

om_peo_close(&f, &c);

return 0;
}



