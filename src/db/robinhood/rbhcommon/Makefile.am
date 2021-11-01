AM_CFLAGS= $(CC_OPT) $(DB_CFLAGS)

noinst_LTLIBRARIES=librbhcommon.la

rbhsrctop = $(RBH)/src

AM_CPPFLAGS=-I$(rbhsrctop)/include -D_TMP_FS_MGR -I../include

nodist_librbhcommon_la_SOURCES= $(rbhsrctop)/common/RobinhoodMisc.c \
                     $(rbhsrctop)/common/global_config.c \
                     $(rbhsrctop)/common/uidgidcache.c \
                     $(rbhsrctop)/common/RW_Lock.c

indent:
	$(top_srcdir)/scripts/indent.sh
