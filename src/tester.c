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

int
main(int argc, char *argv[]) {
  int l, i, j;

  for(l = 0; l < 8; l++) {
      for(i = 0; i < 3; i++) {
          for(j = 0; j < 19; j++) {
              openlog("SuperMegaTest", loption[i], lfacility[j]);
              syslog(level[l], "Superbatimensaje l = [%i] i = [%i] j = [%i]",
                      l, i, j);
              closelog();
          }
      }
  }
  return (0);
}
  
