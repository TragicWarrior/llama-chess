dnl $Id: configure.ac,v 1.14 2003-01-28 17:39:42 bjk Exp $
dnl Process this file with autoconf to produce a configure script.
AC_INIT(cboard, 0.1.4, [Ben Kibbey <bjk@arbornet.org>])
AC_CONFIG_AUX_DIR(build)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign])
AC_PROG_MAKE_SET
AC_CONFIG_SRCDIR([src/cboard.c])
AM_CONFIG_HEADER([config.h])

case "$target_os" in
    darwin*)
	LDFLAGS="$LDFLAGS -flat_namespace -L/sw/lib"
	CPPFLAGS="$CPPFLAGS -I/sw/include -L/sw/lib"
        ;;
    *)
        ;;
esac

dnl Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for library functions.
AC_CHECK_LIB(ncurses, initscr, ,
	     [AC_MSG_ERROR([Missing or incomplete ncurses installation.])])
AC_CHECK_LIB(panel, new_panel, ,
	     [AC_MSG_ERROR([Missing or incomplete ncurses installation.])])
AC_CHECK_LIB(form, new_form, ,
	     [AC_MSG_ERROR([Missing or incomplete ncurses installation.])])
AC_CHECK_LIB(menu, new_menu, , 
	     [AC_MSG_ERROR([Missing or incomplete ncurses installation.])])

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h sys/time.h unistd.h stdlib.h string.h ncurses.h \
		 panel.h form.h menu.h getopt.h sys/param.h regex.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero strdup strerror strrchr vasprintf])

AC_DEFINE([MAX_PGN_LINE_LEN], 255, [Maximum line length in a PGN file.])
AC_DEFINE([MAX_PGN_MOVE_LEN], 7, [Maximum SAN move length without terminating NULL.])
AC_DEFINE([MAX_PGN_NAG], 5, [Maximum number of NAG entries per move.])
AC_DEFINE_UNQUOTED([DEFAULT_ICS_SERVER], "localhost", 
		   [Default internet chess server.])
AC_DEFINE([DEFAULT_ICS_PORT], 5000, [Default internet chess server port.])
AC_DEFINE_UNQUOTED([DEFAULT_ICS_USER], "guest",
		   [Default internet chess server port.])

AC_DEFUN([AC_DEBUG],
[
    if test "$1"; then
	ac_cv_sys_debug=$1
    fi

    AC_CACHE_CHECK([if debugging is wanted], [ac_cv_sys_debug],
	[ac_cv_sys_debug=no])

    if test "$ac_cv_sys_debug" = "yes"; then
	CPPFLAGS="$CPPFLAGS -DDEBUG"
    fi
])

AC_ARG_ENABLE(debug, AC_HELP_STRING([--enable-debug], [Enable debugging.]),
    AC_DEBUG([$enableval]), AC_DEBUG)

AM_WITH_DMALLOC

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile etc/Makefile])
AC_OUTPUT
