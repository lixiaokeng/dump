%define	_sbindir /sbin
# XXX --enable-kerberos		needs krcmd
%define	myoptions --with-binmode=6755 --with-manowner=root --with-mangrp=root --with-manmode=0644 --with-dumpdates="%{_sysconfdir}/dumpdates" --enable-readline --enable-largefile --enable-qfa

Summary: Programs for backing up and restoring ext2/ext3 filesystems.
Name: dump
Version: 0.4b22
Release: 1
License: BSD
Group: Applications/Archiving
Source: http://download.sourceforge.net/dump/dump-%{version}.tar.gz
Requires: rmt
BuildPrereq: e2fsprogs-devel, libtermcap-devel, readline-devel
BuildRoot: %{_tmppath}/%{name}-root

%description
The dump package contains both dump and restore.  Dump examines files in
a filesystem, determines which ones need to be backed up, and copies
those files to a specified disk, tape or other storage medium.  The
restore command performs the inverse function of dump; it can restore a
full backup of a filesystem.  Subsequent incremental backups can then be
layered on top of the full backup.  Single files and directory subtrees
may also be restored from full or partial backups.

Install dump if you need a system for both backing up filesystems and
restoring filesystems after backups.

%package -n rmt
Summary: Provides certain programs with access to remote tape devices.
Group: Applications/Archiving

%description -n rmt
The rmt utility provides remote access to tape devices for programs
like dump (a filesystem backup program), restore (a program for
restoring files from a backup) and tar (an archiving program).

%package -n dump-static
Summary: Statically linked versions of dump and restore.
Group: Applications/Archiving

%description -n dump-static
The dump package contains both dump and restore.  Dump examines files in
a filesystem, determines which ones need to be backed up, and copies
those files to a specified disk, tape or other storage medium.  The
restore command performs the inverse function of dump; it can restore a
full backup of a filesystem.  Subsequent incremental backups can then be
layered on top of the full backup.  Single files and directory subtrees
may also be restored from full or partial backups.

Install dump if you need a system for both backing up filesystems and
restoring filesystems after backups.

This packages contains statically linked versions of dump and restore.

%prep
%setup -q

%build
%configure %{myoptions} --enable-static

%ifarch alpha
RPM_OPT_FLAGS=""
%endif
make OPT="$RPM_OPT_FLAGS -Wall -Wpointer-arith -Wstrict-prototypes \
                         -Wmissing-prototypes -Wno-char-subscripts"

mv dump/dump dump/dump.static
mv restore/restore restore/restore.static

make distclean

%configure %{myoptions} --enable-rmt

make OPT="$RPM_OPT_FLAGS -Wall -Wpointer-arith -Wstrict-prototypes \
                         -Wmissing-prototypes -Wno-char-subscripts"

%install
rm -rf %{buildroot}
mkdir -p %{buildroot}%{_sbindir}
mkdir -p %{buildroot}%{_mandir}/man8

%makeinstall SBINDIR=%{buildroot}%{_sbindir} MANDIR=%{buildroot}%{_mandir}/man8 BINOWNER=$(id -un) BINGRP=$(id -gn) MANOWNER=$(id -un) MANGRP=$(id -gn)

cp dump/dump.static %{buildroot}%{_sbindir}
cp restore/restore.static %{buildroot}%{_sbindir}

