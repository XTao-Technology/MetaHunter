# -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil; -*-
# vim:expandtab:shiftwidth=4:tabstop=4:
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)

AC_INIT( [metahunter], [0.0.24], [javen.wu@xtaotech.com])

AC_DEFINE_UNQUOTED(RELEASE, "$RELEASE", [release info] )
AC_SUBST(RELEASE)

AC_CONFIG_AUX_DIR([autotools])
AC_CONFIG_MACRO_DIR([autotools/m4])
AC_CONFIG_HEADER([src/include/config.h])

AM_INIT_AUTOMAKE([-Wall -Werror tar-pax foreign])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_PREFIX_DEFAULT([/usr])

AC_GNU_SOURCE

# disable shared libraries generation
AC_ENABLE_SHARED(yes)

# required for automake 1.12 (since fedora 18)
m4_ifdef([AM_PROG_AR], [AM_PROG_AR])

AC_PROG_LIBTOOL
AC_PROG_CC

# required for automake 1.12 (since fedora 18)
m4_ifdef([AM_PROG_CC_C_O], [AM_PROG_CC_C_O])

AC_C_INLINE

# define everything necessary for accessing large files (64bits offset)
AC_SYS_LARGEFILE

# gcc options
#CC_OPT="-Wall -Werror"
CC_OPT="-Wall -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations"
AC_SUBST(CC_OPT)

AC_CHECK_SIZEOF([nlink_t])
AC_CHECK_SIZEOF([off_t])
AC_CHECK_SIZEOF([ino_t])
AC_CHECK_SIZEOF([dev_t])
AC_CHECK_SIZEOF([time_t])
AC_CHECK_SIZEOF([size_t])
AC_CHECK_SIZEOF([pthread_t])

AX_ENABLE_FLAG([strict], [Compile with -Wall -Werror], [-Wall -Werror])

# debug flags
AX_ENABLE_FLAG([debug-db], [enables debug traces for database operations], [-D_DEBUG_DB])
AX_ENABLE_FLAG([debug-parsing], [enables debug traces for configuration file parsing], [-D_DEBUG_PARSING])
AM_CONDITIONAL(DEBUG_PARSING, test "x$enable_debug_parsing" == "xyes" )

AX_ENABLE_FLAG([debug-pipeline], [enables debug traces for entry processor pipeline], [-D_DEBUG_ENTRYPROC])
AX_ENABLE_FLAG([bench-scan], [test only: build special version for scan benchmarking], [-D_BENCH_SCAN])
AX_ENABLE_FLAG([bench-db], [test only: build special version for DB benchmarking], [-D_BENCH_DB])
AX_ENABLE_FLAG([bench-pipeline], [test only: build special version for pipeline benchmarking], [-D_BENCH_PIPELINE -D_BENCH_DB])
AX_ENABLE_FLAG([gprof], [test only: add gprof info to the binaries], [-g -pg])
AX_ENABLE_FLAG([gcov], [test only: add gcov info to the binaries], [--coverage])

# behavior flags
AX_DISABLE_FLAG([atfunc], [Don't use 'at' functions for scanning], [-D_NO_AT_FUNC])

AX_ENABLE_FLAG( [fid2path-leading-slash], [must be enabled if fid2path() returns a leading slash], [-D_FID2PATH_LEADING_SLASH] )

# program to be used for mailing
AX_PROG_MAIL

AC_ARG_ENABLE([robinhood],
              AC_HELP_STRING([--enable-robinhood],
                             [enable metahunter robinhood]),
                             enable_robinhood="$enableval", enable_robinhood="no")

if test "x$enable_robinhood" == "xyes"; then
        AX_RBH_INFO
fi

LDFLAGS="$LDFLAGS -L/usr/lib64"

AC_CHECK_LIB([cephfs], [ceph_lstat], [CEPHFS_LIB='-lcephfs'], AC_MSG_ERROR([libcephfs is required]))

AM_CONDITIONAL(USE_MYSQL_DB,    test "$DB" = "MYSQL")
AM_CONDITIONAL(USE_SQLITE_DB,   test "$DB" = "SQLITE")

AM_CONDITIONAL(MNTENTCOMPAT, test "$getmntent_r" = "no" )

AM_CONDITIONAL(ROBINHOOD,  test  "x$enable_robinhood" = "xyes" )

AC_ARG_ENABLE([irods],
              AC_HELP_STRING([--enable-irods],
                             [enable metahunter robinhood]))

if test "x$enable_irods" == "xyes"; then
#        AC_CHECK_LIB([irods_client], [rcCollCreate], [IRODSC_LIB='-lirods_client'], AC_MSG_ERROR([libirods_client is required]))
        IRODS_CPPFLAGS="-I /usr/include/irods"
        AC_SUBST(IRODS_CPPFLAGS)
fi

AM_CONDITIONAL(IRODS,  test  "x$enable_irods" = "xyes" )

AC_CHECK_HEADERS([attr/xattr.h],[],[AC_MSG_ERROR([libattr-devel is not installed.])])


CFLAGS="$CFLAGS -I\$(top_srcdir)/src/include"


# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([string.h sys/param.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_UID_T
AC_TYPE_SIZE_T

AS_AC_EXPAND(CONFDIR, $sysconfdir)
if test $prefix = NONE && test "$CONFDIR" = "/usr/etc"  ; then
    CONFDIR="/etc"
fi
AC_MSG_NOTICE([Using config dir $CONFDIR])
AC_DEFINE_UNQUOTED([SYSCONFDIR],"$CONFDIR", [Configuration directory])
AC_SUBST(CONFDIR)

AS_AC_EXPAND(SBINDIR, $sbindir)
AC_SUBST(SBINDIR)

# for exporting to spec file
AC_SUBST(ac_configure_args)

AC_CONFIG_FILES([Makefile
         src/Makefile
         src/include/Makefile src/common/Makefile
         src/cfg_parser/Makefile
         src/hunter/Makefile
         src/scanner/Makefile
         src/processor/Makefile
         src/processor/standard/Makefile
         src/processor/scanner/Makefile
         src/processor/irods/Makefile
         src/db/Makefile
         src/db/robinhood/Makefile
         src/db/robinhood/include/Makefile
         src/db/robinhood/listmgr/Makefile
         src/db/robinhood/rbhcommon/Makefile
         src/db/robinhood/rbhdb/Makefile
         src/db/robinhood/rbhlog/Makefile
         src/db/robinhood/rbhcfg/Makefile
         src/db/robinhood/rbhpolicy/Makefile
         src/fs/Makefile
         src/fs/ceph/Makefile
         metahunter.spec
         rpms/Makefile
])

AC_OUTPUT
