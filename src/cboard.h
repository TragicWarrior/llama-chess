/* $Id: cboard.h,v 1.11 2002-12-09 21:24:24 bjk Exp $ */
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
#define COPYRIGHT	"Copyright (c) 2002 " PACKAGE_BUGREPORT

#define BOARD_WHITE	((COLORS) ? COLOR_PAIR(1) : A_REVERSE)
#define BOARD_BLACK	((COLORS) ? COLOR_PAIR(2) : A_NORMAL)
#define BOARD_SELECTED	((COLORS) ? COLOR_PAIR(3) : A_BOLD | A_REVERSE)
#define BOARD_CURSOR	((COLORS) ? COLOR_PAIR(4) : A_NORMAL)
#define WINDOW_TITLE	((COLORS) ? COLOR_PAIR(5) : A_REVERSE)
#define ENGINE_STATUS	((COLORS) ? COLOR_PAIR(6) : A_BOLD)
#define NOTIFY_STATUS	((COLORS) ? COLOR_PAIR(7) | A_BOLD : A_BOLD)
#define WINDOW_BORDER	((COLORS) ? COLOR_PAIR(9) : A_NORMAL)

#define STATUS_TITLE	"Game Status"
#define DATA_TITLE	"Game Information"
#define MAIN_HELP	"Command Keys"

/* order must match the BOOK_... enumeration on common.h */
const char *book_methods[] = {
    "off", "prefer", "best", "worst", "random"
};

struct {
    int icon;
    int row;
    int col;
} selected_piece;

const char *mainhelp[] = {
    "UP/j    - cursor up              N - new game",
    "DOWN/k  - cursor down            R - refresh screen",
    "LEFT/l  - cursor left/reverse    c - send a command to the game engine",
    "RIGHT/; - cursor right/forward   v - version information",
    "                                 q - quit",
    "SPACE   - select piece           w - switch sides",
    "ENTER   - move selected piece    s - save game in PGN format",
    "ESC     - cancel selected piece  r - resume a PGN game",
    "                                 u - take back previous move",
    "                                 h - toggle viewing of movement history",
    "                                 g - force engine to make next move",
    "                                 b - cycle through book modes",
    "                                 i - PGN information",
    NULL
};

WINDOW *boardw;
PANEL *boardp;
WINDOW *statusw;
PANEL *statusp;
WINDOW *dataw;
PANEL *datap;

const char *x_grid_chars = "abcdefgh";
int piece_selected;
int selected_y, selected_x;
int quit;

void init_chess_engine(void);
void send_to_engine(const char *, ...);
int parse_pgn_file(const char *);
int save_pgn(const char *, struct pgndata *);
void update_history(void);
void init_history(void);
void reset_history(void);
struct pgndata *edit_pgn_data(int);
