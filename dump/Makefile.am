AM_CPPFLAGS = -I$(top_srcdir)/dump -I$(top_srcdir)/compat/include -I$(top_srcdir)/common
DISTCLEANFILES =
EXTRA_DIST =

man_MANS = dump.8
DISTCLEANFILES += dump.8
EXTRA_DIST += dump.8.in
dist_man_MANS = rdump.8

sbin_PROGRAMS = dump

dump_SOURCES = dump.h itime.c main.c optr.c tape.c traverse.c unctime.c
dump_LDADD = ../common/libcommon.la ../compat/lib/libcompat.la $(EXT2FS_LIBS) $(COM_ERR_LIBS)

dump.8: dump.8.in
	$(AM_V_GEN)\
	sed -e "s|__DUMPDATES__|$(DUMPDATESPATH)|g" \
	    -e "s|__DATE__|$(DATE)|g" \
	    -e "s|__VERSION__|$(VERSION)|g" $< > $@

install-exec-hook:
	$(MKDIR_P) $(DESTDIR)$(sbindir)
	$(LN_S) dump $(DESTDIR)$(sbindir)/rdump

uninstall-hook:
	$(RM) -f $(DESTDIR)$(sbindir)/rdump
