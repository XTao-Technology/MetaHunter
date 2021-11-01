AM_CFLAGS= $(CC_OPT)
AM_LDFLAGS= -lpthread

processor_LTLIBRARIES = scanner.la
processordir = $(libdir)/metahunter/$(PACKAGE_VERSION)/processor

scanner_la_SOURCES=scanner.c
scanner_la_LDFLAGS = -module

noinst_HEADERS=scanner.h

scanner_la_LIBADD = $(top_builddir)/src/common/libcommon.la

CLEANFILES =

indent:
	$(top_srcdir)/scripts/indent.sh
