dnl $Id: configure.ac,v 1.3 2002-12-09 18:50:22 bjk Exp $
dnl Process this file with autoconf to produce a configure script.
AC_INIT(cboard, 0.1.1, [Ben Kibbey <bjk@arbornet.org>])
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
AC_CHECK_LIB(ncurses, initscr)
AC_CHECK_LIB(panel, new_panel)
AC_CHECK_LIB(form, new_form)
AC_CHECK_LIB(menu, new_menu)

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([limits.h sys/time.h unistd.h stdlib.h string.h ncurses.h \
		 panel.h form.h menu.h getopt.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME

dnl Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([bzero strdup strerror strrchr vasprintf])

AM_WITH_DMALLOC

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
