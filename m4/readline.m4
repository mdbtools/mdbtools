dnl readline.m4
dnl mdbtools specific version
dnl TODO: Check if official version works then add serial
AC_DEFUN([VL_LIB_READLINE], [
  AC_CACHE_CHECK([for a readline compatible library],
                 vl_cv_lib_readline, [
    ORIG_LIBS="$LIBS"
    for readline_lib in readline edit editline; do
      for termcap_lib in "" termcap curses ncurses; do
        TRY_LIB="-l$readline_lib"
        if test -n "$termcap_lib"; then
          TRY_LIB="$TRY_LIB -l$termcap_lib"
        fi
        LIBS="$ORIG_LIBS $TRY_LIB"
        AC_TRY_LINK_FUNC(readline, vl_cv_lib_readline=yes)
        if test "$vl_cv_lib_readline" = yes; then
          break
        fi
      done
      if test "$vl_cv_lib_readline" = yes; then
        break
      fi
    done
  ])

  if test "$vl_cv_lib_readline" = yes; then
    AC_DEFINE(HAVE_LIBREADLINE, 1,
              [Define if you have a readline compatible library])
    AC_CHECK_HEADERS(readline.h readline/readline.h)
    AC_CACHE_CHECK([whether readline supports history],
                   vl_cv_lib_readline_history, [
      AC_TRY_LINK_FUNC(add_history, vl_cv_lib_readline_history=yes)
    ])
    if test "$vl_cv_lib_readline_history" = yes; then
      AC_DEFINE(HAVE_READLINE_HISTORY, 1,
                [Define if your readline library has \`add_history'])
      AC_CHECK_HEADERS(history.h readline/history.h)
    fi
  fi
  LIBS="$ORIG_LIBS"

  LIBREADLINE=
  if test "$vl_cv_lib_readline" = yes; then
    LIBREADLINE="$TRY_LIB"
  fi
  AC_SUBST(LIBREADLINE)
])dnl