{ cd %{buildroot}
  strip .%{_sbindir}/* || :
  ln -sf dump .%{_sbindir}/rdump
  ln -sf dump.static .%{_sbindir}/rdump.static
  ln -sf restore .%{_sbindir}/rrestore
  ln -sf restore.static .%{_sbindir}/rrestore.static
  chmod ug-s .%{_sbindir}/rmt
  mkdir -p .%{_sysconfdir}
  > .%{_sysconfdir}/dumpdates
  ln -sf ..%{_sbindir}/rmt .%{_sysconfdir}/rmt
}

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc CHANGES COPYRIGHT KNOWNBUGS MAINTAINERS README REPORTING-BUGS THANKS TODO
%doc dump.lsm
%attr(0664,root,disk)	%config(noreplace) %{_sysconfdir}/dumpdates
%attr(0755,root,root)	%{_sbindir}/dump
%{_sbindir}/rdump
%attr(0755,root,root)	%{_sbindir}/restore
%{_sbindir}/rrestore
%{_mandir}/man8/dump.*
%{_mandir}/man8/rdump.*
%{_mandir}/man8/restore.*
%{_mandir}/man8/rrestore.*

%files -n rmt
%defattr(-,root,root)
%attr(0755,root,root)	%{_sbindir}/rmt
%{_sysconfdir}/rmt
%{_mandir}/man8/rmt.*

%files -n dump-static
%defattr(-,root,root)
%attr(0755,root,root)	%{_sbindir}/dump.static
%{_sbindir}/rdump.static
%attr(0755,root,root)	%{_sbindir}/restore.static
%{_sbindir}/rrestore.static

%changelog
* Sat May 12 2001 Stelian Pop <pop@noos.fr>
- dump 0.4b22 released, first packaging.

* Sat Jan 30 2001 Stelian Pop <pop@noos.fr>
- dump 0.4b21 released, first packaging.

* Fri Nov 10 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b20 released, first packaging.

* Sun Aug 20 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b19 released, first packaging.

* Thu Jun 30 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b18 released, first packaging.

* Thu Jun  1 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b17 released, first packaging.

* Sat Mar 11 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b16 released, first packaging.

* Thu Mar  2 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b15 released, first packaging.

* Thu Feb 10 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b14 released, first packaging.

* Fri Jan 21 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b13 released, first packaging.

* Fri Jan 8 2000 Stelian Pop <pop@noos.fr>
- dump 0.4b12 released, first packaging.

* Sun Dec 5 1999 Stelian Pop <pop@noos.fr>
- dump 0.4b11 released, first packaging.

* Sun Nov 21 1999 Stelian Pop <pop@noos.fr>
- dump 0.4b10 released, first packaging.

* Thu Nov 11 1999 Stelian Pop <pop@noos.fr>
- make static versions also for rescue purposes.

* Wed Nov 5 1999 Stelian Pop <pop@noos.fr>
- dump 0.4b9 released, first packaging.

* Wed Nov 3 1999 Stelian Pop <pop@noos.fr>
- dump 0.4b8 released, first packaging.

* Thu Oct 8 1999 Stelian Pop <pop@noos.fr>
- dump 0.4b7 released, first packaging.

* Thu Sep 30 1999 Stelian Pop <pop@noos.fr>
- dump 0.4b6 released, first packaging.

* Fri Sep 10 1999 Jeff Johnson <jbj@redhat.com>
- recompile with e2fsprogs = 1.15 (#4962).

* Sat Jul 31 1999 Jeff Johnson <jbj@redhat.com>
- workaround egcs bug (#4281) that caused dump problems (#2989).
- use sigjmp_buf, not jmp_buf (#3260).
- invoke /etc/rmt (instead of rmt) like other unices. (#3272).
- use glibc21 err/glob rather than the internal compatibility routines.
- wire $(OPT) throughout Makefile's.
- fix many printf problems, mostly lint clean.
- merge SuSE, Debian and many OpenBSD fixes.

* Thu Mar 25 1999 Jeff Johnson <jbj@redhat.com>
- remove setuid/setgid bits from /sbin/rmt (dump/restore are OK).

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com> 
- auto rebuild in the new build environment (release 6)

* Fri Mar 19 1999 Jeff Johnson <jbj@redhat.com>
- strip binaries.

* Thu Mar 18 1999 Jeff Johnson <jbj@redhat.com>
- Fix dangling symlinks (#1551).

* Wed Mar 17 1999 Michael Maher <mike@redhat.com>
- Top O' the morning, build root's fixed for man pages.  

* Fri Feb 19 1999 Preston Brown <pbrown@redhat.com>
- upgraded to dump 0.4b4, massaged patches.

* Tue Feb 02 1999 Ian A Cameron <I.A.Cameron@open.ac.uk>
- added patch from Derrick J Brashear for traverse.c to stop bread errors

* Wed Jan 20 1999 Jeff Johnson <jbj@redhat.com>
- restore original 6755 root.tty to dump/restore, defattr did tty->root (#684).
- mark /etc/dumpdates as noreplace.

* Tue Jul 14 1998 Jeff Johnson <jbj@redhat.com>
- add build root.

* Tue May 05 1998 Prospector System <bugs@redhat.com>
- translations modified for de, fr, tr

* Thu Apr 30 1998 Cristian Gafton <gafton@redhat.com>
- added a patch for resolving linux/types.h and sys/types.h conflicts

* Wed Dec 31 1997 Erik Troan <ewt@redhat.com>
- added prototype of llseek() so dump would work on large partitions

* Thu Oct 30 1997 Donnie Barnes <djb@redhat.com>
- made all symlinks relative instead of absolute

* Thu Jul 10 1997 Erik Troan <ewt@redhat.com>
- built against glibc

* Thu Mar 06 1997 Michael K. Johnson <johnsonm@redhat.com>
- Moved rmt to its own package.

* Tue Feb 11 1997 Michael Fulbright <msf@redhat.com>
- Added endian cleanups for SPARC

* Fri Feb 07 1997 Michael K. Johnson <johnsonm@redhat.com> 
- Made /etc/dumpdates writeable by group disk.
