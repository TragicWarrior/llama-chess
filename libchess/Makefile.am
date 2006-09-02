noinst_LTLIBRARIES = libchess.la
libchess_la_SOURCES = common.h pgn.c pgn.h chess.h debug.c debug.h move.c \
		      move.h
libchess_la_LDFLAGS = -avoid-version

if NO_STRSEP
libchess_la_SOURCES += $(top_builddir)/../osdep/strsep.c
endif

if NO_ERR_H
libchess_la_SOURCES += $(top_builddir)/../osdep/err.c
CPPFLAGS += -I$(top_builddir)/../osdep
endif
