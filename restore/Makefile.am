AM_CPPFLAGS = $(CCTRANSSELINUX) -I$(top_srcdir)/common -I$(top_srcdir)/compat/include
DISTCLEANFILES =
EXTRA_DIST =

man_MANS = restore.8
DISTCLEANFILES += restore.8
EXTRA_DIST += restore.8.in
dist_man_MANS = rrestore.8

sbin_PROGRAMS = restore

restore_SOURCES = \
	dirs.c extern.h interactive.c main.c restore.c restore.h symtab.c \
	tape.c utilities.c xattr.c
restore_LDADD = ../common/libcommon.la ../compat/lib/libcompat.la \
	$(READLINE) $(LTRANSSELINUX) $(EXT2FS_LIBS) -le2p

restore.8: restore.8.in
	$(AM_V_GEN)\
	sed -e "s|__DATE__|$(DATE)|g" \
	    -e "s|__VERSION__|$(VERSION)|g" $< > $@

install-exec-hook:
	$(MKDIR_P) $(DESTDIR)$(sbindir)
	$(LN_S) restore $(DESTDIR)$(sbindir)/rrestore

uninstall-hook:
	$(RM) -f $(DESTDIR)$(sbindir)/rrestore
