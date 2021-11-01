AM_CFLAGS= $(CC_OPT) $(DB_CFLAGS) $(PURPOSE_CFLAGS) -DYY_NO_UNPUT

rbhsrctop = $(RBH)/src

rbhcfgsrc = $(rbhsrctop)/cfg_parsing

$(rbhcfgsrc)/conf_lex.l: $(rbhcfgsrc)/conf_yacc.h
$(rbhcfgsrc)/conf_yacc.h: $(rbhcfgsrc)/conf_yacc.c

noinst_LTLIBRARIES          = librbhcfg.la

AM_CPPFLAGS=-I$(rbhsrctop)/include -I$(rbhcfgsrc) -I../include

nodist_librbhcfg_la_SOURCES = $(rbhcfgsrc)/conf_yacc.y $(rbhcfgsrc)/analyze.c \
	$(rbhcfgsrc)/analyze.h $(rbhcfgsrc)/conf_lex.l \
	$(rbhcfgsrc)/config_parsing.c $(rbhcfgsrc)/RobinhoodConfig.c

indent:
	$(top_srcdir)/scripts/indent.sh
