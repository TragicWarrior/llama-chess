bin_PROGRAMS = cboard
cboard_SOURCES = cboard.c message.c message.h cboard.h misc.c engine.c \
		 engine.h input.c input.h help.c rcfile.c rcfile.h colors.c \
		 colors.h strings.h debug.c window.h conf.h debug.h misc.h \
		 common.h

LDADD = $(top_builddir)/libchess/.libs/libchess.a
CPPFLAGS = -DDATA_PATH=\"$(pkgdatadir)\" -I$(top_builddir)/libchess @CPPFLAGS@
