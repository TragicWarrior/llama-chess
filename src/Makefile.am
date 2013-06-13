bin_PROGRAMS = cboard
cboard_SOURCES = cboard.c message.c message.h misc.c engine.c \
		 engine.h input.c input.h rcfile.c rcfile.h colors.c \
		 colors.h window.h window.c conf.h misc.h common.h \
		 menu.c menu.h tags.c keys.h filebrowser.c filebrowser.h
cboard_LDFLAGS = @PANEL_LIB@ @MENU_LIB@ @FORM_LIB@ @CURSES_LIB@
cboard_LDADD = $(top_builddir)/libchess/libchess.a
cboard_CPPFLAGS = -DDATA_PATH=\"$(pkgdatadir)\" -I$(top_srcdir)/libchess \
	          -DLOCALE_DIR=\"$(prefix)/share/locale\"

if WITH_LIBPERL
cboard_SOURCES += perl-plugin.c perl-plugin.h
cboard_CFLAGS = @LIBPERL_CFLAGS@
cboard_LDFLAGS += @LIBPERL_LDFLAGS@
endif

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
