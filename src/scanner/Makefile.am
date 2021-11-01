AM_CFLAGS= $(CC_OPT)
AM_LDFLAGS= -lpthread

DATE=`date '+%F %T'`
MISC_FLAGS="-DCOMPIL_DATE=\"$(DATE)\""

all_libs=       ../cfg_parser/libcfgparser.la \
	        ../common/libcommon.la

sbin_PROGRAMS=metascanner

# dependencies:
metascanner_DEPENDENCIES=$(all_libs)

metascanner_SOURCES=scanner.c
metascanner_CFLAGS=$(AM_CFLAGS)
metascanner_LDFLAGS=$(all_libs)

