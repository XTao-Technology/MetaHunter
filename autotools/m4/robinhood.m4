#
# This macro test for Robinhood metahunter
#
AC_DEFUN([AX_RBH_INFO],
[
        AC_ARG_WITH([robinhood],
                AS_HELP_STRING([--with-robinhood=PATH],
                [Path to robinhood srouce code]),
                [rbhsrc="$withval"])

	AC_MSG_CHECKING([robinhood source directory])
	AS_IF([test -z "$rbhsrc"], [
		    rbhsrc=$(ls -1d /usr/src/robinhood-* 
                                     2>/dev/null | head -1)
                ]
        )

	AC_MSG_RESULT([$rbhsrc])


	AS_IF([test ! -d "$rbhsrc"], [
                AC_MSG_ERROR([
        *** Please make sure the robinhood source for your distribution
        *** is installed and then try again.  If that fails, you can specify the
        *** location of the robinhood source with the '--with-robinhood=PATH' option.])
        ])

	RBH=${rbhsrc}
	AC_SUBST(RBH)

	AC_ARG_WITH( [db], AS_HELP_STRING([--with-db=MYSQL|SQLITE (default=MYSQL)],[type of database engine] ),
	             DB="$withval", DB="MYSQL")

        AM_PROG_LEX
        AC_PROG_YACC

	# @TODO check database libraries and path

	# Db dependent checks and flags
	case $DB in
	    MYSQL)
	
	        # check mysql version and mysql_config program
	        AX_MYSQL_INFO
	
	        AC_CHECK_HEADERS([mysql/mysql.h])
	        AC_CHECK_HEADER([mysql/mysql.h], HAVE_MYSQLCLNT="true",
	                        AC_MSG_ERROR([MySQL client header not found (mysql/mysql.h). mysql-devel may not be installed.]))
	
	        DB_CFLAGS="-D_MYSQL `$MYSQL_CONFIG --include`"
	        DB_LDFLAGS=`mysql_config --libs_r`
	
	        if test "$MYSQL_VERSION" -lt "5" ; then
	            AC_MSG_WARN([MySQL version is too old (<5), optimized accounting won't be supported.])
	        else
	            DB_CFLAGS="$DB_CFLAGS -D_MYSQL5"
	        fi
	        ;;
	
	    SQLITE)
	        # check lib and headers
	        AC_CHECK_HEADER([sqlite3.h], HAVE_SQLITE_HEADER="true",
	                    AC_MSG_ERROR([sqlite-devel not installed]))
	        AC_CHECK_LIB([sqlite3], [sqlite3_exec], HAVE_SQLITE_LIB="true",
	                    AC_MSG_ERROR([sqlite3 library not found]))
	        DB_CFLAGS="-D_SQLITE"
	        DB_LDFLAGS="-lsqlite3"
	        ;;
	    *)
	        AC_MSG_ERROR([This Database is not supported yet])
	        ;;
	esac
	
	AC_SUBST(DB_CFLAGS)
	AC_SUBST(DB_LDFLAGS)

        PURPOSE_CFLAGS="-D_TMP_FS_MGR"
        PURPOSE_BIN="robinhood"
        PURPOSE_EXT="tmpfs"
        PURPOSE_SVC="robinhood"
	AC_DEFINE_UNQUOTED([PURPOSE_EXT],"$PURPOSE_EXT", [Directory extension for the given usage])
	AC_SUBST(PURPOSE_CFLAGS)
	AC_SUBST(PURPOSE_BIN)
	AC_SUBST(PURPOSE_EXT)
	AC_SUBST(PURPOSE_SVC)

	AC_DEFINE(XTAO, 1, [XTAO Metahunter Robinhood])

	AC_CHECK_FUNC([getmntent_r],[getmntent_r=yes],[getmntent_r=no])
	test "$getmntent_r" = "yes" && AC_DEFINE(HAVE_GETMNTENT_R, 1, [Reentrant version of getmntent exists])

])

