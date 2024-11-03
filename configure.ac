dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.60)
AC_INIT(cboard, 0.7.6-dev, [Ben Kibbey <bjk@luxsci.net>])
AC_CONFIG_AUX_DIR(build)
AC_CANONICAL_TARGET
AM_INIT_AUTOMAKE([subdir-objects foreign dist-bzip2 no-dist-gzip])
AC_USE_SYSTEM_EXTENSIONS()
AC_CONFIG_HEADERS([config.h])
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.19.8])

case "$target_os" in
    darwin*)
	LDFLAGS="$LDFLAGS -flat_namespace -L/sw/lib"
	CPPFLAGS="$CPPFLAGS -I/sw/include -L/sw/lib"
        ;;
    *)
        ;;
esac

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl Checks for library functions.
AX_WITH_CURSES
AX_WITH_CURSES_PANEL
AX_WITH_CURSES_MENU
AX_WITH_CURSES_FORM

AC_ARG_WITH(libperl, AS_HELP_STRING([--with-libperl], [PERL support.]),
    [want_libperl=yes], [want_libperl=no])

AC_MSG_CHECKING([if PERL support is wanted])
AC_MSG_RESULT($want_libperl)

if test "$want_libperl" = "yes"; then
    AC_CHECK_LIB(perl, perl_run, have_libperl=yes, have_libperl=no)

    if test "$have_libperl" = "yes"; then
	LIBPERL_CFLAGS=`perl -MExtUtils::Embed -e ccopts`

	if test $? -ne 0; then
	    AC_MSG_ERROR([Missing or incomplete PERL installation.])
	fi

	AC_SUBST(LIBPERL_CFLAGS)
	LIBPERL_LDFLAGS=`perl -MExtUtils::Embed -e ldopts`
	AC_SUBST(LIBPERL_LDFLAGS)
	AC_DEFINE([WITH_LIBPERL], 1, [Define if you want PERL support.])
    else
	AC_MSG_ERROR([Missing or incomplete PERL installation.])
    fi
fi

AM_CONDITIONAL([WITH_LIBPERL], [test "$have_libperl" = "yes"])
dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h paths.h sys/socket.h limits.h sys/time.h unistd.h \
		  stdlib.h string.h strings.h wchar.h getopt.h sys/param.h \
                  regex.h stdarg.h paths.h])
AC_CHECK_HEADERS([err.h],, [AC_CHECK_FUNCS([fputs_unlocked putwc_unlocked \
		 putw_unlocked])])

AM_CONDITIONAL([NO_ERR_H], [test "x$ac_cv_header_err_h" = "xno"])

if test x"$ac_cv_header_err_h" = "xyes"; then
    AC_MSG_CHECKING([for __progname])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <stdio.h>
#include <err.h>
extern char *__progname;
]], [[
printf("%s\n", __progname);
]])], [have_progname=yes], [have_progname=no])
    AC_MSG_RESULT($have_progname)
    if test x"$have_progname" = "xyes"; then
        AC_DEFINE([HAVE_PROGNAME], 1, \
                  [Define if your system has __progname.])
    fi
fi

AC_CHECK_HEADERS([glob.h])
if test x"$ac_cv_header_glob_h" = "xyes"; then
    AC_MSG_CHECKING([if glob.h defines GLOB_NOMATCH])
    AC_LINK_IFELSE([AC_LANG_PROGRAM([[
#include <glob.h>
]], [[
int n = GLOB_NOMATCH;
]])], [have_glob_nomatch=yes], [have_glob_nomatch=no])
    AC_MSG_RESULT($have_glob_nomatch)
    if test x"$have_glob_nomatch" = "xyes"; then
         AC_DEFINE([HAVE_GLOB_NOMATCH], 1, \
	           [Define if glob.h defines GLOB_NOMATCH.])
    fi
fi

dnl Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl Checks for library functions.
AC_FUNC_ALLOCA
AC_FUNC_MEMCMP
AC_FUNC_VPRINTF
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STAT
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([bzero dup2 getcwd gethostbyname memset mkdir mkfifo regcomp \
		select socket strcasecmp strdup strerror strrchr strstr \
		vasprintf strsep strchr strncasecmp strpbrk putenv mkstemp])

AM_CONDITIONAL([NO_STRSEP], [test "x$ac_cv_func_strsep" = "xno"])

AC_DEFUN([AC_DEBUG],
[
    if test "$1"; then
	ac_cv_sys_debug=$1
    fi

    AC_CACHE_CHECK([if debugging is wanted], [ac_cv_sys_debug],
	[ac_cv_sys_debug=no])
    AM_CONDITIONAL([WITH_DEBUG], [test "$ac_cv_sys_debug" = "yes"])
])

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable debugging.]),
    AC_DEBUG([$enableval]), AC_DEBUG)

AC_MSG_CHECKING([for Unix98 pty support])
if test -c "/dev/ptmx"; then
    have_ptmx=yes
else
    have_ptmx=no
fi

ptmx_works=no

if test "x$have_ptmx" = "xyes"; then
    AC_LANG([C])
    AC_RUN_IFELSE([AC_LANG_SOURCE([[\
		   #include <stdio.h>
		   #include <stdlib.h>
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
		   ]])], [ptmx_works=yes], [ptmx_works=no])

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

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile data/Makefile \
		 libchess/Makefile po/Makefile.in doc/cboard.6])
AC_OUTPUT
