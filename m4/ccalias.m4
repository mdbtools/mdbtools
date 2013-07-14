dnl Check whether the target supports symbol aliases.
dnl This is a mdbtools specific version
dnl TODO: Check if gnulib version works, and add serial

dnl Code copied from libgomp
AC_DEFUN([AM_GCC_ATTRIBUTE_ALIAS], [
  AC_CACHE_CHECK([whether the target supports symbol aliases],
		 am_cv_gcc_have_attribute_alias, [
  AC_LINK_IFELSE([AC_LANG_PROGRAM([
void foo(void) { }
extern void bar(void) __attribute__((alias("foo")));],
    [bar();])], am_cv_gcc_have_attribute_alias=yes, am_cv_gcc_have_attribute_alias=no)])
  if test $am_cv_gcc_have_attribute_alias = yes; then
    AC_DEFINE(HAVE_ATTRIBUTE_ALIAS, 1,
      [Define to 1 if the target supports __attribute__((alias(...))).])
  fi])
