/* $Id: cboard.h,v 1.33 2003-01-06 20:16:15 bjk Exp $ */
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
#ifndef CBOARD_H
#define CBOARD_H

#define COPYRIGHT	"Copyright (c) 2002 " PACKAGE_BUGREPORT
#define BOARD_HEIGHT	18
#define BOARD_WIDTH	34
#define STATUS_HEIGHT	(10)
#define STATUS_WIDTH	(COLS - BOARD_WIDTH)
#define BW_HEIGHT	(LINES - STATUS_HEIGHT) / 2
#define BW_WIDTH	(COLS - BOARD_WIDTH)
#define HISTORY_HEIGHT	(LINES - BOARD_HEIGHT)
#define HISTORY_WIDTH	(COLS - BW_WIDTH)
#define HISTORY_TITLE	"Move History"
#define ENGINE_COMMAND_PROMPT	"Engine Command"
#define STATUS_TITLE	"Game Status"
#define MAIN_HELP	"Command Keys"
#define MAIN_HELP_PROMPT	"Type CTRL-g for available command keys"
#define ANNOTATE_HISTORY	"Editing Annotation for"
#define NAG_PROMPT	"Type CTRL-t to edit NAG"
#define EXTRA_BROWSE	"Type TAB for file browser"
#define LOAD_PGN	"Load PGN filename"
#define SAVE_PGN	"Save to PGN filename"

/* The order must match the BOOK_... enumeration on common.h. */
const char *book_methods[] = {
    "off", "prefer", "best", "worst", "random"
};

const char *mainhelp[] = {
    "   UP/j - cursor up/hist jump     R - refresh screen",
    " DOWN/k - cursor down/hist jump   b - cycle through book modes",
    " LEFT/l - cursor left/hist rev    c - send a command to the game engine",
    "RIGHT/; - cursor right/hist fwd   w - switch sides",
    "                                  u - undo previous move",
    "  SPACE - select piece            g - force engine to make next move",
    "  ENTER - commit selected piece   h - toggle history mode",
    "    ESC - cancel selected piece   a - annotate previous move",
    "                                  ] - view the next moves annotation",
    "      N - new game                [ - view the previous moves annotation",
    "      r - resume a saved game",
    "      i - show PGN roster tags",
    "      s - save game",
    "      > - next game or round",
    "      < - previous game or round",
    " ",
    "      q - quit",
    NULL
};

WINDOW *boardw;
PANEL *boardp;
WINDOW *whitew, *blackw;
PANEL *whitep, *blackp;
WINDOW *statusw;
PANEL *statusp;

int selected_y, selected_x;
int quit;
int gactive;
char **agony;

pid_t init_chess_engine(void);
int parse_pgn_file(const char *);
void update_history(void);
void reset_history(void);
struct pgndata *edit_pgn_data(int);
void parse_engine_output(char *);
char *real_filename(char *);
void send_to_engine(const char *, ...);
int get_history_by_index(int, struct history *);
void history_next(int);
void history_previous(int);
void init_history(void);
void parse_rcfile(const char *);
char *history_edit_nag(void *);
void view_annotation(int);
int save_pgn(const char *, struct pgndata *, int);
void set_engine_defaults(void);
int start_chess_engine(void);
void stop_engine(void);
void set_pgn_defaults(void);
void help(const char *, const char **);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void set_default_colors(void);
void init_color_pairs(void);
char *browse_directory(void *);

#endif
