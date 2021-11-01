AM_CFLAGS= $(CC_OPT) $(DB_CFLAGS) $(PURPOSE_CFLAGS)

noinst_LTLIBRARIES = liblistmgr.la

dbdir = $(libdir)/metahunter/$(PACKAGE_VERSION)/db

rhsrctop = $(RBH)/src

robinhoodsrc = $(rhsrctop)/list_mgr

if USE_MYSQL_DB
DB_WRAPPER_SRC=$(robinhoodsrc)/mysql_wrapper.c
endif

if USE_SQLITE_DB
DB_WRAPPER_SRC=$(robinhoodsrc)/sqlite_wrapper.c
endif

AM_CPPFLAGS = -I$(rhsrctop)/include -I../include

nodist_liblistmgr_la_SOURCES= $(robinhoodsrc)/listmgr_init.c \
		        $(robinhoodsrc)/listmgr_common.c $(robinhoodsrc)/listmgr_common.h \
			$(robinhoodsrc)/listmgr_get.c $(robinhoodsrc)/listmgr_insert.c \
			$(robinhoodsrc)/listmgr_update.c $(robinhoodsrc)/listmgr_filters.c \
			$(robinhoodsrc)/listmgr_remove.c $(robinhoodsrc)/listmgr_iterators.c \
			$(robinhoodsrc)/listmgr_tags.c $(robinhoodsrc)/listmgr_reports.c \
			$(robinhoodsrc)/listmgr_internal.h $(robinhoodsrc)/database.h \
			$(robinhoodsrc)/listmgr_vars.c $(robinhoodsrc)/listmgr_ns.c \
			$(robinhoodsrc)/listmgr_config.c \
			$(DB_WRAPPER_SRC)

CLEANFILES =

indent:
	$(top_srcdir)/scripts/indent.sh
