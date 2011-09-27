/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2007 Ben Kibbey <bjk@luxsci.net>

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

#define PGN_TIME_FORMAT	"%Y.%m.%d"
#define MAX_PGN_LINE_LEN 255
#define MAX_SAN_MOVE_LEN 7
#define MAX_PGN_NAG 8

#define VALIDRANK	VALIDFILE
#define VALIDFILE(f)	(f >= 1 && f <= 8)
#define RANKTOBOARD(r)	(8 - r)
#define FILETOBOARD(c)	(c - 1)
#define RANKTOINT(r)	(r - '0')
#define FILETOINT(c)	(c - ('a' - 1))
#define VALIDROW(r)	(r >= '1' && r <= '8')
#define VALIDCOL(c)	(c >= 'a' && c <= 'h')
#define INTTORANK(r)	(r + '0')
#define INTTOFILE(f)	(f + ('a' - 1))

#define SET_FLAG(var, f)	(var |= f)
#define CLEAR_FLAG(var, f)	(var &= ~(f))
#define TOGGLE_FLAG(var, f)	(var ^= (f))
#define TEST_FLAG(var, f)	(var & f)

enum {
    OPEN_SQUARE, PAWN, BISHOP, ROOK, KNIGHT, QUEEN, KING
};

enum {
    WHITE, BLACK
};

/* Game flags. */
#define GF_PERROR	0x01	/* Parse error for this game. */
#define GF_ENPASSANT	0x02	/* For En Passant validation. */
#define GF_GAMEOVER	0x04	/* End of game. */
#define GF_WK_CASTLE	0x08
#define GF_WQ_CASTLE	0x010
#define GF_BK_CASTLE	0x020
#define GF_BQ_CASTLE	0x040
#define GF_BLACK_OPENING	0x080

/*
 * The chess board.
 */
typedef struct {
    unsigned char icon;		// The piece.
    unsigned char valid: 1, 	// != 0 if this square is a valid move for the
    				// selected piece.
		  enpassant: 1;
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
    unsigned char nag[MAX_PGN_NAG];	// Numeric Annotation Glyph.
    struct history **rav;		// Variation of the current move.
} HISTORY;

/*
 * Keeps game state history for the root move.
 */
typedef struct {
    char *fen;		// Game board state.
    unsigned short flags;
    unsigned short hindex;
    HISTORY **hp;	// Pointer to the root move.
} RAV;

/* 
 * This is an array of 'games' structures. One for each game in a file, or
 * the current game.
 */
typedef struct game_s {
    TAG **tag;			// Roster tags.
    HISTORY **history;		// Move history for this game.
    HISTORY **hp; 		// History pointer pointing to the location 
    				// in *history used mainly for RAV.
    RAV *rav;			// Saved game states for the root move of RAV.
    int ravlevel;		// An index to *rav.
    unsigned short hindex;	// Current move in *hp.
    unsigned short flags;	// Game flags.
    unsigned short oflags;	// Game flags for the previous move.
    unsigned char side: 1,      // This playing side. BLACK or WHITE.
                  turn: 1;      // BLACK or WHITE.
    unsigned short ply;         // Move count.
    void *data;			/* User data associated with this game. Must
				 * be freed by the user. */
} *GAME;

/*
 * Global GAME array. pgn_new_game() appends to this array.
 */
GAME *game;

/*
 * 'gindex' and 'gtotal' are the current and total number of games in 'game'.
 */
int gindex, gtotal;

/*
 * Library configuration flags. These will affect all games.
 */
typedef enum {
    /*
     * When pgn_write() is called to write a game, write reduced PGN format.
     * This will only write the seven tag roster and move text skipping any
     * annotation. The type for this flag is an int.
     */
    PGN_REDUCED,

    /*
     * The number of full moves to write per line. If 0 then pgn_write() will
     * write as many as possible within 80 columns. The type for this flag
     * is an int.
     */
    PGN_MPL,

    /*
     * Normally when a parse error occurs in a game the game is flagged with
     * GF_PERROR and the rest of the game is discarded and processing of the
     * next game is done. When set and a parse error occurs the rest of the
     * entire file will be discarded. The type for this flag is an int.
     */
    PGN_STOP_ON_ERROR,

#ifdef DEBUG
    /*
     * If the following is set to a value > 0 and DEBUG was defined
     * at compile time then debugging output will be written to
     * "libchess.debug" in the current directory.
     */
    PGN_DEBUG,
#endif

    /*
     * After PGN_PROGRESS amount of bytes have been read from a file, call
     * PGN_PROGRESS_FUNC. The type for PGN_PROGRESS is a long.
     * PGN_PROGRESS_FUNC is of type pgn_progress.
     */
    PGN_PROGRESS,
    PGN_PROGRESS_FUNC,

    /*
     * If set to 1 and an opponent can attack a castling square the castling
     * move will not be a valid one.
     */
    PGN_STRICT_CASTLING,
} pgn_config_flag;

