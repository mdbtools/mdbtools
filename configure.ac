dnl Process this file with autoconf to produce a configure script.
AC_INIT([mdbtools],[0.9.1],[https://github.com/mdbtools/mdbtools/issues],[],[https://github.com/mdbtools/mdbtools])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR(src/extras/mdb-dump.c)
AM_INIT_AUTOMAKE([foreign dist-zip])

MDBTOOLS_VERSION_MAJOR=0
MDBTOOLS_VERSION_MINOR=9
MDBTOOLS_VERSION_MICRO=1

# Update these numbers with every release
# See https://www.gnu.org/software/libtool/manual/html_node/Updating-version-info.html
VERSION_INFO=3:1:0
AC_SUBST(VERSION_INFO)

AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_PROG_CC(gcc)
AC_PROG_CXX
dnl Checks for programs.
AC_PROG_MAKE_SET
m4_pattern_allow([AM_PROG_AR], [AM_PROG_AR])
LT_INIT([win32-dll])
AC_PROG_LEX
AC_PROG_YACC

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h limits.h unistd.h xlocale.h)
AC_CHECK_HEADERS(wordexp.h)
AC_CHECK_LIB(mswstr, DBLCMapStringW)
AC_CHECK_DECLS([program_invocation_short_name], [], [], [[
                #define _GNU_SOURCE
                #include <errno.h>]])

dnl Checks for library functions.
VL_LIB_READLINE
AC_CHECK_FUNCS(strptime fmemopen gmtime_r wcstombs_l vasprintf vasnprintf)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

AM_ICONV
AM_GCC_ATTRIBUTE_ALIAS

dnl Enable large files on 32-bit systems
AC_SYS_LARGEFILE

dnl no optional stuff by default
OPTDIRS=""

dnl ---------------------------------------------------------------------
dnl Compile time options
dnl ---------------------------------------------------------------------

sql=true
AC_MSG_CHECKING( Are we using flex )
if test "x$LEX" = "xflex"; then
LFLAGS="$LFLAGS -i -8"
AC_MSG_RESULT( yes );
else
AC_MSG_RESULT( no - SQL engine disable);
sql=false
fi

if $YACC -V | grep "bison.* 3[.]" >/dev/null 2>&1; then
    if $YACC -Wno-conflicts-sr -V >/dev/null 2>&1; then
        YFLAGS="$YFLAGS -Wno-conflicts-sr"
    fi
    if $YACC -Wno-yacc -V >/dev/null 2>&1; then
        YFLAGS="$YFLAGS -Wno-yacc"
    fi
else
    AC_MSG_WARN([Bison 3.0+ is not available: SQL disabled.])
    sql=false
fi

if test "x$sql" = "xtrue"; then
	CFLAGS="$CFLAGS -DSQL"
	OPTDIRS="$OPTDIRS sql"
fi

AM_CONDITIONAL(SQL, test x$sql = xtrue)
AC_SUBST(SQL)
AC_SUBST(LFLAGS)

CFLAGS="$CFLAGS -Wall -Werror"
AS_CASE([$host],
        [*mingw*|*cygwin*], [LDFLAGS="$LDFLAGS -no-undefined"], [])
AS_CASE([$host],
        [*mingw*], [LDFLAGS="$LDFLAGS -lWs2_32"], [])

dnl Fuzz testing

AC_ARG_ENABLE([fuzz-testing], AS_HELP_STRING([--enable-fuzz-testing], ["Enable fuzz testing (requires Clang 6 or later)"]), [
   AC_MSG_CHECKING([whether $CC accepts -fsanitize=fuzzer])
   tmp_saved_flags=$[]_AC_LANG_PREFIX[]FLAGS
   _AC_LANG_PREFIX[]FLAGS="$[]_AC_LANG_PREFIX[]FLAGS -fsanitize=fuzzer"
   AC_LINK_IFELSE([AC_LANG_PROGRAM()],
       [
        AC_MSG_RESULT(yes)
        OPTDIRS="$OPTDIRS fuzz"
        fuzzer=yes],
        AC_MSG_RESULT(no)
       AC_MSG_FAILURE([-fsanitize=fuzzer not supported (Required with --enable-fuzz-testing)]))
   _AC_LANG_PREFIX[]FLAGS=$tmp_saved_flags
], [fuzzer=no])
AM_CONDITIONAL([FUZZER_ENABLED], test "x$fuzzer" = "xyes")

AC_ARG_VAR([LIB_FUZZING_ENGINE], [Location of prebuilt fuzzing engine library])
AC_SUBST([LIB_FUZZING_ENGINE])

dnl Enable -Wl,--as-needed by default to prevent overlinking

AC_ARG_ENABLE([as-needed],
  AS_HELP_STRING([--disable-as-needed],[disable overlinking protection]),
  [enable_as_needed=$enableval], [enable_as_needed=yes])

if test "x$enable_as_needed" != "xno"; then
  AC_MSG_CHECKING([whether $LD accepts --as-needed])
  case `$LD --as-needed -v 2>&1 </dev/null` in
  *GNU* | *'with BFD'*)
    LDFLAGS="$LDFLAGS -Wl,--as-needed"
    AC_MSG_RESULT([yes])
    ;;
  *)
    AC_MSG_RESULT([no])
    ;;
  esac
fi

dnl check for iODBC

AC_ARG_WITH(iodbc,
  AS_HELP_STRING([--with-iodbc=/path/to/iodbc], [build odbc driver against iODBC]))
if test "$with_iodbc"; then
	HAVE_ODBC=true
	ODBC_CFLAGS=$(iodbc-config --prefix="$with_iodbc" --cflags)
	ODBC_LIBS=$(iodbc-config --prefix="$with_iodbc" --libs)
	ODBC_LDFLAGS=""
	CFLAGS="$CFLAGS -DIODBC"

	OLDLDFLAGS=$LDFLAGS
	LDFLAGS="$LDFLAGS $ODBC_LIBS"
	AC_CHECK_LIB(iodbcinst, SQLGetPrivateProfileString, [FOO=bar],
                 AC_MSG_ERROR([Could not find SQLGetPrivateProfileString in -liodbcinst]))
	LDFLAGS=$OLDLDFLAGS
fi

dnl check for unixODBC
AC_ARG_WITH(unixodbc,
  AS_HELP_STRING([--with-unixodbc=/path/to/unixodbc], [build odbc driver against unixODBC]))
if test "$with_unixodbc"; then
	HAVE_ODBC=true
	ODBC_CFLAGS="-I$with_unixodbc/include"
	ODBC_LIBS="-L$with_unixodbc/lib"
	CFLAGS="$CFLAGS -DUNIXODBC"

	dnl SIZEOF_LONG_INT and HAVE_LONG_LONG are required by some versions of unixODBC
	dnl https://github.com/lurcher/unixODBC/issues/40
	AC_CHECK_SIZEOF([long int])
	AC_CHECK_TYPES([long long])

	OLDLDFLAGS=$LDFLAGS
	LDFLAGS="$LDFLAGS $ODBC_LIBS"
	AC_CHECK_LIB(odbcinst, SQLGetPrivateProfileString, [ODBC_LIBS="$ODBC_LIBS -lodbcinst"],
                 AC_MSG_ERROR([Could not find SQLGetPrivateProfileString in -lodbcinst]))
	LDFLAGS=$OLDLDFLAGS

	OLDLDFLAGS=$LDFLAGS
	LDFLAGS="$LDFLAGS -Wl,-Bsymbolic-functions"
	AC_MSG_CHECKING([for -Bsymbolic-functions linker flag])
	AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
				[AC_MSG_RESULT(yes)
				ODBC_LDFLAGS="-Wl,-Bsymbolic-functions"],
				[AC_MSG_RESULT(no)
				ODBC_LDFLAGS=""])
	LDFLAGS=$OLDLDFLAGS
fi

if test "x$HAVE_ODBC" = "xtrue"; then
	if test "x$sql" != "xtrue" ; then
		AC_MSG_ERROR([ODBC requires flex and bison for the SQL engine])
	fi

	AC_SUBST(ODBC_CFLAGS)
	AC_SUBST(ODBC_LIBS)
	AC_SUBST(ODBC_LDFLAGS)
	OPTDIRS="$OPTDIRS odbc"
fi

dnl Conditionally build odbc wide version
AM_CONDITIONAL(ICONV, test "$am_cv_func_iconv" = "yes")

dnl Testing presence of pkg-config
AC_MSG_CHECKING([pkg-config m4 macros])
if test m4_ifdef([PKG_CHECK_MODULES], [yes], [no]) = yes; then
	AC_MSG_RESULT([yes]);
else
	AC_MSG_RESULT([no]);
	AC_MSG_ERROR([
pkg-config is required.
See pkg-config.freedesktop.org])
fi

dnl See if GLib is present and wanted
AC_ARG_ENABLE(glib,
    AS_HELP_STRING([--disable-glib], [do not link with GLib]),
        [enable_glib=$enableval], [enable_glib=yes])

if test "$enable_glib" = "yes"; then
    GLIB_PACKAGE=glib-2.0
    PKG_CHECK_MODULES([GLIB], [$GLIB_PACKAGE], HAVE_GLIB=true, HAVE_GLIB=false)
    if test "x$HAVE_GLIB" = "xtrue"; then
        GLIB_CFLAGS="$GLIB_CFLAGS -DHAVE_GLIB=1"
        AC_SUBST(GLIB_PACKAGE)
    else
        enable_glib=no
    fi
fi
AM_CONDITIONAL(FAKE_GLIB, test "x$enable_glib" != "xyes")

AC_SUBST([OPTDIRS])
AC_CONFIG_FILES([src/Makefile])

##################################################
# Check for txt2man
##################################################

AC_ARG_ENABLE(man,
  AS_HELP_STRING([--disable-man], [disable man generation]),
  enable_man="$enableval", [enable_man=yes])
if test "$enable_man" = yes; then
  if ! which gawk > /dev/null; then
    AC_MSG_ERROR([Could not find GNU awk. Install it or configure with --disable-man if you are not interested in manuals.])
  fi
fi
AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" = yes)

##################################################
# Check for bash-completion.
##################################################

AC_ARG_WITH([bash-completion-dir],
  AS_HELP_STRING([--with-bash-completion-dir[=PATH]],
  [Install the bash auto-completion script in this directory. @<:@default=yes@:>@]),
  [],
  [with_bash_completion_dir=yes])

if test "x$with_bash_completion_dir" = "xyes"; then
  PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
    [BASH_COMPLETION_DIR="`pkg-config --variable=completionsdir bash-completion`"],
    [BASH_COMPLETION_DIR="$datadir/bash-completion/completions"])
