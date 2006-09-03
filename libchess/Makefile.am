EXTRA_DIST = debug.c debug.h
noinst_LTLIBRARIES = libchess.la
libchess_la_SOURCES = common.h pgn.c pgn.h chess.h move.c move.h
libchess_la_LDFLAGS = -avoid-version

if WITH_DEBUG
libchess_la_SOURCES += $(top_builddir)/debug.c
CPPFLAGS += -DDEBUG
endif

if NO_STRSEP
libchess_la_SOURCES += $(top_builddir)/../osdep/strsep.c
endif

if NO_ERR_H
libchess_la_SOURCES += $(top_builddir)/../osdep/err.c
CPPFLAGS += -I$(top_builddir)/../osdep
endif
