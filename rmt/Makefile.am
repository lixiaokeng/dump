AM_CPPFLAGS = -I$(top_srcdir)/compat/include
DISTCLEANFILES =
EXTRA_DIST =

sbin_PROGRAMS = rmt

if BUILD_ERMT
sbin_PROGRAMS += ermt
endif

man_MANS = rmt.8
DISTCLEANFILES += rmt.8
EXTRA_DIST += rmt.8.in

rmt_SOURCES = rmt.c
rmt_LDADD = ../compat/lib/libcompat.la $(EXT2FS_LIBS)

ermt_SOURCES = ermt.c cipher.c
ermt_LDADD = ../compat/lib/libcompat.la $(EXT2FS_LIBS)

rmt.8: rmt.8.in
	$(AM_V_GEN)\
	sed -e "s|__DATE__|$(DATE)|g" \
	    -e "s|__VERSION__|$(VERSION)|g" $< > $@
