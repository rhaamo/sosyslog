#include <syslog.h>
#include <varargs.h>
#include <unistd.h>
#include <string.h>

int level[] = { LOG_EMERG, LOG_ALERT, LOG_CRIT, LOG_ERR, LOG_WARNING,
			LOG_NOTICE, LOG_INFO, LOG_DEBUG };
int loption[] = { LOG_CONS, LOG_NDELAY, LOG_PERROR, LOG_PID };

int lfacility[] = { LOG_AUTH, LOG_AUTHPRIV, LOG_CRON, LOG_DAEMON, LOG_FTP,
			LOG_KERN, LOG_LPR, LOG_MAIL, LOG_NEWS, LOG_SYSLOG,
			LOG_USER, LOG_UUCP, LOG_LOCAL0, LOG_LOCAL1,
			LOG_LOCAL2, LOG_LOCAL3, LOG_LOCAL4, LOG_LOCAL5,
			LOG_LOCAL6, LOG_LOCAL7 };

void
usage(char *pname) {
  printf("Usage:  %s <-l level> <-o option> <-f facility> "
         " <-m 'message'> <-d>\n"
	 "        you may specify multiple options\n"
	 "        you MUST specify l,o,f together\n");
  exit(-1);
}

#define dprintf		if (Debug) printf

int
main(int argc, char *argv[]) {
  int l, o, f, ch, started, m, Debug;
  char *pname, msg[512];
  extern char *optarg;
  extern int optind;

  l = 0; o = 0; f = 0; started = 0; m = 0; Debug = 0;
  pname = strdup(argv[0]);

  while ((ch = getopt(argc, argv, "l:o:f:m:d")) != -1) {
          switch (ch) {
              case 'l':
                      l = atoi(optarg);
                      started++;
                      break;
              case 'o':
                      o |= atoi(optarg);
                      started++;
                      break;
              case 'f':
                      l = atoi(optarg);
                      started++;
                      break;
              case 'm':
                      strncpy(msg, optarg, 511);
                      m++;
                      break;
              case 'd':
                      Debug++;
                      break;
              case '?':
              default:
                      usage(pname);
          }
  }
  argc -= optind;
  argv += optind;

  if (!m)
      sprintf(msg, "Superbatimensaje default");

  if ( started == 3) {
      openlog("SuperMegaTest", loption[o], lfacility[f]);
      dprintf("%s: going to log l %i, o %i, f %i\n", pname, l, o, f);
      syslog(level[l], "%s level = [%i]"
	      " option = [%i] facility = [%i]", msg, l, o, f);
      closelog();
  } else if (started > 0) {
      dprintf("You nust specify ALL args or none\n");
      usage(pname);
  } else {
      for(l = 0; l < 8; l++) {
          for(o = 0; o < 3; o++) {
              for(f = 0; f < 19; f++) {
                  openlog("SuperMegaTest", loption[o], lfacility[f]);
                  syslog(level[l], "%s level = [%i] option = [%i]"
                          " facility = [%i]", msg, l, o, f);
                  dprintf("%s: going to log l %i, o %i, f %i\n", pname, l, o, f);
                  closelog();
              }
          }
      }
  }

  return (1);
}
  
