bin_PROGRAMS = cboard
cboard_SOURCES = cboard.c message.c message.h cboard.h misc.c engine.c \
		 engine.h input.c input.h rcfile.c rcfile.h colors.c \
		 colors.h strings.h window.h window.c conf.h misc.h common.h \
		 menu.c menu.h tags.c tags.h keys.h filebrowser.c \
		 filebrowser.h gettext.h
cboard_CFLAGS = @libncursesw_CFLAGS@ @libformw_CFLAGS@ @libpanelw_CFLAGS@ \
		@libmenuw_CFLAGS@
cboard_LDFLAGS = @libncursesw_LIBS@ @libformw_LIBS@ @libpanelw_LIBS@ \
		 @libmenuw_LIBS@
cboard_LDADD = $(top_builddir)/libchess/libchess.a
CPPFLAGS += -DDATA_PATH=\"$(pkgdatadir)\" -I$(top_srcdir)/libchess

if WITH_LIBPERL
cboard_SOURCES += perl.c perl.h
LIBPERL_CFLAGS += @LIBPERL_CFLAGS@
cboard_LDFLAGS += @LIBPERL_LDFLAGS@

# This is needed because LIBPERL_CFLAGS has form.h in the include path which
# conflicts with the curses form.h which is normally in a standard system
# include path that gcc -I ignores.
perl.o: perl.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LIBPERL_CFLAGS) -c $< -o $@
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
