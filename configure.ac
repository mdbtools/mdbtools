dnl Process this file with autoconf to produce a configure script.
AC_INIT([mdbtools],[0.8.2],[mdbtools-dev@lists.sourceforge.net])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_SRCDIR(src/extras/mdb-dump.c)
AM_INIT_AUTOMAKE

AM_MAINTAINER_MODE([enable])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

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
AC_CHECK_HEADERS(fcntl.h limits.h unistd.h)
AC_CHECK_HEADERS(wordexp.h)
AC_CHECK_LIB(mswstr, DBLCMapStringW)
AC_CHECK_DECLS([program_invocation_short_name], [], [], [[
                #define _GNU_SOURCE
                #include <errno.h>]])

dnl Checks for library functions.
VL_LIB_READLINE
AC_CHECK_FUNC(strptime,[ AC_DEFINE(HAVE_STRPTIME, 1, [strptime check]) AM_CONDITIONAL(HAVE_STRPTIME, true) ],[ AM_CONDITIONAL(HAVE_STRPTIME, false) ])
AC_CHECK_FUNC(fmemopen,[ AC_DEFINE(HAVE_FMEMOPEN, 1, [fmemopen check]) ])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

AM_ICONV
AM_GCC_ATTRIBUTE_ALIAS

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
sql=false
fi

if test "x$sql" = "xtrue"; then
	CFLAGS="$CFLAGS -DSQL"
	OPTDIRS="$OPTDIRS sql"
else
	AC_MSG_WARN([Bison 3.0+ is not available: SQL disabled.])
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
		echo
		echo ODBC requires flex and bison for the SQL engine
		exit 1
	fi

	AC_SUBST(ODBC_CFLAGS)
	AC_SUBST(ODBC_LIBS)
	AC_SUBST(ODBC_LDFLAGS)
	OPTDIRS="$OPTDIRS odbc"
fi

dnl Turn on -Wstrict-prototypes here because AC_CHECK_LIB is dumb
CFLAGS="$CFLAGS -Wstrict-prototypes"

dnl Conditionally build odbc wide version
AM_CONDITIONAL(ICONV, test "$am_cv_func_iconv" = "yes")

dnl Testing presence of pkg-config
AC_MSG_CHECKING([pkg-config m4 macros])
if test m4_ifdef([PKG_CHECK_MODULES], [yes], [no]) == yes; then
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
    PKG_CHECK_MODULES([GLIB], [glib-2.0], HAVE_GLIB=true, HAVE_GLIB=false)
    if test "x$HAVE_GLIB" = "xtrue"; then
        GLIB_CFLAGS="$GLIB_CFLAGS -DHAVE_GLIB=1"
    else
        enable_glib=no
    fi
fi
AM_CONDITIONAL(FAKE_GLIB, test "x$enable_glib" != "xyes")

dnl check for gtk/gnome
PKG_CHECK_MODULES([GNOME], [gtk+-2.0 >= 2.14 libglade-2.0 libgnomeui-2.0], HAVE_GNOME=true, HAVE_GNOME=false)

GNOME_DOC_INIT
AC_ARG_ENABLE(gmdb2,
        AS_HELP_STRING([--disable-gmdb2],[do not build gmdb2]),
        [build_gmdb2=$enableval], [build_gmdb2=yes])

if test "$build_gmdb2" = "yes" ; then
	if test "x$HAVE_GNOME" = "xtrue"; then
		AC_SUBST(GNOME_CFLAGS)
		AC_SUBST(GNOME_LIBS)
		OPTDIRS="$OPTDIRS gmdb2"
	fi
else
        AC_MSG_NOTICE(gmdb2 disabled)
	build_gmdb2=no
fi

AC_SUBST([OPTDIRS])
AC_CONFIG_FILES([src/Makefile])


##################################################
# Check for gtk-doc.
##################################################

AC_ARG_WITH(html-dir,
  AS_HELP_STRING([--with-html-dir=PATH], [path to installed docs]))

if test "x$with_html_dir" = "x" ; then
  HTML_DIR='${datadir}/gtk-doc/html'
else
  HTML_DIR=$with_html_dir
fi

AC_SUBST(HTML_DIR)

gtk_doc_min_version=1.0
AC_MSG_CHECKING([gtk-doc version >= $gtk_doc_min_version])
if pkg-config --atleast-version=$gtk_doc_min_version gtk-doc; then
  AC_MSG_RESULT(yes)
  GTKDOC=true
else
  AC_MSG_RESULT(no)
  GTKDOC=false
fi

dnl Let people disable the gtk-doc stuff.
AC_ARG_ENABLE(gtk-doc, AS_HELP_STRING([--enable-gtk-doc], [use gtk-doc to build documentation [default=auto]]), enable_gtk_doc="$enableval", enable_gtk_doc=auto)

if test x$enable_gtk_doc = xauto ; then
  if test x$GTKDOC = xtrue ; then
    enable_gtk_doc=yes
  else
    enable_gtk_doc=no 
  fi
fi

AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)

##################################################
# Check for txt2man
##################################################

AC_ARG_ENABLE(man,
  AS_HELP_STRING([--disable-man], [disable man generation]),
  enable_man="$enableval", [enable_man=yes])
if test "$enable_man" = yes; then
  if ! which txt2man > /dev/null; then
    AC_MSG_ERROR([Could not find txt2man script. Install it or configure with --disable-man if you are not interrested in manuals.])
  fi
fi
AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" = yes)

##################################################
# Check for docbook
##################################################
AM_CONDITIONAL(ENABLE_DOCBOOK, test -n "$DOCBOOK_DSL")

localedir=${datadir}/locale
AC_SUBST(localedir)

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
src/gmdb2/Makefile
src/gmdb2/gladefiles/Makefile
src/gmdb2/pixmaps/Makefile
src/gmdb2/help/Makefile
mdbtools.spec
include/mdbver.h])
AC_OUTPUT
dnl doc/reference/libmdb/Makefile])

##################################################
# Print summary
##################################################


bold_red=$(tput bold)$(tput setf 4)
bold_green=$(tput bold)$(tput setf 2)
bold=$(tput bold)
reset=$(tput sgr0)
AC_MSG_NOTICE([])
AC_MSG_NOTICE([${bold}MDB Tools $VERSION - Configuration summary${reset}])
AC_MSG_NOTICE([])
if test x$sql = xtrue; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   SQL          : ${summary}${reset}])
if test x$HAVE_ODBC = xtrue; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   ODBC         : ${summary}${reset}])
if test x$enable_glib = xyes; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   GLib         : ${summary}${reset}])
if test x$build_gmdb2 = xyes; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   UI           : ${summary}${reset}])
if test x$enable_gtk_doc = xyes; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   Gtk doc      : ${summary}${reset}])
if test x$enable_man = xyes; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   man          : ${summary}${reset}])
if test -n "$DOCBOOK_DSL"; then summary=${bold_green}enabled; else summary=${bold_red}disabled; fi
AC_MSG_NOTICE([   html docbook : ${summary}${reset}])
AC_MSG_NOTICE([])
