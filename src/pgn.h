/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2006 Ben Kibbey <bjk@arbornet.org>

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

#ifdef __linux__
extern char *strptime(const char *, const char *, struct tm *);
#endif

#define VALIDCOL(c)	((c >= 'a' && c <= 'h') ? 1 : 0)
#define BROWSE_HEIGHT	12
#define MAX_TIME_LEN	18
#define TIME_FORMAT	"%B %d, %Y" /* When displayed in-game. */
#define PGN_TIME_FORMAT	"%Y.%m.%d"
#define MAX_VALUE_WIDTH	(COLS - 8)

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
    "     CTRL-f - add FEN tag from current position",
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

BOARD pgnboard;

void *Malloc(size_t);
void add_to_history(HISTORY **, int *, int *, const char *);
void reset_history(void);
void free_game_data(void);
void send_to_engine(const char *, ...);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);
void help(const char *, const char *, const char **);
char *tilde_expand(char *);
char *real_filename(char *);
int parse_move_text(BOARD, char *);
char *a2a4tosan(BOARD, char *);
int integer_len(int);
void switch_turn(void);
FILE *open_file(const char *, int *);
char *compression_cmd(const char *, int);
int parse_fen_line(BOARD, char *);
char *board_to_fen(BOARD, GAME);
void copy_board(BOARD, BOARD);

#endif
