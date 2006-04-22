dnl $Id: configure.ac,v 1.18 2003-02-07 19:27:39 bjk Exp $
dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.59)
AC_INIT(cboard, 0.2.1, [Ben Kibbey bjk@luxsci.net])
AC_CONFIG_AUX_DIR(build)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([foreign])
AC_PROG_MAKE_SET
AC_CONFIG_SRCDIR([src/cboard.c])
AM_CONFIG_HEADER([config.h])
AC_CONFIG_SUBDIRS([libchess])

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
AC_PROG_AWK
AC_PROG_LN_S
AC_PROG_CC
AC_PROG_INSTALL

dnl Checks for library functions.
AC_CHECK_LIB(ncurses, initscr, ,
	     [AC_MSG_ERROR([Missing or incomplete ncurses installation.])])
AC_CHECK_LIB(panel, new_panel, ,
	     [AC_MSG_ERROR([Missing or incomplete ncurses installation.])])
AC_CHECK_LIB(form, new_form, ,
	     [AC_MSG_ERROR([Missing or incomplete ncurses installation.])])

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h paths.h sys/socket.h limits.h sys/time.h unistd.h \
		  stdlib.h string.h ncurses.h panel.h form.h getopt.h \
		  sys/param.h regex.h wordexp.h])

dnl Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_TYPE_SIGNAL
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([bzero dup2 getcwd gethostbyname memset mkdir mkfifo regcomp \
		select socket strcasecmp strdup strerror strrchr strstr \
		vasprintf])

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

AC_MSG_CHECKING([for Unix98 pty support])
if test -c "/dev/ptmx"; then
    have_ptmx=yes
else
    have_ptmx=no
fi

ptmx_works=no

if test "x$have_ptmx" = "xyes"; then
    AC_RUN_IFELSE([\
		   #include <stdio.h>
		   #include <sys/types.h>
		   #include <sys/stat.h>
		   #include <fcntl.h>

		   int main()
		   {
		       int fd;

		       if ((fd = open("/dev/ptmx", O_RDWR)) == -1)
		           exit(1);

		       if (ptsname(fd) == NULL)
		           exit(1);

		       exit(0);
		   }
		   ], [ptmx_works=yes], [ptmx_works=no])

    if test "x$ptmx_works" = "xyes"; then
	AC_MSG_RESULT([yes])
	AC_DEFINE([UNIX98], 1, [Define if your system has Unix98 pty support.])
    else
	AC_MSG_RESULT([no])
    fi
fi

if test "x$have_ptmx" = "xno" -o "x$ptmx_works" = "xno"; then
# From the GNU Screen package.
    AC_MSG_CHECKING([pty ranges])
    if test -d /dev/ptym; then
    pdir='/dev/ptym'
    else
    pdir='/dev'
    fi

    ptys=`echo $pdir/pty??`

    if test "$ptys" != "$pdir/pty??" ; then
    p0=`echo $ptys | tr ' ' '\012' | sed -e 's/^.*\(.\).$/\1/g' | sort -u | tr -d '\012'`
    p1=`echo $ptys | tr ' ' '\012' | sed -e 's/^.*\(.\)$/\1/g'  | sort -u | tr -d '\012'`

    AC_DEFINE_UNQUOTED([PTY_MAJOR], "$p0", [Pseudo tty ranges.])
    AC_DEFINE_UNQUOTED([PTY_MINOR], "$p1", [Pseudo tty ranges.])
    AC_MSG_RESULT($p0 $p1)
    fi
fi

AM_WITH_DMALLOC

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile data/Makefile \
		 test/Makefile])
AC_OUTPUT
