bin_PROGRAMS = cboard
cboard_SOURCES = cboard.c message.c message.h cboard.h misc.c engine.c \
		 engine.h input.c input.h rcfile.c rcfile.h colors.c \
		 colors.h strings.h window.h window.c conf.h misc.h common.h \
		 menu.c menu.h tags.c tags.h

LDADD = $(top_builddir)/libchess/.libs/libchess.a
CPPFLAGS = -DDATA_PATH=\"$(pkgdatadir)\" -I$(top_builddir)/libchess @CPPFLAGS@