/*
 * Errors returned from the following functions.
 */
typedef enum {
    E_PGN_ERR = -1,
    E_PGN_OK,
    E_PGN_PARSE,
    E_PGN_AMBIGUOUS,
    E_PGN_INVALID
} pgn_error_t;

typedef struct {
    FILE *fp;
    char *filename;
    char *tmpfile;	/* For appending. */
    int pipe;
} PGN_FILE;

/*
 * The prototype of the PGN_PROGRESS_FUNC function pointer.
 */
typedef void (pgn_progress)(long size, long offset);

/*
 * Sets config flag 'f' to the next argument. Returns E_PGN_OK on success or
 * E_PGN_ERR if 'f' is an invalid flag or E_PGN_INVALID if 'val' is an invalid
 * flag value.
 */
pgn_error_t pgn_config_set(pgn_config_flag f, ...);

/*
 * Gets the value of config flag 'f'. The next argument should be a pointer of
 * the config type which is set to the value of 'f'. Returns E_PGN_ERR if 'f'
 * is an invalid flag or E_PGN_OK on success.
 */
pgn_error_t pgn_config_get(pgn_config_flag f, ...);

/* 
 * Returns E_PGN_OK if 'filename' is a recognized compressed filetype or
 * E_PGN_ERR if not.
 */
pgn_error_t pgn_is_compressed(const char *filename);

/*
 * Opens a file 'filename' with the given 'mode'. 'mode' should be "r" for
 * reading, "w" for writing (will truncate if the file exists) or "a" for
 * appending to an existing file or creating a new one. Returns E_PGN_OK on
 * success and sets 'result' to a file handle for use will the other file
 * functions or E_PGN_ERR if there is an error opening the file in which case
 * errno will be set to the error or E_PGN_INVALID if 'mode' is an invalid
 * mode or if 'filename' is not a regular file.
 */
pgn_error_t pgn_open(const char *filename, const char *mode, PGN_FILE **result);

/*
 * Closes and free's a PGN file handle. Returns E_PGN_OK on success,
 * E_PGN_INVALID if 'pgn' is NULL, or E_PGN_ERR if rename() failed.
 */
pgn_error_t pgn_close(PGN_FILE *pgn);

/*
 * Parses a PGN_FILE which was opened with pgn_open(). If 'pgn' is NULL then a
 * single empty game will be allocated. If there is a parsing error
 * E_PGN_PARSE is returned, if there was a memory allocation error E_PGN_ERR
 * is returned, otherwise E_PGN_OK is returned and the global 'gindex' is set
 * to the last parsed game in the file and the global 'gtotal' is set to the
 * total number of games in the file. The file should be closed with
 * pgn_close() after processing.
 */
pgn_error_t pgn_parse(PGN_FILE *pgn);

/*
 * Allocates a new game and increments 'gtotal'. 'gindex' is then set to the
 * new game. Returns E_PGN_ERR if there was a memory allocation error or
 * E_PGN_OK on success.
 */
pgn_error_t pgn_new_game();

/*
 * Writes a PGN formatted game 'g' to a file which was opened with pgn_open().
 * See 'pgn_config_flag' for output options. Returns E_PGN_ERR if there was a
 * memory allocation or write error and E_PGN_OK on success.
 */
pgn_error_t pgn_write(PGN_FILE *pgn, GAME g);

/*
 * Frees all games in the global 'game' array. Returns nothing.
 */
void pgn_free_all(void);

/*
 * Frees a single game 'g'. Returns nothing.
 */
void pgn_free(GAME g);

/*
 * Adds a tag 'name' with value 'value' to the pointer to array of TAG
 * pointers 'dst'. If a duplicate tag 'name' was found then the existing tag
 * is updated to the new 'value'. If 'value' is NULL, the tag is removed.
 * Returns E_PGN_ERR if there was a memory allocation error or E_PGN_OK on
 * success.
 */
pgn_error_t pgn_tag_add(TAG ***dst, char *name, char *value);

/*
 * Returns the total number of tags in 't' or 0 if 't' is NULL.
 */
int pgn_tag_total(TAG **t);

/*
 * Finds a tag 'name' in the structure array 't'. Returns the location in the
 * array of the found tag or E_PGN_ERR if the tag could not be found.
 */
pgn_error_t pgn_tag_find(TAG **t, const char *name);

/*
 * Sorts a tag array. The first seven tags are in order of the PGN standard so
 * don't sort'em. Returns nothing.
 */
void pgn_tag_sort(TAG **t);

/*
 * Frees a TAG array. Returns nothing.
 */
