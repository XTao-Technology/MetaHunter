AM_CFLAGS= $(CC_OPT)
AM_LDFLAGS= -lpthread

DATE=`date '+%F %T'`
MISC_FLAGS="-DCOMPIL_DATE=\"$(DATE)\""

all_libs=       ../cfg_parser/libcfgparser.la \
	        ../common/libcommon.la

sbin_PROGRAMS=metahunter

# dependencies:
metahunter_DEPENDENCIES=$(all_libs)

metahunter_SOURCES=hunter.c
metahunter_CFLAGS=$(AM_CFLAGS)
metahunter_LDFLAGS=$(all_libs)

