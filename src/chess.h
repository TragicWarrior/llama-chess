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
#ifndef CHESS_H
#define CHESS_H

#ifdef HAVE_CONFIG_H
#include <sys/select.h>		// fd_set
#endif

#define PGN_TIME_FORMAT	"%Y.%m.%d"

#define VALIDRANK	VALIDFILE
#define VALIDFILE(f)	(f >= 1 && f <= 8)
#define ROWTOBOARD(r)	(8 - r)
#define COLTOBOARD(c)	(c - 1)
#define ROWTOINT(r)	(r - '0')
#define COLTOINT(c)	(c - ('a' - 1))
#define VALIDROW(r)	(r >= '1' && r <= '8')
#define VALIDCOL(c)	(c >= 'a' && c <= 'h')

#define SET_FLAG(var, f)	(var |= f)
#define CLEAR_FLAG(var, f)	(var &= ~(f))
#define TOGGLE_FLAG(var, f)	(var ^= (f))
#define TEST_FLAG(var, f)	(var & f)

enum {
    OPEN_SQUARE, PAWN, BISHOP, ROOK, KNIGHT, QUEEN, KING, MAX_PIECES
};

enum {
    WHITE, BLACK
};

enum {
    TAG_EVENT, TAG_SITE, TAG_DATE, TAG_ROUND, TAG_WHITE, TAG_BLACK, TAG_RESULT
};

/* Game flags. */
#define GF_PERROR	0x01	/* Parse error for this game. */
#define GF_DELETE	0x02	/* Flagged for deletion ('x' command). */
#define GF_MODIFIED	0x04	/* Modified tags or history. */
#define GF_ENPASSANT	0x08	/* For En Passant validation. */
#define GF_GAMEOVER	0x010	/* End of game. */
#define GF_WK_CASTLE	0x020
#define GF_WQ_CASTLE	0x040
#define GF_BK_CASTLE	0x080
#define GF_BQ_CASTLE	0x0100
#define GF_BLACK_OPENING	0x0200

/*
 * The chess board.
 */
typedef struct {
    unsigned char icon;		// The piece.
    unsigned char valid: 1, 	// != 0 if this square is a valid move for the
    				// selected piece.
		  enpassant: 1; // This square is an en passant one.
} BOARD[8][8];

/*
 * PGN Roster tags.
 */
typedef struct tags {
    char *name;		// Tag name.
    char *value;	// Tag value.
} TAG;

/*
 * Move history.
 *
 * g.hp is the pointer to the current history which may be .rav for
 * Recursive Annotated Variations. The depth of recursion is kept track of in
 * g.ravlevel. 
 */
typedef struct history {
    char *move;				// The SAN move text.
    char *comment;			// Annotation for this move.
    unsigned char nag[MAX_PGN_NAG];	// Numeric Annotation Glyph. FIXME
    struct history **rav;		// Variation of the current move.
} HISTORY;

/*
 * Keeps game state history for the root move.
 */
typedef struct {
    char *fen;		// Game board state.
    unsigned short flags;
    unsigned char hindex;
    HISTORY **hp;	// Pointer to the root move.
} RAV;

/* 
 * This is an array of 'games' structures. One for each game in a file, or
 * the current game.
 */
typedef struct games {
    BOARD b;			// The board associated with this game.
    fd_set fds;   		// The file descriptors associated with this
    				// game.
    TAG **tag;			// Roster tags.
    HISTORY **history;		// Move history for this game.
    HISTORY **hp; 		// History pointer pointing to the location 
    				// in *history used mainly for RAV.
    RAV *rav;			// Saved game states for the root move of RAV.
    int ravlevel;		// An index to *rav.
    unsigned char hindex;	// Current move in *hp.
    unsigned moveclock;		// Move clock. FIXME
    unsigned short flags;	// Game flags.
    unsigned char castle: 2,	// The current move is a castling move. FIXME
                  side: 1,      // This playing side. BLACK or WHITE.
                  turn: 1,      // BLACK or WHITE.
    	          mode: 2;      // MODE_[HISTORY/EDIT/PLAY]
    unsigned char ply;          // Move count.
} GAME;

GAME *game;
int gindex, gtotal;

typedef enum {
    /*
     * When pgn_write() is called to write a game, write reduced PGN format.
     * This will only write the seven tag roster and move text skipping any
     * annotation.
     */
    PGN_REDUCED,

    /*
     * The number of full moves to write per line. If 0 then pgn_write() will
     * write as many as possible within 80 columns.
     */
    PGN_MPL,

    /*
     * Normally when a parse error occurs in a game the game is flagged with
     * GF_PERROR and the rest of the game is discarded and processing of the
     * next game is done. When set and a parse error occurs the rest of the
     * entire file will be discarded.
     */
    PGN_STOP_ON_ERROR
} pgn_config_flag;

/*
 * Set game flags. See chess.h for available flags.
 */
int pgn_config_set(pgn_config_flag f, int val);

/*
 * Returns the value accociated with 'f' or -1 if 'f' is invalid.
 */
