#
# SECURITY NOTE!!
#
# Since msyslog needs here the password to log to PGSQL,
# this file shouldn't be world readable!!!
#

# Log to server logger.mydomain.edu thru PGSQL
*.*	%pgsql -s logger.mydomain.edu -u loguser -p loguserpassword -d syslogDB -t syslogTB

