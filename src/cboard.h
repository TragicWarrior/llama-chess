/* $Id: cboard.h,v 1.49 2003-01-27 16:55:16 bjk Exp $ */
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

#define COPYRIGHT	"Copyright (C) 2002-2003 " PACKAGE_BUGREPORT
#define LINE_GRAPHIC(c)	((!config.linegraphics) ? ' ' : c)
#define ROWTOMATRIX(r)	((8 - r) * 2 + 2 - 1)
#define COLTOMATRIX(c)	((c == 1) ? 1 : c * 4 - 3)
#define BOARD_HEIGHT	18
#define BOARD_WIDTH	34
#define STATUS_HEIGHT	(LINES - TAG_HEIGHT)
#define STATUS_WIDTH	(COLS - BOARD_WIDTH)
#define TAG_HEIGHT	10
#define TAG_WIDTH	(COLS - BOARD_WIDTH)
#define HISTORY_HEIGHT	(LINES - BOARD_HEIGHT)
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
int gactive;
char **agony;

const char *mainhelp[] = {
    "   UP - cursor up/hist jump*    R - refresh screen",
    " DOWN - cursor down/hist jump*  b - cycle through book modes",
    " LEFT - cursor left/hist rev*   c - send a command to the game engine",
    "RIGHT - cursor right/hist fwd*  w - switch playing sides",
    " 0..9 - command repeat count    u - undo previous move*",
    "SPACE - select piece            g - force engine to make next move",
    "ENTER - commit selected piece   h - toggle history mode",
    "  ESC - cancel selected piece   a - annotate previous move",
    "                                ] - view the next moves annotation",
    "    n - new game or round       [ - view the previous moves annotation",
    "    N - new game from scratch   i - view PGN roster tags",
    "    > - next game or round*     j - jump to move number*",
    "    < - previous game or round* q - quit",
    "    J - jump to game or round*",
    "    d - toggle game delete flag",
    "    D - delete flagged games",
    " ",
    "    r - resume a saved game",
    "    s - save game",
    "    S - save game with prompt   * = can take a repeat count",
    NULL
};

pid_t init_chess_engine(void);
int parse_pgn_file(BOARD, const char *);
void update_history(void);
void reset_history(void);
struct tags *edit_tags(int);
void parse_engine_output(BOARD, char *);
char *real_filename(char *);
void send_to_engine(const char *, ...);
int get_history_by_index(int, struct history *);
void history_next(BOARD, int);
void history_previous(BOARD, int);
void init_history(BOARD);
void parse_rcfile(const char *);
char *history_edit_nag(void *);
void view_annotation(int);
int save_pgn(const char *, int);
void set_engine_defaults(void);
int start_chess_engine(void);
void stop_engine(void);
void help(const char *, const char **);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void set_default_colors(void);
void init_color_pairs(void);
char *browse_directory(void *);
char *a2a4tosan(BOARD, char *);
int add_tag(struct tags **, int *, const char *, const char *);
void new_game(BOARD);
void *Malloc(size_t);
int isinteger(const char *);
int parse_ics_output(char *);
char *compression_cmd(const char *, int);
int piece_to_int(int);
int int_to_piece(int);
void free_tag_data(struct tags *, int);
void free_historydata(struct history *, int);
void get_valid_moves(BOARD, int, int, int, int *, int *, int *, int *);
void reset_valid_moves(BOARD);
int parse_move_text(BOARD, char *, int);
void parse_history_move(BOARD, int);
void switch_turn(void);

#endif
