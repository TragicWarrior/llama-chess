/* $Id: pgn.h,v 1.1.1.1 2002-12-05 20:38:47 bjk Exp $ */
/*
    Copyright (C) 2002 Ben Kibbey <bjk@arbornet.org>

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
#define MAX_TIME_LEN	18
#define TIME_FORMAT	"%B %d, %Y" /* When displayed in-game. */
#define PGN_TIME_FORMAT	"%Y.%m.%d"

struct {
    char *pgn;
    char *fancy;
} fancy_results[] = {
    {"1-0", "white wins"},
    {"0-1", "black wins"},
    {"1/2-1/2", "draw"},
    {"*", "undetermined"}
};
