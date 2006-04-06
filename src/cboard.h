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
#ifndef CBOARD_H
#define CBOARD_H

#define COPYRIGHT	"Copyright (C) 2002-2006 " PACKAGE_BUGREPORT
#define LINE_GRAPHIC(c)	((!config.linegraphics) ? ' ' : c)
#define ROWTOMATRIX(r)	((8 - r) * 2 + 2 - 1)
#define COLTOMATRIX(c)	((c == 1) ? 1 : c * 4 - 3)
#define BOARD_HEIGHT	18
#define BOARD_WIDTH	34
#define STATUS_HEIGHT	(BOARD_HEIGHT + HISTORY_HEIGHT - TAG_HEIGHT)
#define STATUS_WIDTH	(COLS - BOARD_WIDTH)
#define TAG_HEIGHT	10
#define TAG_WIDTH	(COLS - BOARD_WIDTH)
#define HISTORY_HEIGHT	(LINES - BOARD_HEIGHT)
#define HISTORY_WIDTH	(COLS - STATUS_WIDTH)
#define MAX_VALUE_WIDTH	(COLS - 8)

enum {
    UP, DOWN, LEFT, RIGHT
};

WINDOW *boardw;
PANEL *boardp;
WINDOW *tagw;
PANEL *tagp;
WINDOW *statusw;
PANEL *statusp;
WINDOW *historyw;
PANEL *historyp;
WINDOW *loadingw;
PANEL *loadingp;

int delete_count = 0;
int markstart = -1, markend = -1;
int board_details;
int c_row, c_col;
int pushkey;
int keycount;
char loadfile[FILENAME_MAX];
int quit;
char **agony;
int paused;

// The selected piece.
struct {
    unsigned char icon;		// The piece.
    char row;			// The source rank.
    char col;			// The source file.
    char destrow;		// Destination rank.
    char destcol;		// Destination file.
} sp;

// Loaded filename from the command line or from the file input dialog.
int filetype;
enum {
    NO_FILE, PGN_FILE, FEN_FILE, EPD_FILE
};

struct country_codes {
    char code[4];
    char country[64];
} *ccodes;

const char *historyhelp[] = {
    "   UP/DOWN - jump to next or previous history *",
    "RIGHT/LEFT - next or previous history *",
    "     SPACE - toggle half move (ply) stepping",
    "         j - jump to move number (prompt) *",
    "         / - specify a new move text search expression *",
    "         ] - find the next move text expression *",
    "         [ - find the previous move text expression *",
    "         a - annotate the previous move",
    "         v - view annotation for the next move",
    "         V - view annotation for the previous move",
    "         + - next variation for the previous move",
    "         - - previous variation for the previous move",
    "         h - exit history mode",
    "        F1 - global and other key help",
    NULL
};

const char *mainhelp[] = {
    "p - play mode keys",
    "h - history mode keys",
    "e - board edit mode keys",
    "g - global game keys",
    NULL
};

const char *edithelp[] = {
    "              0..9 - cursor repeat count",
    "UP/DOWN/LEFT/RIGHT - position cursor *",
    "             SPACE - select piece under cursor for movement",
    "             ENTER - commit selected piece",
    "            ESCAPE - cancel selected piece",
    "                 d - delete the piece under the cursor",
    "                 i - insert a new piece",
    "                 c - toggle castling availability",
    "                 p - this square is the en passant one",
    "                 w - switch turn",
    "                 e - exit edit mode",
    "                F1 - global and other key help",
    NULL,
};

const char *gamehelp[] = {
    " 0...9 - command repeat count",
    "     T - edit the current games roster tags",
    "     t - view the current games roster tags",
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
    "     Q - quit",
    "   F10 - About",
    "    F1 - global and other key help",
    NULL
};

const char *playhelp[] = {
    "             0...9 - cursor repeat count",
    "UP/DOWN/LEFT/RIGHT - position cursor *",
    "             SPACE - select piece under cursor for movement",
    "             ENTER - commit selected piece",
    "            ESCAPE - cancel selected piece",
    "                 d - show game board details",
    "                 w - switch playing side",
    "                 u - undo previous move *",
    "                 g - force engine to make the next move",
    "                 | - send a command to the chess engine",
    "                 o - toggle engine move looping",
    "                 U - toggle two human play",
    "                F1 - global and other key help",
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
    "     CTRL-t - add custom tags",
    "     ESCAPE - quit",
    NULL
};

struct d_entries {
    char *name;
    char *fancy;
    char desc[25];
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

const char *naghelp[] = {
    "    UP/DOWN - previous/next item",
    " LEFT/RIGHT - previous/next selected item",
    "       HOME - first item",
    "        END - last item",
    "CTRL-p/PGUP - previous page",
    "CTRL-n/PGDN - next page",
    "  a-zA-Z0-9 - jump to item",
    "      SPACE - toggle current item",
    "      ENTER - quit with changes",
    "     ESCAPE - quit without changes",
    NULL
};

struct nag_s {
    char *line;
} *nags;

// Status window.
struct {
    char *notify;	// The status window notification line buffer.
} status;

enum {
    MODE_HISTORY, MODE_PLAY, MODE_EDIT
};

int curses_initialized;

// When in history mode a full step is to the next move of the same playing
// side. Half stepping is alternating sides.
int movestep;

void update_all(GAME g);
void parse_engine_output(GAME *g, char *str);

#ifdef DEBUG
void dump_board(int, BOARD);
void dump_flags(int);
char *debug_board(BOARD);
#endif

#endif
