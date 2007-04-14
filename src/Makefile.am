bin_PROGRAMS = cboard
cboard_SOURCES = cboard.c message.c message.h cboard.h misc.c engine.c \
		 engine.h input.c input.h rcfile.c rcfile.h colors.c \
		 colors.h strings.h window.h window.c conf.h misc.h common.h \
		 menu.c menu.h tags.c tags.h keys.h filebrowser.c filebrowser.h

LDADD = $(top_builddir)/libchess/libchess.a
CPPFLAGS += -DDATA_PATH=\"$(pkgdatadir)\" -I$(top_srcdir)/libchess

if WITH_DEBUG
CPPFLAGS += -DDEBUG
endif

if NO_STRSEP
cboard_SOURCES += $(top_srcdir)/osdep/strsep.c
endif

if NO_ERR_H
cboard_SOURCES += $(top_srcdir)/osdep/err.c
CPPFLAGS += -I$(top_srcdir)/osdep
endif
