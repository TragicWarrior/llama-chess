/* $Id: cboard.h,v 1.63 2003-02-05 16:21:44 bjk Exp $ */
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
#ifndef CBOARD_H
#define CBOARD_H

#define COPYRIGHT	"Copyright (C) 2002-2003 " ## PACKAGE_BUGREPORT
#define LINE_GRAPHIC(c)	((!config.linegraphics) ? ' ' : c)
#define ROWTOMATRIX(r)	((8 - r) * 2 + 2 - 1)
#define COLTOMATRIX(c)	((c == 1) ? 1 : c * 4 - 3)
#define BOARD_HEIGHT	18
#define BOARD_WIDTH	34
#define STATUS_HEIGHT	(BOARD_HEIGHT + HISTORY_HEIGHT - TAG_HEIGHT)
#define STATUS_WIDTH	(COLS - BOARD_WIDTH)
#define TAG_HEIGHT	10
#define TAG_WIDTH	(COLS - BOARD_WIDTH)
#define HISTORY_HEIGHT	6
#define HISTORY_WIDTH	(COLS - STATUS_WIDTH)

enum {
    UP, DOWN, LEFT, RIGHT
};

/* The order must match the BOOK_... enumeration on common.h. */
const char *book_methods[] = {
    BOOK_OFF_STR, BOOK_PREFER_STR, BOOK_BEST_STR, BOOK_WORST_STR,
    BOOK_RANDOM_STR
};

WINDOW *boardw;
PANEL *boardp;
WINDOW *tagw;
PANEL *tagp;
WINDOW *statusw;
PANEL *statusp;
WINDOW *historyw;
PANEL *historyp;

BOARD board;
int quit;
char **agony;

const char *cmdlinehelp[] = {
    "Usage: cboard [-hv] [-pf <file>] [-i hostname[:port]] "
	"[-u username[:passwd]]\n",
    "  -p  Load PGN file.\n",
    "  -f  Load FEN file.\n",
    "  -i  ICS hostname and optional port.\n",
    "  -u  ICS username and optional password.\n",
    "  -v  Version information.\n",
    "  -h  This help text.\n",
    NULL
};

const char *historyhelp[] = {
    "   UP/DOWN - next or previous history with jump count *",
    "RIGHT/LEFT - next or previous history *",
    "     SPACE - toggle half move stepping",
    "         j - jump to move number *",
    "         / - specify a new move text search expression *",
    "         ] - find the next move text expression *",
    "         [ - find the previous move text expression *",
    "         a - edit comments for the previous move",
    "         v - view comments for the next move",
    "         V - view comments for the previous move",
    "         h - toggle history mode",
    NULL
};

const char *mainhelp[] = {
    "p - play mode keys",
    "h - history mode keys",
    "e - board edit mode keys",
    "g - other game keys",
    NULL
};

const char *edithelp[] = {
    "             0...9 - cursor repeat count",
    "UP/DOWN/LEFT/RIGHT - position cursor *",
    "            !-*A-H - position cursor at rank or file",
    "             SPACE - select piece under cursor for movement",
    "             ENTER - commit selected piece",
    "            ESCAPE - cancel selected piece",
    "                 x - delete the piece under the cursor",
    "                 I - insert a new piece",
    "                 e - toggle board edit mode",
    NULL,
};

const char *gamehelp[] = {
    " 0...9 - command repeat count",
    "     t - edit the current games roster tags",
    "     i - view the current games roster tags",
    "     ? - specify a new roster tag expression *",
    "     } - find the next roster tag expression *",
    "     { - find the previous roster tag expression *",
    "     n - start new game or round",
    "     N - start new game from scratch resetting all other games",
    "     > - next game or round *",
    "     < - previous game or round *",
    "     J - jump to game or round *",
    "     x - toggle game delete flag *",
    "     X - delete the current or all flagged games",
    "     r - resume a saved game",
    "     s - save game",
    "     S - save game and prompt",
    "     q - quit",
    NULL
};

const char *playhelp[] = {
    "             0...9 - cursor repeat count",
    "UP/DOWN/LEFT/RIGHT - position cursor *",
    "            !-*A-H - position cursor at rank or file",
    "             SPACE - select piece under cursor for movement",
    "             ENTER - commit selected piece",
    "            ESCAPE - cancel selected piece",
    "                 + - set engine depth level *",
    "                 b - cycle through book modes",
    "                 w - switch playing side",
    "                 u - undo previous move *",
    "                 g - force engine to make the next move",
    "                 c - send a command to the chess engine",
    NULL
};

pid_t init_chess_engine(void);
int parse_pgn_file(BOARD, const char *);
void update_history(void);
void reset_history(void);
TAG *edit_tags(BOARD, TAG *, int, int);
void parse_engine_output(BOARD, char *);
char *real_filename(char *);
void send_to_engine(const char *, ...);
int get_history_by_index(int, HISTORY *);
void history_next(BOARD, int);
void history_previous(BOARD, int);
void init_history(BOARD);
void parse_rcfile(const char *);
char *history_edit_nag(void *);
void view_annotation(int);
int save_pgn(const char *, int, int);
void set_engine_defaults(void);
int start_chess_engine(void);
void stop_engine(void);
int help(const char *, const char *, const char **);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void set_default_colors(void);
void init_color_pairs(void);
char *browse_directory(void *);
char *a2a4tosan(BOARD, char *);
int add_tag(TAG **, int *, const char *, const char *);
void new_game(BOARD);
void *Malloc(size_t);
int isinteger(const char *);
int parse_ics_output(char *);
char *compression_cmd(const char *, int);
int piece_to_int(int);
int int_to_piece(int);
void free_tag_data(TAG *, int);
void free_historydata(HISTORY **, int, int);
void get_valid_moves(BOARD, int, int, int, int *, int *, int *, int *);
void reset_valid_moves(BOARD);
int parse_move_text(BOARD, char *);
void parse_history_move(BOARD, int);
void switch_turn(void);
char *str_etc(const char *, int, int);
char *tilde_expand(char *);
int parse_fen_file(BOARD, const char *);
char *board_to_fen(BOARD, GAME);

#endif
