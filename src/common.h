/* $Id: common.h,v 1.31 2002-12-30 19:00:55 bjk Exp $ */
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
#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_FORM_H
#include <form.h>
#endif

#ifdef HAVE_PANEL_H
#include <panel.h>
#endif

#ifndef LINE_MAX
#ifdef _POSIX2_LINE_MAX
#define LINE_MAX	_POSIX2_LINE_MAX
#elif defined(_POSIX_LINE_MAX)
#define LINE_MAX	_POSIX_LINE_MAX
#else
#define LINE_MAX	2048
#endif
#endif

FILE *debugfp;

#define DEBUG(fmt, args...)	debugfp = fopen("debug", "a"); \
	fprintf(debugfp, fmt, ##args); \
        fclose(debugfp)

#define ACK			message("ack", "ack", "ack")
#define ACK2			message("ack2", "ack2", "ack2")

#define NONE			"none"
#define x_grid_chars		"abcdefgh"
#define NARRAY(arr)		(sizeof(arr) / sizeof(arr[0]))
#define CTRL(x)			((x) & 0x1f)
#define KEY_ESCAPE		CTRL('[')
#define ANYKEY			"[ press any key to continue ]"
#define YESNO			"[ Yes or No ]"
#define ERROR			"[ ERROR ]"
#define CONFIRM			"[ CONFIRM ]"
#define UNKNOWN			"not available"

#define CALCPOSY(y)		((y > LINES - 1) ? 0 : LINES / 2 - y / 2)
#define CALCPOSX(x)		(COLS / 2 - x / 2)
#define CENTERX(x, str)		(x / 2 - strlen(str) / 2)

enum {WHITE, BLACK};

enum {
    PGN_EVENT, PGN_SITE, PGN_DATE, PGN_ROUND, PGN_WHITE, PGN_BLACK, PGN_RESULT
};

struct board_matrix {
    chtype icon;
};

struct board_matrix board[8][8];

enum { 
    BOOK_OFF, BOOK_PREFER, BOOK_BEST, BOOK_WORST, BOOK_RANDOM, BOOK_MAX
};

enum {
    ENGINE_OFFLINE = -1, ENGINE_READY, ENGINE_THINKING, HISTORY_MODE, 
    ENGINE_INITIALIZING
};

#define message(title, prompt, args...)	\
    dump_message(title, prompt, 1, NULL, NULL, NULL, 0, ##args)

#define message_uncentered(title, prompt, args...) \
    dump_message(title, prompt, 0, NULL, NULL, NULL, 0, ##args)

#define show_message(title, prompt, ehelp, func, arg, key, args...) \
    dump_message(title, prompt, 0, ehelp, func, arg, key, ##args)

#define SEND_TO_ENGINE(fmt, args...)	(engine_initialized) ? \
    send_to_engine(fmt, ##args) : 0

#define get_input_str(title, init) \
    get_input(title, init, 1, 0, NULL, NULL, NULL, 0, -1, 20)

#define get_input_str_clear(title, init) \
    get_input(title, init, 1, 1, NULL, NULL, NULL, 0, -1, 20)

struct {
    int engine;
    int bw;
    char *notify;
    int turn;
} status;

struct pgndata {
    char token[MAX_PGN_LINE_LEN];
    char value[MAX_PGN_LINE_LEN];
};

struct history {
    char move[MAX_PGN_MOVE_LEN];
    char comment[MAX_PGN_LINE_LEN];
    int nag[MAX_PGN_NAG];
};

/* This is an array of 'games' structures. One for each game in a file, or
 * the current game.
 */
struct games {
    struct pgndata *pgn;
    int pindex;
    struct history *history;
    int hindex;
    int htotal;
    int wcaptures;
    int bcaptures;
} *game;

/* This holds the selected piece info. */
struct {
    int icon;
    int row;
    int col;
    int destrow;
    int destcol;
} sp;

enum { 
    CONF_BWHITE, CONF_BBLACK, CONF_BSELECTED, CONF_BCURSOR, CONF_BGRAPHICS,
    CONF_BCOORDS,
    CONF_WWINDOW, CONF_WTITLE, CONF_WBORDER,
    CONF_BWINDOW, CONF_BTITLE, CONF_BBORDER,
    CONF_SWINDOW, CONF_STITLE, CONF_SBORDER, CONF_SNOTIFY, CONF_SENGINE,
    CONF_HWINDOW, CONF_HTITLE, CONF_HBORDER,
    CONF_MWINDOW, CONF_MTITLE, CONF_MBORDER, CONF_MPROMPT,
    CONF_IWINDOW, CONF_ITITLE, CONF_IBORDER, CONF_IPROMPT,
    CONF_MAX_COLORS
};

struct colors {
    short fg;
    short bg;
    int attrs; /* Attributes for a color terminal. */
    int nattrs; /* Attributes for a non-color terminal. */
};

struct {
    int history_jump;
    int book_method;
    int engine_depth;
    int historyagony;
    int agony;
    int linegraphics;
    char nagfile[FILENAME_MAX];
    char agonyfile[FILENAME_MAX];
    char configfile[FILENAME_MAX];
    char fifo[FILENAME_MAX];
    char savedirectory[FILENAME_MAX];
    struct pgndata *pgn;
    struct colors color[CONF_MAX_COLORS];
    int pindex;
} config;

/* Chess engine file descriptors. 0 = from, 1 = to. */
int enginefd[2];

int curses_initialized;
char pgnfile[FILENAME_MAX];
int gindex, gtotal; /* Current game and total number of games. */
int cursor_y, cursor_x; /* Current cursor position. */
int browse_history; /* 1 if in history mode. */
int cancel_manual_mode;
int engine_initialized;
int oldhistorytotal; /* This is a failsafe when resuming a game. */
WINDOW *historyw;
PANEL *historyp;

enum { FIELD_TYPE_ALNUM, FIELD_TYPE_ALPHA, FIELD_TYPE_INTEGER,
    FIELD_TYPE_NUMERIC, FIELD_TYPE_REGEXP, FIELD_TYPE_IPV4, FIELD_TYPE_ENUM,
    FIELD_TYPE_PGN_TAG_NAME, FIELD_TYPE_PGN_DATE, FIELD_TYPE_PGN_ROUND};

char *get_input(const char *, const char *, int, int, const char *,
	char *(*)(void *), void *, chtype, int, ...);
void *Calloc(size_t, size_t);
void *Realloc(void *, size_t);
int dump_message(const char *, const char *, int, const char *, 
	void (*)(void *), void *, int, const char *, ...);
char *trim(char *);

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif
#endif
