/* $Id: common.h,v 1.52 2003-01-30 17:52:41 bjk Exp $ */
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
#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_FORM_H
#include <form.h>
#endif

#ifdef HAVE_PANEL_H
#include <panel.h>
#endif

#if defined(__linux__) && defined(HAVE_VASPRINTF)
extern int vasprintf(char **, const char *, va_list);
#endif

#include "strings.h"

#ifndef LINE_MAX
#ifdef _POSIX2_LINE_MAX
#define LINE_MAX	_POSIX2_LINE_MAX
#elif defined(_POSIX_LINE_MAX)
#define LINE_MAX	_POSIX_LINE_MAX
#else
#define LINE_MAX	2048
#endif
#endif

#ifdef DEBUG
#define ACK			(curses_initialized && message("ack", "ack", "ack"))
#define ACK2			message("ack2", "ack2", "ack2")
#endif

#define NARRAY(arr)		(sizeof(arr) / sizeof(arr[0]))
#define CTRL(x)			((x) & 0x1f)
#define KEY_ESCAPE		CTRL('[')

#define CALCPOSY(y)		((y > LINES - 1) ? 0 : LINES / 2 - y / 2)
#define CALCPOSX(x)		(COLS / 2 - x / 2)
#define CENTERX(x, str)		(x / 2 - strlen(str) / 2)

#define VALIDFILE(f)	((f >= 1 && f <= 8) ? 1 : 0)
#define ROWTOBOARD(r)	(8 - r)
#define COLTOBOARD(c)	(c - 1)

enum {
    OPEN_SQUARE, PAWN, BISHOP, ROOK, KNIGHT, QUEEN, KING, MAX_PIECES
};

enum {WHITE, BLACK};

enum {
    TAG_EVENT, TAG_SITE, TAG_DATE, TAG_ROUND, TAG_WHITE, TAG_BLACK, TAG_RESULT
};

typedef struct board_matrix {
    chtype icon;
    short valid;
    short movecount;
} BOARD[8][8];

enum { 
    BOOK_OFF, BOOK_PREFER, BOOK_BEST, BOOK_WORST, BOOK_RANDOM, BOOK_MAX
};

enum {
    ENGINE_OFFLINE = -1, ENGINE_READY, ENGINE_THINKING, ENGINE_INITIALIZING
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
    int side;
    char *notify;
    int turn;
} status;

struct tags {
    char *name;
    char *value;
};

struct history {
    char move[MAX_PGN_MOVE_LEN + 1];
    char *comment;
    int nag[MAX_PGN_NAG];
};

/* This is an array of 'games' structures. One for each game in a file, or
 * the current game.
 */
struct games {
    struct tags *tag;
    int tindex;
    struct history *history;
    int hindex;
    int htotal;
    int sockfd;
    int openingside;
    int wcaptures;
    int bcaptures;
    int delete;
    int gameover;
    int enpassant;
    int castle;
    int wk, bk, rqw, rkw, rqb, rkb;
} *game;

/* This holds the selected piece info. */
struct {
    chtype icon;
    int row;
    int col;
    int destrow;
    int destcol;
} sp;

struct {
    char *pgn;
    char *fancy;
} fancy_results[4];

enum { 
    CONF_BWHITE, CONF_BBLACK, CONF_BSELECTED, CONF_BCURSOR, CONF_BGRAPHICS,
    CONF_BCOORDS, CONF_BMOVES, CONF_BCOUNT, CONF_BDWINDOW,
    CONF_TWINDOW, CONF_TTITLE, CONF_TBORDER,
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
    int jumpcount;
    int book_method;
    int engine_depth;
    int historyagony;
    int agony;
    int linegraphics;
    int saveprompt;
    int deleteprompt;
    int clevel;
    int validmoves;
    char ics_server[MAXHOSTNAMELEN];
    int ics_port;
    char *ics_user;
    char *ics_passwd;
    char *nagfile;
    char *agonyfile;
    char *configfile;
    char *ccfile;
    char *fifo;
    char *tmpfile;
    char *savedirectory;
    char *engine_cmd;
    struct colors color[CONF_MAX_COLORS];
    struct tags *tag;
    int tindex;
} config;

/* Chess engine file descriptors. 0 = from, 1 = to. */
int enginefd[2];

int validate_move;
int newgameinit;
int curses_initialized;
char pgnfile[FILENAME_MAX];
int gindex, gtotal; /* Current game and total number of games. */
int browse_history; /* 1 if in history mode. */
int engine_initialized;
int oldhistorytotal; /* This is a failsafe when resuming a game. */
int movestep;

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

#ifdef DEBUG
void DUMP(const char *, ...);
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#endif
