AM_CFLAGS= $(CC_OPT)
AM_LDFLAGS= -lpthread -ldl

lib_LTLIBRARIES=libcommon.la

libcommon_la_CPPFLAGS = -D__USE_FILE_OFFSET64 \
	-DMHDBDIR=\"$(libdir)/metahunter/$(PACKAGE_VERSION)/db\" \
	-DMHFSDIR=\"$(libdir)/metahunter/$(PACKAGE_VERSION)/fs\" \
	-DMHPROCDIR=\"$(libdir)/metahunter/$(PACKAGE_VERSION)/processor\"

libcommon_la_SOURCES= logging.c mem.c rb.c rbthash.c hashfn.c \
	database.c filesystem.c processor.c thread-pool.c

$(top_builddir)/src/common/libcommon.la:
	$(MAKE) -C $(top_builddir)/src/common all


indent:
	$(top_srcdir)/scripts/indent.sh
