AM_CFLAGS= $(CC_OPT)

fs_LTLIBRARIES = ceph.la
fsdir = $(libdir)/metahunter/$(PACKAGE_VERSION)/fs

ceph_la_SOURCES= ceph-api.c mh-ceph.c

ceph_la_LDFLAGS = -module -lcephfs

ceph_la_CPPFLAGS= -I /usr/local/include

noinst_HEADERS = mh-ceph.h ceph-api.h

ceph_la_LIBADD = $(top_builddir)/src/common/libcommon.la

CLEANFILES =

indent:
	$(top_srcdir)/scripts/indent.sh