int pgn_config_get(pgn_config_flag f);

/*
 * Converts the character piece 'p' to an integer.
 */
int pgn_piece_to_int(int p);

/*
 * Converts the integer piece 'n' to a character whose turn is 'turn'. WHITE
 * piece are uppercase and BLACK pieces are lowercase.
 */
int pgn_int_to_piece(char turn, int n);

/*
 * Returns the total number of tags in 't' or 0 if 't' is NULL.
 */
int pgn_tag_total(TAG **t);

/*
 * Finds a tag 'name' in the structure array 't'. Returns the location in the
 * array of the found tag or -1 on failure.
 */
int pgn_find_tag(TAG **t, const char *name);

/*
 * Sorts a tag array. The first seven tags are in order of the PGN standard so
 * don't sort'em.
 */
void pgn_sort_tags(TAG **t);

/*
 * Adds a tag 'name' with value 'value' to the pointer to array of TAG
 * pointers 'dst'. If a duplicate tag 'name' was found then the existing tag
 * is updated to the new 'value'. Returns 1 if a duplicate tag was found or 0
 * otherwise.
 */
int pgn_add_tag(TAG ***dst, char *name, char *value);

/*
 * Resets or initializes a new game board 'b'.
 */
void pgn_init_board(BOARD b);

/*
 * This is called at the EOG marker and the beginning of the move text
 * section. So at least a move or EOG marker has to exist. It initializes the
 * board (b) to the FEN tag (if found) and sets the castling and enpassant
 * info for the game 'g'. If 'fen' is set it should be a fen tag and will be
 * parsed rather than the game 'g'.tag FEN tag. Returns 0 on success or if
 * there was both a FEN and SetUp tag with the SetUp tag set to 0. Returns 1
 * if there was a FEN parse error or no FEN tag at all.
 */
int pgn_init_fen_board(GAME *g, BOARD b, char *fen);

/*
 * Creates a FEN tag from the current game 'g' and board 'b'. Returns a FEN
 * tag.
 */
char *pgn_game_to_fen(GAME g, BOARD b);

/*
 * Allocates a new game and increments gindex (the current game) and gtotal
 * (the total number of games).
 */
void pgn_new_game();

/*
 * Returns a file pointer associated with 'filename' or NULL on error with
 * errno set to indicate the error. If compressed file support was enabled at
 * compile time and the filetype is supported and the utility is installed
 * then the file will be decompressed.
 */
FILE *pgn_open(const char *filename);

/*
 * Parses a file whose file pointer is 'fp'. 'fp' may have been returned by
 * pgn_open(). If 'fp' is NULL then a single empty game will be allocated. If
 * there is a parsing error 1 is returned otherwise 0 is returned and the
 * global 'gindex' is set to the last parsed game in the file and the global
 * 'gtotal' is set to the total number of games in the file. The file will be
 * closed when the parsing is done.
 */
int pgn_parse(FILE *fp);

/*
 * Writes a PGN formatted game 'g' to the file pointed to by 'fp'. Returns 1
 * if the written move count doesn't match the game's ply count (FEN tag) or 0
 * on success.
 */
void pgn_write(FILE *fp, GAME g);

/* 
 * Returns 1 if 'filename' is a recognized compressed filetype or 0 if not.
 */
int pgn_is_compressed(const char *filename);

/*
 * Returns the total number of moves in 'h' or 0 if none.
 */
int history_total(HISTORY **h);

/* 
 * Deallocates the all the data for 'h' from position 'start' in the array.
 */
void history_free(HISTORY **h, int start);

/*
 * Returns the history ply 'n' from 'h'. If 'n' is out of range then NULL is
 * returned.
 */
HISTORY *history_by_n(HISTORY **h, int n);

/*
 * Appends move 'm' to game 'g' history pointer. The history pointer may be a
 * in a RAV so g->rav.hp is also updated to the new (realloc()'ed) pointer. If
 * not in a RAV then g->history will be updated. Returns 1 if realloc() failed
 * or 0 on success.
 */
int history_add(GAME *g, const char *m);

/*
 * Resets the game 'g' using board 'b' up to history move 'n'.
 */
int history_update_board(GAME *g, BOARD b, int n);

/*
 * Updates the game 'g' using board 'b' to the next 'n'th history move.
 */
void history_previous(GAME *g, BOARD b, int n);

/*
 * Updates the game 'g' using board 'b' to the previous 'n'th history move.
 */
void history_next(GAME *g, BOARD b, int n);

int pgn_validate_move(GAME *g, BOARD b, char *m);

void pgn_switch_turn(GAME *);
char *pgn_a2a4tosan(GAME *g, BOARD b, char *m);
void board_reset_valid_moves(BOARD b);
void board_get_valid_moves(GAME *g, BOARD b, int p, int srow, int scol, int *minr, int *maxr, int *minc, int *maxc);
void pgn_free_all(void);
void pgn_free(GAME);
void pgn_tag_free(TAG **);
void pgn_reset_enpassant(BOARD b);

#endif
