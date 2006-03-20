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

// Famous organization.
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
#define VALIDROW(r)	((r >= '1' && r <= '8') ? 1 : 0)
#define VALIDCOL(c)	((c >= 'a' && c <= 'h') ? 1 : 0)

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
#define GF_WK_CASTLE	0x0020
#define GF_WQ_CASTLE	0x0040
#define GF_BK_CASTLE	0x0080
#define GF_BQ_CASTLE	0x0100
#define GF_BLACK_OPENING	0x0200

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

// The chess board.
typedef struct board_matrix {
    chtype icon;		// The piece.
    char valid;			// != 0 if this square is a valid move for the
    				// selected piece.
    unsigned char movecount;	// Distance from the selected piece. FIXME
    char enpassant;		// This square is an en passant one.
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

// Status window.
struct {
    char engine;	// Chess engine status: ENGINE_[READY/OFFLINE].
    char *notify;	// The status window notification line buffer.
} status;

// Roster tags.
typedef struct tags {
    char *name;		// Tag name.
    char *value;	// Tag value.
} TAG;

typedef struct history {
    char *move;				// The SAN move text. FIXME
    char *comment;			// Annotation for this move.
    unsigned char nag[MAX_PGN_NAG];	// Numeric Annotation Glyph. FIXME
    short n;				// Current move number.
    struct history *rav;		// Variation of the current move.
} HISTORY;

// The selected piece from the board.
struct selected_piece_s {
    chtype icon;		// The piece.
    char row;			// The source rank.
    char col;			// The source file.
    char destrow;		// Destination rank.
    char destcol;		// Destination file.
};

/* This is an array of 'games' structures. One for each game in a file, or
 * the current game.
 */
typedef struct games {
    fd_set fds;   		// The file descriptors associated with this
    				// game.
    BOARD b;
    struct selected_piece_s sp; // The selected piece on the board for this
    				// game or ground.
    TAG *tag;			// Roster tags.
    unsigned char tindex;	// Total number of roster tags.
    unsigned char fentag;	// Location of the FEN tag in *tag.
    HISTORY *history;		// Move history for this game.
    HISTORY *hp; 		// History pointer pointing to the location 
    				// in *history used mainly for RAV.
    unsigned char hindex;	// Current move in *hp.
    unsigned char htotal;	// Total number of moves in *hp.
    unsigned char ravindex;	// The original move of *history before *hp
    				// was updated.
    unsigned short ply;		// Move count. FIXME
    unsigned char wcaptures;	// White capture count.
    unsigned char bcaptures;	// Black capture count.
    double moveclock;		// Move clock. FIXME
    unsigned flags;		// Game flags from above.
    char castle;		// The current move is a castling move. FIXME
    char mode;			// Game mode: MODE_[PLAY/HISTORY/EDIT].
    char side;			// This playing side. BLACK or WHITE.
    char turn;			// BLACK or WHITE.
} GAME;

GAME *game;

// The current game or round and the total.
int gindex, gtotal;

// "white wins" not "1-0"
struct {
    char *pgn;		// The formal PGN tag.
    char *fancy;	// The human readable tag.
} fancy_results[4];

// See init_color_pairs() in colors.c.
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
    short fg;		// Foreground color.
    short bg;		// Background color.
    int attrs;		// Attributes for a color terminal.
    int nattrs;		// Attributes for a non-color terminal.
};

struct {
    int stoponerror;	// Stop processing when a parse error occurs.
    int jumpcount;	// KEY_UP and KEY_DOWN in history mode.
    int book_method;	// FIXME
    int engine_depth;	// FIXME
    int historyagony;	// Whether to display agony strings on capture when in 
    			// history mode.
    int agony;		// Whether to display agony anywhere.
    int linegraphics;	// Board line graphics.
    int saveprompt;	// Prompt to save modified games on quit. FIXME
    int deleteprompt;	// Prompt when deleting a game.
    int clevel;		// Compression level for compressed files.
    int validmoves;	// Display valid squares a selected piece can move to.
    char ics_server[MAXHOSTNAMELEN]; // ICS hostname. FIXME
    int ics_port;	// ICS port. FIXME
    char *ics_user;	// ICS username. FIXME
    char *ics_passwd;	// ICS password. FIXME
    struct passwd *pwd;	// Used throughout (tags/home directory).
    char *nagfile;	// The pathname to the NAG data file.
    char *agonyfile;	// The pathname to the agony data file.
    char *configfile;	// The pathname to the configuration file (default or
    			// from the command line).
    char *ccfile;	// The pathname to the Country Code data file.
    char *fifo;		// The pathname to the FIFO used for resuming games
    			// with a chess engine.
    char *tmpfile;	// Temporary file used for decompression of files.
    char *savedirectory; // Directory where saved games are stored.
    char *engine_cmd;	// Alternate chess engine command. FIXME
    int engine;		// FIXME
    struct colors color[CONF_MAX_COLORS]; // Color configuration.
    TAG *tag;		// Custom PGN tags.
    int tindex;		// Total number of custom PGN tags.
} config;

// This is used to pass to get_input() as an argument for a function pointer.
// Used for NAG editing.
struct annotation_edit_s {
    HISTORY h;		// The move.
    int game;		// The game number the move belongs to.
    unsigned char n;	// The history number from game.hp the move belongs to.
};

// Loaded filename from the command line or from the file input dialog.
char loadfile[FILENAME_MAX];

// Loaded file type.
int filetype;
enum {
    NO_FILE, PGN_FILE, FEN_FILE, EPD_FILE
};

/* Chess engine file descriptors. 0 = from, 1 = to. */
int enginefd[2];

// When outputting formatted PGN data (-S), include custom tags from the
// configuration file.
int save_custom_tags;

// When in history mode a full step is to the next move of the same playing
// side. Half stepping is alternating sides.
int movestep;

// Two human players. FIXME
int noengine;
int engine_initialized;

// Where in game.hp are we? heh.
int ravlevel;

// When set, validate_move() won't update any GAME elements.
int validate;

// This is a failsafe when resuming a game.
int oldhistorytotal;

int newgameinit;
int curses_initialized;

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
char *debug_board(BOARD);
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#endif
