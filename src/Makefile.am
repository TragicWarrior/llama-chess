# $Id: Makefile.am,v 1.12 2003-01-30 16:51:36 bjk Exp $
bin_PROGRAMS = cboard
cboard_SOURCES = cboard.c message.c message.h common.h cboard.h misc.c \
		 engine.c engine.h input.c input.h help.c pgn.c pgn.h \
		 history.c history.h rcfile.c rcfile.h colors.c colors.h \
		 move.c move.h strings.h ics.c ics.h

CPPFLAGS = -DDATA_PATH=\"$(pkgdatadir)\" @CPPFLAGS@
