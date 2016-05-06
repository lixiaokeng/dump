AM_CPPFLAGS = -I$(top_srcdir)/compat/include

noinst_LTLIBRARIES = libcompat.la

libcompat_la_SOURCES = \
	bylabel.c \
	compaterr.c \
	compatglob.c \
	minilzo.c \
	rmtflags.c \
	system.c
libcompat_la_LIBADD = \
	$(BLKID_LIBS)

# List headers for `make dist` to pull in.
libcompat_la_SOURCES += \
	../include/bsdcompat.h \
	../include/bylabel.h \
	../include/compaterr.h \
	../include/compatglob.h \
	../include/darwin.h \
	../include/lzoconf.h \
	../include/minilzo.h \
	../include/pathnames.h \
	../include/rmtflags.h \
	../include/system.h \
	../include/protocols/dumprestore.h