void pgn_tag_free(TAG **);

/*
 * It initializes the board (b) to the FEN tag (if found) and sets the
 * castling and enpassant info for the game 'g'. If 'fen' is set it should be
 * a fen tag and will be parsed rather than the game 'g'.tag FEN tag. Returns
 * E_PGN_OK on success or if there was both a FEN and SetUp tag with the SetUp
 * tag set to 0. Returns E_PGN_PARSE if there was a FEN parse error, E_PGN_ERR
 * if there was no FEN tag or there was a SetUp tag with a value of 0. Returns
 * E_PGN_OK on success.
 */
pgn_error_t pgn_board_init_fen(GAME g, BOARD b, char *fen);

/*
 * Creates a FEN tag from the current game 'g', history move (g.hindex) and
 * board 'b'. Returns a FEN tag.
 */
char *pgn_game_to_fen(GAME g, BOARD b);

/*
 * Resets or initializes a new game board 'b'. Returns nothing.
 */
void pgn_board_init(BOARD b);

/* 
 * Valididate move 'm' against the game state 'g' and game board 'b' and
 * update board 'b'. 'm' is ensured to be in SAN format and 'frfr' will the
 * file/rank representation of 'm'. Returns E_PGN_PARSE if there was a move
 * text parsing error, E_PGN_INVALID if the move is invalid, E_PGN_AMBIGUOUS
 * if the move is invalid with ambiguities or E_PGN_OK if successful.
 */
pgn_error_t pgn_parse_move(GAME g, BOARD b, char **m, char **frfr);

/*
 * Like pgn_parse_move() but don't modify game flags in 'g' or board 'b'.
 */
pgn_error_t pgn_validate_move(GAME g, BOARD b, char **m, char **frfr);

/*
 * Returns the total number of moves in 'h' or 0 if there are none.
 */
int pgn_history_total(HISTORY **h);

/*
 * Returns the history ply 'n' from 'h'. If 'n' is out of range then NULL is
 * returned.
 */
HISTORY *pgn_history_by_n(HISTORY **h, int n);

/*
 * Appends move 'm' to game 'g' history pointer. The history pointer may be a
 * in a RAV so g->rav.hp is also updated to the new (realloc()'ed) pointer. If
 * not in a RAV then g->history will be updated. Returns E_PGN_ERR if
 * realloc() failed or E_PGN_OK on success.
 */
pgn_error_t pgn_history_add(GAME g, const char *m);

/*
 * Deallocates all of the history data from position 'start' in the array 'h'.
 * Returns nothing.
 */
void pgn_history_free(HISTORY **h, int start);

/*
 * Resets the game 'g' using board 'b' up to history move (g.hindex) 'n'.
 * Returns E_PGN_OK on success or E_PGN_PARSE if there was a FEN tag but the
 * parsing of it failed. Or returns E_PGN_INVALID if somehow the move failed
 * validation while resetting.
 */
pgn_error_t pgn_board_update(GAME g, BOARD b, int n);

/*
 * Updates the game 'g' using board 'b' to the next 'n'th history move.
 * Returns nothing.
 */
void pgn_history_prev(GAME g, BOARD b, int n);

/*
 * Updates the game 'g' using board 'b' to the previous 'n'th history move.
 * Returns nothing.
 */
void pgn_history_next(GAME g, BOARD b, int n);

/*
 * Converts the character piece 'p' to an integer. Returns the integer
 * associated with 'p' or E_PGN_ERR if 'p' is invalid.
 */
int pgn_piece_to_int(int p);

/*
 * Converts the integer piece 'n' to a character whose turn is 'turn'. WHITE
 * piece are uppercase and BLACK pieces are lowercase. Returns the character
 * associated with 'n' or E_PGN_ERR if 'n' is an invalid piece.
 */
pgn_error_t pgn_int_to_piece(char turn, int n);

/*
 * Toggles g->turn. Returns nothing.
 */
void pgn_switch_turn(GAME);

/*
 * Toggles g->side and switches the White and Black roster tags. Returns
 * nothing.
 */
void pgn_switch_side(GAME g);

/*
 * Clears the enpassant flag for all positions on board 'b'. Returns nothing.
 */
void pgn_reset_enpassant(BOARD b);

/*
 * Clears the valid move flag for all positions on board 'b'. Returns nothing.
 */
void pgn_reset_valid_moves(BOARD b);

/* 
 * Sets valid moves (b.valid) from game 'g' using board 'b'. The valid moves
 * are for the piece on the board 'b' at 'rank' and 'file'. Returns nothing.
 */
void pgn_find_valid_moves(GAME g, BOARD b, int rank, int file);

/*
 * Returns the version string of the library.
 */
char *pgn_version(void);

#endif
