/* $Id: common.h,v 1.3 2002-12-05 23:48:33 bjk Exp $ */
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

#ifdef HAVE_CURSES_H
#include <curses.h>
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

#define MAX_PGN_LINE_LEN	255
#define MAX_MOVE_LEN		7 /* As defined by SAN. */
#define NARRAY(arr)		(sizeof(arr) / sizeof(arr[0]))
#define KEY_ESCAPE		'\033'
#define KEY_RETURN		'\015'
#define KEY_TAB			'\011'
#define ANYKEY			"[ press any key to continue ]"
#define YESNO			"[ Yes or No ]"
#define ERROR			"[ ERROR ]"
#define CONFIRM			"[ CONFIRM ]"
#define ACK			message("ack", "ack", "ack")
#define ENGINE_COMMAND_PROMPT	"Command: "
#define UNKNOWN			"not available"

#define CALCPOSY(y)		((y > LINES - 1) ? 0 : LINES / 2 - y / 2)
#define CALCPOSX(x)		(COLS / 2 - x / 2)
#define CENTERX(x, str)		(x / 2 - strlen(str) / 2)

#define MESSAGE_CP		((COLORS) ? COLOR_PAIR(8) : 0)
enum {WHITE, BLACK};

struct {
    chtype icon;
} board[8][8];

struct {
    char white[32];
    char black[32];
    char date[32];
    char site[32];
    char event[32];
    char round[32];
    char result[32];
    char pgnfile[FILENAME_MAX];
} data;

enum { BOOK_OFF, BOOK_PREFER, BOOK_BEST, BOOK_WORST, BOOK_RANDOM, BOOK_MAX };
enum { ENGINE_READY, ENGINE_THINKING, HISTORY_MODE };

struct {
    int engine;
    int bw;
    int book_method;
    char *notify;
    int turn;
    int rounds;
} status;

struct pgndata {
    char token[MAX_PGN_LINE_LEN];
    char value[MAX_PGN_LINE_LEN];
} *pgn;

struct history_s {
    char move[MAX_MOVE_LEN];
} *history;

int cursor_y, cursor_x;
int to_engine;
int from_engine;
int history_index;
int history_total;
int browse_history;
int pgn_index;

void *Calloc(size_t, size_t);
void *Realloc(void *, size_t);
int message(const char *, const char *, const char *, ...);
void parse_engine_output(char *);
void help(void);
void draw_window_title(WINDOW *, const char *, int);
char *real_filename(char *);

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif
