/* $Id: pgn.h,v 1.22 2003-01-14 20:44:15 bjk Exp $ */
/*
    Copyright (C) 2002-2003 Ben Kibbey <bjk@arbornet.org>

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

#define VALIDCOL(c)	((c >= 'a' && c <= 'h') ? 1 : 0)
#define BROWSE_HEIGHT	12
#define MAX_TIME_LEN	18
#define TIME_FORMAT	"%B %d, %Y" /* When displayed in-game. */
#define PGN_TIME_FORMAT	"%Y.%m.%d"
#define PGN_EDIT_TITLE	"Editing PGN Save Data"
#define HELP_PROMPT	"Type CTRL-g for help"
#define CC_PROMPT	"Type CTRL-t for country codes"
#define CC_TITLE	"Country Codes"
#define CC_KEY_HELP	"Country Code Keys"
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
#define BROWSER_HELP	"File Browser Keys"
#define CHANGE_DIRECTORY	"Change Directory"

const struct {
    char *pgn;
    char *fancy;
} fancy_results[] = {
    {"1-0", "white wins"},
    {"0-1", "black wins"},
    {"1/2-1/2", "draw"},
    {"*", "undetermined"}
};

struct country_codes {
    char code[4];
    char country[64];
} *ccodes;

struct d_entries {
    char *name;
    char *fancy;
    char desc[25];
};

const char *pgn_edit_help[] = {
    "    UP/DOWN - select menu entry",
    "       HOME - first entry",
    "        END - last entry",
    "CTRL-n/PGDN - next page",
    "CTRL-p/PGUP - previous page",
    "  a-zA-Z0-9 - jump to entry",
    "      ENTER - edit selected entry",
    "     CTRL-a - add an entry",
    "     CTRL-r - remove selected entry",
    "     ESCAPE - quit",
    NULL
};

const char *pgn_info_help[] = {
    "    UP/DOWN - select menu entry",
    "       HOME - first entry",
    "        END - last entry",
    "CTRL-n/PGDN - next page",
    "CTRL-p/PGUP - previous page",
    "  a-zA-Z0-9 - jump to entry",
    "      ENTER - view selected entry",
    "     ESCAPE - quit",
    NULL
};

const char *cc_help[] = {
    "    UP/DOWN - previous/next menu entry",
    "       HOME - first entry",
    "        END - last entry",
    "CTRL-n/PGDN - next page",
    "CTRL-p/PGUP - previous page",
    "  a-zA-Z0-9 - jump to entry",
    "      ENTER - selected entry",
    "     ESCAPE - cancel",
    NULL
};

const char *file_browser_help[] = {
    "    UP/DOWN - select menu entry",
    "       HOME - first entry",
    "        END - last entry",
    "CTRL-n/PGDN - next page",
    "CTRL-p/PGUP - previous page",
    "  a-zA-Z0-9 - jump to entry",
    "     CTRL-x - change directory",
    "          ~ - change to home directory",
    "      ENTER - commit selected entry",
    "     ESCAPE - quit",
    NULL
};

struct board_matrix pgnboard[8][8];

void *Malloc(size_t);
void add_to_history(struct history **, int *, int *, const char *);
void reset_history(void);
void free_game_data(void);
void send_to_engine(const char *, ...);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);
void help(const char *, const char **);
char *tilde_expand(char *);
char *real_filename(char *);
int parse_move_text(struct board_matrix[][], char *, int);
char *a2a4tosan(struct board_matrix [][], char *);

#endif
