/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2006 Ben Kibbey <bjk@arbornet.org>

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
#define ROWTOINT(r)	(r - '0')
#define COLTOINT(c)	(c - ('a' - 1))

#define SET_FLAG(var, f)	(var |= f)
#define CLEAR_FLAG(var, f)	(var &= ~(f))
#define TOGGLE_FLAG(var, f)	(var ^= (f))
#define TEST_FLAG(var, f)	(var & f)

/* Game flags. */
#define GF_PERROR	0x0001 /* Parse error for this game. */
#define GF_DELETE	0x0002 /* Flagged for deletion ('x' command). */
#define GF_MODIFIED	0x0004 /* Modified tags or history. */
#define GF_ENPASSANT	0x0008 /* For En Passant validation. */
#define GF_GAMEOVER	0x0010 /* End of game. */
#define GF_WK		0x0020 /* For castling validation ... */
#define GF_BK		0x0040
#define GF_WKR		0x0080
#define GF_WQR		0x0100
#define GF_BKR		0x0200
#define GF_BQR		0x0400

enum {
    OPEN_SQUARE, PAWN, BISHOP, ROOK, KNIGHT, QUEEN, KING, MAX_PIECES
};

enum {
    WHITE, BLACK
};

enum {
    TAG_EVENT, TAG_SITE, TAG_DATE, TAG_ROUND, TAG_WHITE, TAG_BLACK, TAG_RESULT
};

enum {
    GNUCHESS, CRAFTY, MAX_ENGINES
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

enum {
    MODE_HISTORY, MODE_PLAY, MODE_EDIT
};

#define cmessage(title, prompt, args...)	\
    dump_message(title, prompt, 1, NULL, NULL, NULL, 0, ##args)

#define message(title, prompt, args...) \
    dump_message(title, prompt, 0, NULL, NULL, NULL, 0, ##args)

#define show_message(title, prompt, ehelp, func, arg, key, args...) \
    dump_message(title, prompt, 0, ehelp, func, arg, key, ##args)

#define SEND_TO_ENGINE(fmt, args...)	(engine_initialized && !noengine) ? \
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
    int mode;
} status;

typedef struct tags {
    char *name;
    char *value;
} TAG;

typedef struct history {
    char move[MAX_PGN_MOVE_LEN + 1];
    char *comment;
    int nag[MAX_PGN_NAG];
} HISTORY;

/* This is an array of 'games' structures. One for each game in a file, or
 * the current game.
 */
typedef struct games {
    TAG *tag;
    int tindex;
    int fentag;
    HISTORY *history;
    int hindex;
    int htotal;
    int sockfd;
    int ply;
    int wcaptures;
    int bcaptures;
    double moveclock;
    int flags;
    int openingside;
    int castle;
} GAME;

GAME *game;

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
    CONF_BCOORDS, CONF_BMOVESW, CONF_BMOVESB, CONF_BCOUNT, CONF_BDWINDOW,
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
    struct passwd *pwd;
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
    int engine;
    struct colors color[CONF_MAX_COLORS];
    TAG *tag;
    int tindex;
} config;

enum {
    PGN_FILE, FEN_FILE, EPD_FILE
};

/* Chess engine file descriptors. 0 = from, 1 = to. */
int enginefd[2];

int validate_move;
int newgameinit;
int curses_initialized;
char loadfile[FILENAME_MAX];
int filetype;
int gindex, gtotal; /* Current game and total number of games. */
int engine_initialized;
int oldhistorytotal; /* This is a failsafe when resuming a game. */
int movestep;
int noengine;

enum {
    FIELD_TYPE_ALNUM, FIELD_TYPE_ALPHA, FIELD_TYPE_INTEGER,
    FIELD_TYPE_NUMERIC, FIELD_TYPE_REGEXP, FIELD_TYPE_IPV4, FIELD_TYPE_ENUM,
    FIELD_TYPE_PGN_TAG_NAME, FIELD_TYPE_PGN_DATE, FIELD_TYPE_PGN_ROUND
};

char *get_input(const char *, const char *, int, int, const char *,
	char *(*)(void *), void *, chtype, int, ...);
void *Calloc(size_t, size_t);
void *Realloc(void *, size_t);
int dump_message(const char *, const char *, int, const char *, 
	void (*)(void *), void *, int, const char *, ...);
char *trim(char *);

#ifdef DEBUG
#define DUMP(fmt, args...)	(write_debug_output(0, fmt, ## args))
#define DUMP_F(fmt, args...)	(write_debug_output(1, fmt, ## args))
void write_debug_output(int, const char *, ...);
void dump_board(int, BOARD);
void dump_flags(int);
int debug;
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#endif
