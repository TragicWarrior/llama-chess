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
WINDOW *enginew;
PANEL *enginep;

static char gameexp[255];
static char moveexp[255];
struct itimerval clock_timer;
int delete_count = 0;
int markstart = -1, markend = -1;
int keycount;
char loadfile[FILENAME_MAX];
int quit;
time_t now;
int input_c;

// Loaded filename from the command line or from the file input dialog.
int filetype;
enum {
    NO_FILE, PGN_FILE, FEN_FILE, EPD_FILE
};

struct file_s {
    char *path;
    char *name;
    char *st;
};

struct file_s **files;

const char *history_menu_help_str = {
    "    UP/DOWN - previous/next menu item\n" \
    "   HOME/END - first/last menu item\n" \
    "  PGDN/PGUP - next/previous page\n" \
    "  a-zA-Z0-9 - jump to item\n" \
    "     CTRL-a - annotate the selected move\n" \
    "      ENTER - view annotation\n" \
    "     CTRL-d - toggle board details\n" \
    "     ESCAPE - quit"
};

const char *mainhelp = {
    "p - play mode keys\n" \
    "h - history mode keys\n" \
    "e - board edit mode keys\n" \
    "g - global game keys"
};

const char *filebrowser_help = {
    "    UP/DOWN - previous/next menu item\n" \
    "   HOME/END - first/last menu item\n" \
    "  PGDN/PGUP - next/previous page\n" \
    "  a-zA-Z0-9 - jump to item\n" \
    "      ENTER - select item\n" \
    "          ~ - change to home directory\n" \
    "     ESCAPE - abort"
};

const char *naghelp = {
    "    UP/DOWN - previous/next menu item\n" \
    "   HOME/END - first/last menu item\n" \
    "  PGDN/PGUP - next/previous page\n" \
    "  a-zA-Z0-9 - jump to item\n" \
    "      SPACE - toggle selected item\n" \
    "     CTRL-X - quit with changes"
};

char **nags;

// Status window.
struct {
    char *notify;	// The status window notification line buffer.
} status;

int curses_initialized;

// When in history mode a full step is to the next move of the same playing
// side. Half stepping is alternating sides.
int movestep;

void update_all(GAME g);
void parse_engine_output(GAME *g, char *str);
void update_status_notify(GAME g, char *fmt, ...);
void edit_tags(GAME g, BOARD b, int edit);
void add_custom_tags(TAG ***t);

#ifdef DEBUG
void dump_board(int, BOARD);
void dump_flags(int);
char *debug_board(BOARD);
#endif

#ifndef HAVE_PROGNAME
char *__progname;
#endif

#endif
