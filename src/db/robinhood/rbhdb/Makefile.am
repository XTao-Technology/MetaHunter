AM_CFLAGS= $(CC_OPT) $(DB_CFLAGS)

db_LTLIBRARIES=robinhood.la
dbdir = $(libdir)/metahunter/$(PACKAGE_VERSION)/db

rbhsrctop = $(RBH)/src

AM_CPPFLAGS=-I$(rbhsrctop)/include -I../include -D_TMP_FS_MGR

all_libs= $(top_builddir)/src/db/robinhood/rbhcommon/librbhcommon.la \
	$(top_builddir)/src/db/robinhood/listmgr/liblistmgr.la \
	$(top_builddir)/src/db/robinhood/rbhlog/librbhlog.la \
	$(top_builddir)/src/db/robinhood/rbhcfg/librbhcfg.la \
	$(top_builddir)/src/db/robinhood/rbhpolicy/librbhpolicy.la

noinst_HEADERS = rbh-db.h

robinhood_la_SOURCES = rbh-db.c
robinhood_la_LDFLAGS = -module 	$(all_libs) $(DB_LDFLAGS)

robinhood_la_LIBADD = $(top_builddir)/src/common/libcommon.la

indent:
	$(top_srcdir)/scripts/indent.sh
