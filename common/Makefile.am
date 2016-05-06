AM_CPPFLAGS = -I$(top_srcdir)/dump -I$(top_srcdir)/compat/include

noinst_LTLIBRARIES = libcommon.la

libcommon_la_SOURCES = \
	dumprmt.c \
	indexer.h \
	legacy_indexer.c \
	slave.h \
	sqlite_indexer.c \
	transformation.h \
	transformation_null.c \
	transformation_lzo.c \
	transformation_zlib.c \
	transformation_bzlib.c \
	transformation_ssl.c
libcommon_la_LIBADD = \
	$(UUID_LIBS) \
	$(ZLIB_LIBS) \
	$(BZLIB) \
	$(LZOLIB) \
	$(SSLLIB) \
	$(SQLITE3)

check_PROGRAMS = indexer_test

indexer_test_SOURCES = indexer_test.c
indexer_test_LDADD = libcommon.la

# Disabled due to missing transform_test.c -- it was accidentally not added.
# We can re-add when someone re-implements the test code :(.
#check_PROGRAMS += transform_test
#transform_test_SOURCES = transform_test.c
#transform_test_LDADD = libcommon.la
