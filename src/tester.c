#include <syslog.h>
#include <varargs.h>
#include <unistd.h>

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
  printf("Usage:  %s <-l level> <-o option> <-f facility>\n"
	 "        you may specify multiple options\n");
  exit(-1);
}


int
main(int argc, char *argv[]) {
  int l, o, f, ch, started;
  extern char *optarg;
  extern int optind;

  l = 0; o = 0; f = 0; started = 0;

  while ((ch = getopt(argc, argv, "l:o:f:")) != -1) {
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
              case '?':
              default:
                      usage(argv[0]);
          }
  }
  argc -= optind;
  argv += optind;

  if ( started == 3) {
      openlog("SuperMegaTest", loption[o], lfacility[f]);
      syslog(level[l], "Superbatimensaje level = [%i]"
	      " option = [%i] facility = [%i]", l, o, f);
      closelog();
  } else if (started > 0) {
      printf("You nust specify ALL args or none\n");
      usage(argv[0]);
  } else {
      for(l = 0; l < 8; l++) {
          for(o = 0; o < 3; o++) {
              for(f = 0; f < 19; f++) {
                  openlog("SuperMegaTest", loption[o], lfacility[f]);
                  syslog(level[l], "Superbatimensaje l = [%i] o = [%i] f = [%i]",
                          l, o, f);
                  closelog();
              }
          }
      }
  }

  return (1);
}
  
