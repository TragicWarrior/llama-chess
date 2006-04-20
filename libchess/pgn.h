/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2006 Ben Kibbey <bjk@luxsci.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#ifndef PGN_H
#define PGN_H

#include <time.h>

#ifdef __linux__
extern char *strptime(const char *, const char *, struct tm *);
#endif

struct {
    int mpl;
    int stop;
    int reduced;
    int fmd;
} pgn_config;

BOARD pgn_board;
int done_fen_tag;
RAV *rav;
int ravlevel;
int validate;
int nulltags;
int tag_section;
int pgn_ret;
int pgn_write_turn;
int pgn_mpl;
int pgn_lastc;
int pgn_isfile;
int pgn_count;
int pgn_fen_tag;

#endif
