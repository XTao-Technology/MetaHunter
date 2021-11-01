AM_CFLAGS= $(CC_OPT)
AM_LDFLAGS= -lpthread

processor_LTLIBRARIES = irods.la
processordir = $(libdir)/metahunter/$(PACKAGE_VERSION)/processor

irods_la_SOURCES=mh_irods.c

irods_la_LDFLAGS = -module -lz -lssl -lcrypto -ldl -lm -lirods_client -lirods_client_api
irods_la_LDFLAGS +=  -lboost_filesystem -lboost_regex -lboost_thread -lboost_chrono -lboost_program_options -ljansson

irods_la_LIBADD = $(top_builddir)/src/common/libcommon.la

irods_la_CPPFLAGS= $(IRODS_CPPFLAGS)

noinst_HEADERS=mh_irods.h

CLEANFILES =

indent:
	$(top_srcdir)/scripts/indent.sh
