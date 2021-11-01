AM_CFLAGS= $(CC_OPT)
AM_LDFLAGS= -lpthread

processor_LTLIBRARIES = standard.la
processordir = $(libdir)/metahunter/$(PACKAGE_VERSION)/processor

standard_la_SOURCES=standard.c
standard_la_LDFLAGS = -module
noinst_HEADERS=standard.h
standard_la_LIBADD = $(top_builddir)/src/common/libcommon.la

CLEANFILES =

indent:
	$(top_srcdir)/scripts/indent.sh
