/* $Id: pgn.h,v 1.12 2002-12-21 21:32:17 bjk Exp $ */
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
#ifndef PGN_H
#define PGN_H

#define MAX_TIME_LEN	18
#define TIME_FORMAT	"%B %d, %Y" /* When displayed in-game. */
#define PGN_TIME_FORMAT	"%Y.%m.%d"
#define PGN_EDIT_TITLE	"Editing PGN Save Data"
#define PGN_PROMPT	"Type CTRL-g for help"
#define PGN_INFO_HELP	"PGN Information Keys"
#define PGN_EDIT_HELP	"PGN Edit Keys"
#define PGN_EDIT_TAG	"Editing PGN Roster Tag"
#define PGN_BAD_INDEX	"Could not get window index number"
#define PGN_REMOVE_STR	"Cannot remove the Seven Tag Roster"
#define PGN_NEW_TAG	"New Roster Tag Name"
#define PGN_DUPLICATE	"Could not add duplicate tag"
#define PGN_INFO_TITLE	"PGN Information"
#define OVERWRITE_PROMPT	"'a' to append, 'o' to overwrite"
#define MAX_VALUE_WIDTH	30

enum {PGN_EVENT, PGN_SITE, PGN_DATE, PGN_ROUND, PGN_WHITE, PGN_BLACK,
    PGN_RESULT };

const struct {
    char *pgn;
    char *fancy;
} fancy_results[] = {
    {"1-0", "white wins"},
    {"0-1", "black wins"},
    {"1/2-1/2", "draw"},
    {"*", "undetermined"}
};

const char *pgn_edit_help[] = {
    "    UP/DOWN - select menu entry",
    "[A-Za-z0-9] - jump to entry",
    "      ENTER - edit selected entry",
    "     CTRL-a - add an entry",
    "     CTRL-r - remove an entry",
    "     ESCAPE - quit",
    NULL
};

const char *pgn_info_help[] = {
    "    UP/DOWN - select menu entry",
    "[A-Za-z0-9] - jump to entry",
    "      ENTER - view selected entry",
    "     ESCAPE - quit",
    NULL
};

void add_to_history(struct history **, int *, int *, const char *);
void reset_history(void);
void free_game_data(void);
void send_to_engine(const char *, ...);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);
void help(const char *, const char **);

#endif
