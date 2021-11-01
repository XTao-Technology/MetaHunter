AM_CFLAGS= $(CC_OPT) $(DB_CFLAGS) $(PURPOSE_CFLAGS)

noinst_LTLIBRARIES=librbhpolicy.la

rbhsrctop = $(RBH)/src

rbhpolicysrc = $(rbhsrctop)/policy_matching

AM_CPPFLAGS=-I$(rbhsrctop)/include -I../include -D_TMP_FS_MGR

nodist_librbhpolicy_la_SOURCES=$(rbhpolicysrc)/readconf_policies.c \
	$(rbhpolicysrc)/apply_policies.c

indent:
	$(top_srcdir)/scripts/indent.sh
