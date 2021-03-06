Summary: A daemon for the syslog system log interface
Name: msyslog
Version: 1.3
Release: 1
Serial: 1
Group: System Environment/Daemons
License: BSD
URL: http://www.core-sdi.com/english/freesoft.html
Packager: Florin Andrei <florin@sgi.com>
Source: %{name}-%{version}.tgz
Source2: msyslog.init
Source3: msyslog.sysconfig
Buildroot: %{_tmppath}/%{name}-%{version}-root
Provides: msyslog sysklogd

%description
This project is intended as a whole revision of previous Secure Syslogd
project (wich is unsupported by now). It has all functionalities and some
more. The remaining things are Solaris support and Audit compatibility (on the
works).
The whole internal structure was redesigned to work with input and output
modules, standarizing interfaces to facilitate development for using special
devices and flexible configurations.
Current available output modules are classic, mysql, peo, pgsql, regex and
tcp. Available input modules are bsd, linux, unix, tcp and udp.

%prep
%setup -n %{name}-%{version}

%build
./configure --prefix=/
make clean; make
gzip src/man/*.5
gzip src/man/*.8

%install
if [ -d $RPM_BUILD_ROOT ]; then rm -r $RPM_BUILD_ROOT; fi
mkdir -p $RPM_BUILD_ROOT/sbin
mkdir -p $RPM_BUILD_ROOT/lib
mkdir -p $RPM_BUILD_ROOT/%{_mandir}/man5
mkdir -p $RPM_BUILD_ROOT/%{_mandir}/man8
mkdir -p $RPM_BUILD_ROOT/etc/rc.d/init.d
mkdir -p $RPM_BUILD_ROOT/etc/sysconfig
# binary files
install -m 755 $RPM_BUILD_DIR/%{name}-%{version}/src/syslogd $RPM_BUILD_ROOT/sbin/msyslogd
#
# FIX THIS!!!
#
#install -m 755 $RPM_BUILD_DIR/%{name}-%{version}/src/modules/libmsyslog.so.%{version} $RPM_BUILD_ROOT/lib/
install -m 755 $RPM_BUILD_DIR/%{name}-%{version}/src/modules/libmsyslog.so.1.3 $RPM_BUILD_ROOT/lib/
#
#
#
install -m 755 $RPM_BUILD_DIR/%{name}-%{version}/src/peo/peochk $RPM_BUILD_ROOT/sbin
# initialization and configuration files
install -m 755 $RPM_SOURCE_DIR/msyslog.init $RPM_BUILD_ROOT/etc/rc.d/init.d/msyslog
install -m 644 $RPM_SOURCE_DIR/msyslog.sysconfig $RPM_BUILD_ROOT/etc/sysconfig/msyslog
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/syslog.conf.5.gz $RPM_BUILD_ROOT/%{_mandir}/man5/msyslogd.conf.5.gz
# man pages
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/im_bsd.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/im_doors.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/im_linux.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/im_streams.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/im_tcp.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/im_udp.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/im_unix.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/om_classic.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/om_mysql.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/om_peo.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/om_pgsql.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/om_regex.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/om_tcp.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/peochk.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8
install -m 644 $RPM_BUILD_DIR/%{name}-%{version}/src/man/syslogd.8.gz $RPM_BUILD_ROOT/%{_mandir}/man8/msyslogd.8.gz

%clean
if [ -d $RPM_BUILD_ROOT ]; then rm -rf $RPM_BUILD_ROOT; fi
if [ -d $RPM_BUILD_DIR/%{name}-%{version} ]; then rm -rf $RPM_BUILD_DIR/%{name}-%{version}; fi

%files
%defattr(-,root,root)
%doc $RPM_BUILD_DIR/%{name}-%{version}/doc/*
%doc $RPM_BUILD_DIR/%{name}-%{version}/AUTHORS
%doc $RPM_BUILD_DIR/%{name}-%{version}/ChangeLog
%doc $RPM_BUILD_DIR/%{name}-%{version}/COPYING
%doc $RPM_BUILD_DIR/%{name}-%{version}/INSTALL
%doc $RPM_BUILD_DIR/%{name}-%{version}/NEWS
%doc $RPM_BUILD_DIR/%{name}-%{version}/README
/sbin
/etc/rc.d/init.d
/etc/sysconfig
%{_mandir}/man5
%{_mandir}/man8

%changelog
* Fri Oct 19 2001 Florin Andrei <florin@sgi.com>
- fixed a buglet related to permissions of /etc/sysconfig/msyslog (chmod 644)

* Thu Aug 09 2001 Florin Andrei <florin@sgi.com>
- modified msyslog.init to source functions from the right directory
- (as suggested by Hugh Bragg <hbragg@epo.org>)

* Tue Jul 31 2001 Florin Andrei <florin@sgi.com>
- version 1.07-1

* Thu Jul 26 2001 Florin Andrei <florin@sgi.com>
- version 1.06-2
- modified "Provides" so that now sysklogd can be removed without trouble
- fixed some bugs in specfile that prevented src.rpm to include some files
- removed RPM-CONFIGURE from the documentation directory (pointless)
- now src.rpm can be rebuilt without failing :-)

* Wed Jul 25 2001 Florin Andrei <florin@sgi.com>
- first version of the package (1.06-1)
