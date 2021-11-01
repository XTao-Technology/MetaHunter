AM_CFLAGS= $(CC_OPT)
AM_LDFLAGS= -lm

noinst_LTLIBRARIES=libcfgparser.la

libcfgparser_la_SOURCES= cJSON.c cfg-parser.c
libcfgparser_la_LIBADD = $(top_builddir)/src/common/libcommon.la

CLEANFILES =

indent:
	$(top_srcdir)/scripts/indent.sh
