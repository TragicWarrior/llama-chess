/* $Id: common.h,v 1.19 2002-12-17 23:25:31 bjk Exp $ */
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

struct {
    chtype icon;
} board[8][8];

enum { 
    BOOK_OFF, BOOK_PREFER, BOOK_BEST, BOOK_WORST, BOOK_RANDOM, BOOK_MAX
};

enum {
    ENGINE_OFFLINE = -1, ENGINE_READY, ENGINE_THINKING, HISTORY_MODE, 
    ENGINE_INITIALIZING
};

#define SEND_TO_ENGINE(fmt, args...)	(engine_initialized) ? \
    send_to_engine(fmt, ##args) : 0

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

struct {
    int history_jump;
    int book_method;
    int engine_depth;
} config;

/* Chess engine file descriptors. 0 = from, 1 = to. */
int enginefd[2];

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
	void (*)(void), int, ...);
char *get_input_str(const char *, const char *);
char *get_input_str_clear(const char *, const char *);
void draw_window_title(WINDOW *, const char *, int);
void *Calloc(size_t, size_t);
void *Realloc(void *, size_t);
int message(const char *, const char *, const char *, ...);
void draw_window_title(WINDOW *, const char *, int);
void help(const char *, const char **);
char *trim(char *);
char *itoa(long);

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif
