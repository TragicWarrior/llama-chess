/* $Id: cboard.h,v 1.27 2002-12-20 17:14:08 bjk Exp $ */
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
#define DATA_HEIGHT	(LINES - STATUS_HEIGHT)
#define DATA_WIDTH	(COLS - BOARD_WIDTH)
#define HISTORY_HEIGHT	(LINES - BOARD_HEIGHT)
#define HISTORY_WIDTH	(COLS - DATA_WIDTH)
#define HISTORY_TITLE	"Move History"

#define BOARD_WHITE	((COLORS) ? \
	COLOR_PAIR(1) | config.color[CONF_WHITE].attrs : \
	config.color[CONF_WHITE].nattrs)

#define BOARD_BLACK	((COLORS) ? \
	COLOR_PAIR(2) | config.color[CONF_BLACK].attrs : \
	config.color[CONF_BLACK].nattrs)

#define BOARD_SELECTED	((COLORS) ? \
	COLOR_PAIR(3) | config.color[CONF_SELECTED].attrs : \
	config.color[CONF_SELECTED].nattrs)

#define BOARD_CURSOR	((COLORS) ? \
	COLOR_PAIR(4) | config.color[CONF_CURSOR].attrs : \
	config.color[CONF_CURSOR].nattrs)

#define WINDOW_TITLE	((COLORS) ? \
	COLOR_PAIR(5) | config.color[CONF_TITLE].attrs : \
	config.color[CONF_TITLE].nattrs)

#define ENGINE_STATUS	((COLORS) ? \
	COLOR_PAIR(6) | config.color[CONF_ENGINE].attrs : \
	config.color[CONF_ENGINE].nattrs)

#define NOTIFY_STATUS	((COLORS) ? \
	COLOR_PAIR(7) | config.color[CONF_NOTIFY].attrs : \
	config.color[CONF_NOTIFY].nattrs)

#define WINDOW_BORDER	((COLORS) ? \
	COLOR_PAIR(8) | config.color[CONF_BORDER].attrs : \
	config.color[CONF_BORDER].nattrs)

#define ENGINE_COMMAND_PROMPT	"Engine Command"
#define STATUS_TITLE	"Game Status"
#define DATA_TITLE	"Game Information"
#define MAIN_HELP	"Command Keys"
#define MAIN_HELP_PROMPT	"Type CTRL-g for available command keys"
#define ANNOTATE_HISTORY	"Editing Annotation for"
#define NAG_PROMPT	"Type CTRL-t to edit NAG"

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
    "                                v/V - view history annotations",
    "      N - new game",
    "      r - resume a saved game",
    "      i - show PGN game tags",
    "      s - save game",
    "      > - next game or round",
    "      < - previous game or round",
    " ",
    "      q - quit",
    NULL
};

WINDOW *boardw;
PANEL *boardp;
WINDOW *statusw;
PANEL *statusp;
WINDOW *dataw;
PANEL *datap;

int selected_y, selected_x;
int quit;
int gactive;

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
void history_edit_nag(void *);
void view_annotation(int);
int save_pgn(const char *, struct pgndata *, int);
void set_engine_defaults(void);
int start_chess_engine(void);
void stop_engine(void);
void set_pgn_defaults(void);
#endif
