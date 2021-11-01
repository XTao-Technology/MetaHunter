AM_CFLAGS= $(CC_OPT) $(DB_CFLAGS) $(PURPOSE_CFLAGS)

noinst_LTLIBRARIES=librbhlog.la

rbhsrctop = $(RBH)/src

AM_CPPFLAGS=-I$(rbhsrctop)/include -I../include

nodist_librbhlog_la_SOURCES= $(rbhsrctop)/logs/RobinhoodLogs.c

indent:
	$(top_srcdir)/scripts/indent.sh
