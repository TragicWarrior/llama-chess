dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT(libchess, 1.0-git, [Ben Kibbey bjk@luxsci.net])
AC_CONFIG_AUX_DIR(build)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign])
AC_PROG_MAKE_SET
AC_DISABLE_SHARED
AC_PROG_LIBTOOL
AC_CONFIG_SRCDIR([pgn.c])
AM_CONFIG_HEADER([config.h])

case "$target_os" in
    darwin*)
	LDFLAGS="$LDFLAGS -flat_namespace -L/sw/lib"
	CPPFLAGS="$CPPFLAGS -I/sw/include -L/sw/lib"
        ;;
    *)
        ;;
esac

CFLAGS="$CFLAGS -D_GNU_SOURCE"

dnl Checks for programs.
AC_PROG_CC

dnl Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h limits.h stdlib.h string.h unistd.h])
AC_CHECK_HEADERS([err.h],, [AC_CHECK_FUNCS([fputs_unlocked putwc_unlocked \
		 putw_unlocked])])

AM_CONDITIONAL([NO_ERR_H], [test "x$ac_cv_header_err_h" = "xno"])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([bzero isascii memset strcasecmp strdup strsep getcwd])

AM_CONDITIONAL([NO_STRSEP], [test "x$ac_cv_func_strsep" = "xno"])

AC_DEFUN([AC_DEBUG],
[
    if test "$1"; then
	ac_cv_sys_debug=$1
    fi

    AC_CACHE_CHECK([if debugging is wanted], [ac_cv_sys_debug],
	[ac_cv_sys_debug=no])
    AM_CONDITIONAL([WITH_DEBUG], [test "x$ac_cv_sys_debug" = "xyes"])
])

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [Enable debugging.]),
    AC_DEBUG([$enableval]), AC_DEBUG)

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