else
  BASH_COMPLETION_DIR="$with_bash_completion_dir"
fi

AC_SUBST([BASH_COMPLETION_DIR])
AM_CONDITIONAL([ENABLE_BASH_COMPLETION],[test "x$with_bash_completion_dir" != "xno"])

AC_CONFIG_FILES([
libmdb.pc
libmdbsql.pc
src/util/Makefile
src/util/bash-completion/Makefile
src/extras/Makefile
Makefile
include/Makefile
src/libmdb/Makefile
src/sql/Makefile
src/odbc/Makefile
src/fuzz/Makefile
doc/Makefile
include/mdbver.h])
AC_OUTPUT

##################################################
# Print summary
##################################################


bold_red=$(tput bold 2>/dev/null)$(tput setf 4 2>/dev/null)
bold_green=$(tput bold 2>/dev/null)$(tput setf 2 2>/dev/null)
bold=$(tput bold 2>/dev/null)
reset=$(tput sgr0 2>/dev/null)
AC_MSG_NOTICE([])
AC_MSG_NOTICE([${bold}MDB Tools $VERSION - Configuration summary${reset}])
AC_MSG_NOTICE([])
if test x$sql = xtrue; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   SQL             : ${summary}${reset}])
if test x$HAVE_ODBC = xtrue; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   ODBC            : ${summary}${reset}])
if test x$enable_glib = xyes; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   GLib            : ${summary}${reset}])
if test x$enable_man = xyes; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   man pages       : ${summary}${reset}])
if test "x$with_bash_completion_dir" != "xno"; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   bash-completion : ${summary}${reset}])
AC_MSG_NOTICE([])
