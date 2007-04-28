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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "chess.h"
#include "common.h"
#include "move.h"

#ifdef DEBUG
#include "debug.h"
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

static int val_piece_side(register char turn, register int c)
{
    if ((isupper(c) && turn == WHITE) ||
	    (islower(c) && turn == BLACK))
	return 1;

    return 0;
}

static int count_piece(GAME g, BOARD b, register int piece, register int sfile, 
	register int srank, register int file, register int rank, 
	register int *count)
{
    register int p, pi;

    if (!VALIDRANK(rank) || !VALIDFILE(file))
	return 0;

    p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;
    pi = pgn_piece_to_int(p);

    if (pi != OPEN_SQUARE) {
	if (pi == piece && val_piece_side(g->turn, p)) {
	    if (sfile && file == sfile) {
		if (!srank || (srank && rank == srank))
		    (*count)++;
	    }
	    else if (srank && rank == srank) {
		if (!sfile)
		    (*count)++;
	    }
	    else if (!sfile && !srank)
		(*count)++;
	}

	return 1;
    }

    return 0;
}

/*
 * Get the source row and column for a given piece.
 *
 * The following two functions find 'piece' from the given square 'col' and
 * 'row' and store the resulting column or row in 'c' and 'r'. The return
 * value is the number of 'piece' found (on the current g->side) or zero.
 * Search for 'piece' stops when a non-empty square is found.
 */
static int count_by_diag(GAME g, BOARD b, register int piece,
	register int sfile, register int srank, register int file, 
	register int rank)
{
    int count = 0;
    register int ul = 0, ur = 0, dl = 0, dr = 0;
    register int i;
    register int f, r;

    for (i = 1; VALIDFILE(i); i++) {
	r = rank + i;
	f = file - i;

	if (!ul && VALIDRANK(r) && VALIDFILE(f))
	    ul = count_piece(g, b, piece, sfile, srank, f, r, &count);

	r = rank + i;
	f = file + i;

	if (!ur && VALIDRANK(r) && VALIDFILE(f))
	    ur = count_piece(g, b, piece, sfile, srank, f, r, &count);

	r = rank - i;
	f = file - i;

	if (!dl && VALIDRANK(r) && VALIDFILE(f))
	    dl = count_piece(g, b, piece, sfile, srank, f, r, &count);

	r = rank - i;
	f = file + i;

	if (!dr && VALIDRANK(r) && VALIDFILE(f))
	    dr = count_piece(g, b, piece, sfile, srank, f, r, &count);
    }

    return count;
}

static int count_knight(GAME g, BOARD b, int piece, int sfile, int srank,
	int file, int rank)
{
    int count = 0;

    count_piece(g, b, piece, sfile, srank, file - 1, rank + 2, &count);
    count_piece(g, b, piece, sfile, srank, file + 1, rank + 2, &count);
    count_piece(g, b, piece, sfile, srank, file + 2, rank + 1, &count);
    count_piece(g, b, piece, sfile, srank, file - 2, rank + 1, &count);
    count_piece(g, b, piece, sfile, srank, file + 1, rank - 2, &count);
    count_piece(g, b, piece, sfile, srank, file - 1, rank - 2, &count);
    count_piece(g, b, piece, sfile, srank, file + 2, rank - 1, &count);
    count_piece(g, b, piece, sfile, srank, file - 2, rank - 1, &count);
    return count;
}

static int count_by_rank(GAME g, BOARD b, register int piece,
	register int sfile, register int srank, register int file,
	register int rank)
{
    register int i;
    int count = 0;
    register int u = 0, d = 0;

    for (i = 1; VALIDRANK(i); i++) {
	if (!u && VALIDRANK((rank + i)))
	    u = count_piece(g, b, piece, sfile, srank, file, rank + i, &count);

	if (!d && VALIDRANK((rank - i)))
	    d = count_piece(g, b, piece, sfile, srank, file, rank - i, &count);

	if (d && u)
	    break;
    }

    return count;
}

static int count_by_file(GAME g, BOARD b, register int piece, 
	register int sfile, register int srank, register int file, 
	register int rank)
{
    register int i;
    int count = 0;
    register int l = 0, r = 0;

    for (i = 1; VALIDFILE(i); i++) {
	if (!r && VALIDFILE((file + i)))
	    r = count_piece(g, b, piece, sfile, srank, file + i, rank, &count);

	if (!l && VALIDFILE((file - i)))
	    l = count_piece(g, b, piece, sfile, srank, file - i, rank, &count);

	if (r && l)
	    break;
    }

    return count;
}

static int count_by_rank_file(GAME g, BOARD b, register int piece,
	register int sfile, register int srank, register int file, 
	register int rank)
{
    register int count;

    count = count_by_rank(g, b, piece, sfile, srank, file, rank);
    return count + count_by_file(g, b, piece, sfile, srank, file, rank);
}

static int opponent_can_attack(GAME g, BOARD b, register int file,
	register int rank)
{
    int f, r;
    register int p, pi;
    int kf = kfile, kr = krank;

    pgn_switch_turn(g);
    kfile = okfile, krank = okrank;

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    p = b[RANKTOBOARD(r)][FILETOBOARD(f)].icon;
	    pi = pgn_piece_to_int(p);

	    if (pi == OPEN_SQUARE || !val_piece_side(g->turn, p))
		continue;

	    if (find_source_square(g, b, pi, &f, &r, file, rank) != 0) {
		kfile = kf, krank = kr;
		pgn_switch_turn(g);
		return 1;
	    }
	}
    }

    kfile = kf, krank = kr;
    pgn_switch_turn(g);
    return 0;
}

static int validate_castle_move(GAME g, BOARD b, int side, int sfile, 
	int srank, int file, int rank)
{
    int n;

    if (side == KINGSIDE) {
	if ((g->turn == WHITE && !TEST_FLAG(g->flags, GF_WK_CASTLE)) ||
		(g->turn == BLACK && !TEST_FLAG(g->flags, GF_BK_CASTLE)))
	    return E_PGN_INVALID;
    }
    else {
	if ((g->turn == WHITE && !TEST_FLAG(g->flags, GF_WQ_CASTLE)) ||
		(g->turn == BLACK && !TEST_FLAG(g->flags, GF_BQ_CASTLE)))
	    return E_PGN_INVALID;
    }

    if (file > FILETOINT('e')) {
	if (b[RANKTOBOARD(srank)][FILETOBOARD((sfile + 1))].icon 
		!= pgn_int_to_piece(g->turn, OPEN_SQUARE) ||
		b[RANKTOBOARD(srank)][FILETOBOARD((sfile + 2))].icon 
		!= pgn_int_to_piece(g->turn, OPEN_SQUARE))
	    return E_PGN_INVALID;


	if (pgn_config.strict_castling > 0) {
	    if (opponent_can_attack(g, b, sfile + 1, srank))
		return E_PGN_INVALID;

	    if (opponent_can_attack(g, b, sfile + 2, srank))
		return E_PGN_INVALID;
	}
    }
    else {
	if (b[RANKTOBOARD(srank)][FILETOBOARD((sfile - 1))].icon != 
		pgn_int_to_piece(g->turn, OPEN_SQUARE) ||
		b[RANKTOBOARD(srank)][FILETOBOARD((sfile - 2))].icon != 
		pgn_int_to_piece(g->turn, OPEN_SQUARE) ||
		b[RANKTOBOARD(srank)][FILETOBOARD((sfile - 3))].icon != 
		pgn_int_to_piece(g->turn, OPEN_SQUARE))
	    return E_PGN_INVALID;

	if (pgn_config.strict_castling > 0) {
	    if (opponent_can_attack(g, b, sfile - 1, srank))
		return E_PGN_INVALID;

	    if (opponent_can_attack(g, b, sfile - 2, srank))
		return E_PGN_INVALID;

	    if (opponent_can_attack(g, b, sfile - 3, srank))
		return E_PGN_INVALID;
	}
    }

    n = check_testing;
    check_testing = 1;

    if (check_self(g, b, kfile, krank) == CHECK_SELF) {
	check_testing = n;
	return E_PGN_INVALID;
    }

    check_testing = n;
    castle = side;
    return E_PGN_OK;
}

static int validate_piece(GAME g, BOARD b, register int p, register int sfile,
	register int srank, register int file, register int rank)
{
    register int f, r;
    register int i, dist;

    switch (p) {
	case PAWN:
	    if (sfile == file) {
		/* Find the first pawn in the current column. */
		i = (g->turn == WHITE) ? -1 : 1;

		if (!srank) {
		    for (r = rank + i, dist = 0; VALIDFILE(r); r += i, dist++) {
			p = b[RANKTOBOARD(r)][FILETOBOARD(file)].icon;

			if (pgn_piece_to_int(p) != OPEN_SQUARE)
			    break;
		    }

		    if (pgn_piece_to_int(p) != PAWN || !val_piece_side(g->turn, p) || dist > 2)
			return E_PGN_INVALID;

		    srank = r;
		}
		else {
		    dist = abs(srank - rank);
		    p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;

		    if (pgn_piece_to_int(p) != PAWN || !val_piece_side(g->turn, p) || dist > 2)
			return E_PGN_INVALID;
		} 

		if (g->turn == WHITE) {
		    if ((srank == 2 && dist > 2) || (srank > 2 && dist > 1))
			return E_PGN_INVALID;
		}
		else {
		    if ((srank == 7 && dist > 2) || (srank < 7 && dist > 1))
			return E_PGN_INVALID;
		}

		p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

		if (pgn_piece_to_int(p) != OPEN_SQUARE)
		    return E_PGN_INVALID;
	    }
	    else if (sfile != file) {
		if (abs(sfile - file) != 1)
		    return E_PGN_INVALID;

		srank = (g->turn == WHITE) ? rank - 1 : rank + 1;
		p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;

		if (!val_piece_side(g->turn, p))
		    return E_PGN_INVALID;

		if (pgn_piece_to_int(p) != PAWN || abs(srank - rank) != 1)
		    return E_PGN_INVALID;

		p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

		/* En Passant. */
		if (pgn_piece_to_int(p) == OPEN_SQUARE) {
		    /* Previous move was not 2 squares and a pawn. */
		    if (!TEST_FLAG(g->flags, GF_ENPASSANT))
			return E_PGN_INVALID;

		    if (!b[RANKTOBOARD(rank)][FILETOBOARD(file)].enpassant)
			return E_PGN_INVALID;

		    r = (g->turn == WHITE) ? 6 : 3;

		    if (rank != r)
			return E_PGN_INVALID;

		    r = (g->turn == WHITE) ? rank - 1 : rank + 1;
		    p = b[RANKTOBOARD(r)][FILETOBOARD(file)].icon;

		    if (pgn_piece_to_int(p) != PAWN)
			return E_PGN_INVALID;
		}

		if (val_piece_side(g->turn, p))
		    return E_PGN_INVALID;
	    }

	    p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;

	    if (!val_piece_side(g->turn, p))
		return E_PGN_INVALID;
	    
	    break;
	case KING:
	    f = abs(sfile - file);
	    r = abs(srank - rank);

	    if (r > 1 || f > 2)
		return E_PGN_INVALID;

	    if (f == 2) {
		if (sfile != FILETOINT('e'))
		    return E_PGN_INVALID;
		else {
		    if (validate_castle_move(g, b, (file > FILETOINT('e')) ?
				KINGSIDE : QUEENSIDE, sfile, srank, file, rank)
			    != E_PGN_OK)
			return E_PGN_INVALID;
		}
	    }
	    break;
	default:
	    break;
    }

    return E_PGN_OK;
}

/*
 * Returns the number of pieces of type 'p' that can move to the destination
 * square located at 'file' and 'rank'. The source square 'sfile' and 'srank'
 * (if available in the move text) should be determined before calling this
 * function or set to 0 if unknown. Returns 0 if the move is impossible for
 * the piece 'p'.
 */
static int find_ambiguous(GAME g, BOARD b, register int p, register int sfile,
	register int srank, register int file, register int rank)
{
    int count = 0;

    switch (p) {
	case PAWN:
	    count = validate_pawn(g, b, sfile, srank, file, rank);
	    break;
	case ROOK:
	    count = count_by_rank_file(g, b, p, sfile, srank, file, rank);
	    break;
	case KNIGHT:
	    count = count_knight(g, b, p,  sfile, srank, file, rank);
	    break;
	case BISHOP:
	    count = count_by_diag(g, b, p, sfile, srank, file, rank);
	    break;
	case QUEEN:
	    count = count_by_rank_file(g, b, p, sfile, srank, file, rank); 
	    count += count_by_diag(g, b, p, sfile, srank, file, rank);
	    break;
	case KING:
	    count = count_by_rank_file(g, b, p, sfile, srank, file, rank);
	    count += count_by_diag(g, b, p, sfile, srank, file, rank);
	    break;
	default:
	    break;
    }

    return count;
}

static void find_king_squares(GAME g, BOARD b, register int *file,
	register int *rank, register int *ofile, register int *orank)
{
    register int f, r;

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    register int p = b[RANKTOBOARD(r)][FILETOBOARD(f)].icon;
	    register int pi = pgn_piece_to_int(p);

	    if (pi == OPEN_SQUARE || pi != KING)
		continue;

	    if (val_piece_side(g->turn, p))
		*file = f, *rank = r;
	    else
		*ofile = f, *orank = r;
	}
    }

#ifdef DEBUG
    PGN_DUMP("%s:%d: king location: %c%c %c%c(opponent)\n", __FILE__,
	    __LINE__, INTTOFILE(*file), INTTORANK(*rank), 
	    INTTOFILE(*ofile), INTTORANK(*orank));
#endif
}

static int parse_castle_move(GAME g, BOARD b, int side, int *sfile, 
	int *srank, int *file, int *rank)
{
    *srank = *rank = (g->turn == WHITE) ? 1 : 8;
    *sfile = FILETOINT('e');

    if (side == KINGSIDE)
	*file = FILETOINT('g');
    else
	*file = FILETOINT('c');

    return validate_castle_move(g, b, side, *sfile, *srank, *file, *rank);
}

/*
 * Almost exactly like pgn_find_valid_moves() but returns immediately after a
 * valid move is found.
 */
static int check_mate_thingy(GAME g, BOARD b, int file, int rank)
{
    register int r, f;
    register int p = pgn_piece_to_int(b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon);

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    if (val_piece_side(g->turn, b[RANKTOBOARD(r)][FILETOBOARD(f)].icon))
		continue;

	    if (find_source_square(g, b, p, &file, &rank, f, r) != 0)
		return 1;
	}
    }

    return 0;
}

static int checkmate_test(GAME g, BOARD b)
{
    register int f, r;
    int n = CHECK_MATE;
    register int kf = kfile, kr = krank, okf = okfile, okr = okrank;

#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN checkmate test\n", __FILE__, __LINE__);
#endif
    /*
     * The king squares need to be switched also for find_source_squares()
     * which calls check_self().
     */
    pgn_switch_turn(g);
    kfile = okfile, krank = okrank;

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    int p, pi;
	    int sfile = f, srank = r;

	    p = b[RANKTOBOARD(r)][FILETOBOARD(f)].icon;
	    pi = pgn_piece_to_int(p);

	    if (p == OPEN_SQUARE || !val_piece_side(g->turn, p))
		continue;

	    if (check_mate_thingy(g, b, sfile, srank)) {
		n = CHECK;
		goto done;
	    }
	}
    }

done:
    pgn_switch_turn(g);
    kfile = kf, krank = kr, okfile = okf, okrank = okr;
    return n;
}

static int check_opponent(GAME g, BOARD b, register int file, register int rank)
{
    int f, r;
    register int p, pi;

#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN opponent check test\n");
#endif

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    p = b[RANKTOBOARD(r)][FILETOBOARD(f)].icon;
	    pi = pgn_piece_to_int(p);

	    if (pi == OPEN_SQUARE || !val_piece_side(g->turn, p))
		continue;

	    if (find_source_square(g, b, pi, &f, &r, file, rank) != 0)
		return CHECK;
	}
    }

    return 0;
}

static int check_self(GAME g, BOARD b, int file, int rank)
{
    int f, r;
    register int p, pi;

#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN self check test\n", __FILE__, __LINE__);
#endif
    pgn_switch_turn(g);

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    p = b[RANKTOBOARD(r)][FILETOBOARD(f)].icon;
	    pi = pgn_piece_to_int(p);

	    if (pi == OPEN_SQUARE || !val_piece_side(g->turn, p))
		continue;

	    if (find_source_square(g, b, pi, &f, &r, file, rank) != 0) {
		pgn_switch_turn(g);
		return CHECK_SELF;
	    }
	}
    }

    pgn_switch_turn(g);
    return check;
}

static int check_test(GAME g, BOARD b)
{
#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN check test\n", __FILE__, __LINE__);
#endif
    check = check_opponent(g, b, okfile, okrank);

    if (check)
	return checkmate_test(g, b);

    check_testing = 0;
    return check;
}

static int validate_pawn(GAME g, BOARD b, register int sfile, 
	register int srank, register int file, register int rank)
{
    register int n = abs(srank - rank);
    register int p;

    if (abs(sfile - file) > 1)
	return 0;

    if (g->turn == WHITE) {
	if ((srank == 2 && n > 2) || (srank > 2 && n > 1))
	    return 0;
    }
    else {
	if ((srank == 7 && n > 2) || (srank < 7 && n > 1))
	    return 0;
    }

    if (n > 1 && abs(sfile - file) != 0)
	return 0;

    p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;

    if (!val_piece_side(g->turn, p) || pgn_piece_to_int(p) != PAWN)
	return 0;

    if (srank == rank || (g->turn == WHITE && rank < srank) || 
	    (g->turn == BLACK && rank > srank))
	return 0;

    if (sfile == file) {
	p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

	if (pgn_piece_to_int(p) != OPEN_SQUARE)
	    return 0;

	p = (g->turn == WHITE) ? rank - 1 : rank + 1;
	p = b[RANKTOBOARD(p)][FILETOBOARD(file)].icon;

	if (n > 1 && pgn_piece_to_int(p) != OPEN_SQUARE)
	    return 0;

	return 1;
    }

    p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

    if (pgn_piece_to_int(p) != OPEN_SQUARE) {
	if (val_piece_side(g->turn, p))
	    return 0;

	return 1;
    }

    /* En Passant. */
    p = (g->turn == WHITE) ? rank - 1 : rank + 1;
    p = b[RANKTOBOARD(p)][FILETOBOARD(file)].icon;

    if (pgn_piece_to_int(p) == OPEN_SQUARE || val_piece_side(g->turn, p))
	return 0;

    /* Previous move was not 2 squares and a pawn. */
    if (!TEST_FLAG(g->flags, GF_ENPASSANT) ||
	    b[RANKTOBOARD(rank)][FILETOBOARD(file)].enpassant == 0)
	return 0;

    /*
    // GF_ENPASSANT should take care of this
       if (!b[RANKTOBOARD(rank)][FILETOBOARD(file)].enpassant)
       return 0;
       */

    if ((g->turn == WHITE && rank != 6) || (g->turn == BLACK && rank != 3))
	return 0;

#if 0
    // GF_ENPASSANT should take care of this
    n = (g->turn == WHITE) ? rank - 1 : rank + 1;
    p = b[RANKTOBOARD(n)][FILETOBOARD(file)].icon;

    if (pgn_piece_to_int(p) != PAWN)
	return 0;

    if (val_piece_side(g->turn, p))
	return 0;
#endif

    return 1;
}

static int check_self_test(GAME g, BOARD b, int p, int sfile, int srank,
	int file, int rank)
{
    BOARD tmpb;
    struct game_s newg;
    int oldv = validate;
    int nkfile, nkrank, nokfile, nokrank;

    validate = 0;
    check_testing = 1;
    memcpy(tmpb, b, sizeof(BOARD));
    memcpy(&newg, g, sizeof(struct game_s));

    if (finalize_move(&newg, tmpb, 0, sfile, srank, file, rank) 
	    != E_PGN_OK) {
	check_testing = 0;
	validate = oldv;
	return 0;
    }

    if (p == KING)
	find_king_squares(&newg, tmpb, &nkfile, &nkrank, &nokfile, &nokrank);
    else
	nkfile = kfile, nkrank = krank;

    memcpy(&newg, g, sizeof(struct game_s));

    if (check_self(&newg, tmpb, nkfile, nkrank) == CHECK_SELF) {
	check_testing = 0;
	validate = oldv;
	return 0;
    }

    if (!validate && !validate_find)
	g->flags = newg.flags;

    validate = oldv;
    check_testing = 0;
    return 1;
}

static int find_source_square(GAME g, BOARD b, int piece, int *sfile, 
	int *srank, register int file, register int rank)
{
    int p = 0;
    register int r, f, i;
    register int dist = 0;
    int count = 0;

#ifdef DEBUG
    PGN_DUMP("%s:%d: finding source square: piece=%c source=%c%c dest=%c%c\n",
	    __FILE__, __LINE__, pgn_int_to_piece(g->turn, piece), 
	    (*sfile) ? INTTOFILE(*sfile) : '0', 
	    (*srank) ? INTTORANK(*srank) : '0', INTTOFILE(file),
	    INTTORANK(rank));
#endif

    if (piece == PAWN) {
	if (!*srank && *sfile == file) {
	    /* Find the first pawn in 'file'. */
	    i = (g->turn == WHITE) ? -1 : 1;

	    for (r = rank + i, dist = 0; VALIDFILE(r); r += i, dist++) {
		p = pgn_piece_to_int(b[RANKTOBOARD(r)][FILETOBOARD(file)].icon);

		if (p != OPEN_SQUARE)
		    break;
	    }

	    *srank = r;
	}

	if (!*srank)
	    *srank = (g->turn == WHITE) ? rank - 1 : rank + 1;

	if (!validate_pawn(g, b, *sfile, *srank, file, rank))
	    return 0;
	else
	    count = 1;

	if (!check_testing) {
	    if (check_self_test(g, b, piece, *sfile, *srank, file, rank) == 0)
		return 0;
	}
    }
    else {
	if (*sfile && *srank) {
	    count = find_ambiguous(g, b, piece, *sfile, *srank, file, rank);

	    if (count != 1)
		return count;

	    if (!check_testing) {
		if (check_self_test(g, b, piece, *sfile, *srank, file, rank) 
			== 0)
		    return 0;
	    }
	}
	else {
	    int ff = *sfile, rr = *srank;

	    for (r = 1; VALIDRANK(r); r++) {
		for (f = 1; VALIDFILE(f); f++) {
		    int n;

		    if ((*sfile && f != ff) || (*srank && r != rr))
			continue;

		    n = find_ambiguous(g, b, piece, f, r, file, rank);

		    if (n) {
			if (!check_testing && check_self_test(g, b, piece, f, 
				    r, file, rank) == 0)
			    continue;

			count += n;
			ff = f;
			rr = r;
		    }
		}
	    }

	    if (count == 1) {
		*sfile = ff;
		*srank = rr;
	    }
	}

	if (validate_piece(g, b, piece, *sfile, *srank, file, rank) != E_PGN_OK)
	    return 0;
    }

    return count;
}

static int finalize_move(GAME g, BOARD b, int promo, int sfile, int srank, 
	int file, int rank)
{
    int p, pi;

#ifdef DEBUG
    if (!validate && !check_testing)
	PGN_DUMP("%s:%d: BEGIN finalizing\n", __FILE__, __LINE__);
#endif

    p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;
    pi = pgn_piece_to_int(p);

    if (pi != OPEN_SQUARE && val_piece_side(g->turn, p))
	return E_PGN_INVALID;

    if (!validate) {
#ifdef DEBUG
	if (!check_testing)
	    PGN_DUMP("%s:%d: updating board and game flags\n", __FILE__, 
		    __LINE__);
#endif
	pgn_reset_enpassant(b);
	p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;
	pi = pgn_piece_to_int(p);

	if (pi == PAWN) {
	    p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

	    if (sfile != file && pgn_piece_to_int(p) == OPEN_SQUARE && 
		    TEST_FLAG(g->flags, GF_ENPASSANT)) {
		p = (g->turn == WHITE) ? rank - 1 : rank + 1;
		b[RANKTOBOARD(p)][FILETOBOARD(file)].icon =
		    pgn_int_to_piece(g->turn, OPEN_SQUARE);
	    }

	    if (abs(srank - rank) > 1) {
		SET_FLAG(g->flags, GF_ENPASSANT);
		b[RANKTOBOARD(((g->turn == WHITE) ? rank - 1 : rank + 1))][FILETOBOARD(file)].enpassant = 1;
	    }
	}
	else if (pi == ROOK) {
	    if (g->turn == WHITE) {
		if (sfile == FILETOINT('h') && srank == 1)
		    CLEAR_FLAG(g->flags, GF_WK_CASTLE);
		else if (sfile == FILETOINT('a') && srank == 1)
		    CLEAR_FLAG(g->flags, GF_WQ_CASTLE);
	    }
	    else {
		if (sfile == FILETOINT('h') && srank == 8)
		    CLEAR_FLAG(g->flags, GF_BK_CASTLE);
		else if (sfile == FILETOINT('a') && srank == 8)
		    CLEAR_FLAG(g->flags, GF_BQ_CASTLE);
	    }
	}

	if (pi != PAWN)
	    CLEAR_FLAG(g->flags, GF_ENPASSANT);

	if (pi == KING && !castle) {
	    if (g->turn == WHITE)
		CLEAR_FLAG(g->flags, GF_WK_CASTLE|GF_WQ_CASTLE);
	    else
		CLEAR_FLAG(g->flags, GF_BK_CASTLE|GF_BQ_CASTLE);
	}

	if (castle) {
	    p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;
	    b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon =
		pgn_int_to_piece(g->turn, OPEN_SQUARE);
	    b[RANKTOBOARD(srank)][FILETOBOARD(
		    (file > FILETOINT('e') ? 8 : 1))].icon =
		pgn_int_to_piece(g->turn, OPEN_SQUARE);
	    b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon = p;

	    if (file > FILETOINT('e'))
		b[RANKTOBOARD(rank)][FILETOBOARD((file - 1))].icon = 
		    pgn_int_to_piece(g->turn, ROOK);
	    else
		b[RANKTOBOARD(rank)][FILETOBOARD((file + 1))].icon = 
		    pgn_int_to_piece(g->turn, ROOK);

	    if (g->turn == WHITE)
		CLEAR_FLAG(g->flags, (file > FILETOINT('e')) ? GF_WK_CASTLE :
			GF_WQ_CASTLE);
	    else
		CLEAR_FLAG(g->flags, (file > FILETOINT('e')) ? GF_BK_CASTLE :
			GF_BQ_CASTLE);
	}
	else {
	    if (promo)
		p = pgn_int_to_piece(g->turn, promo);
	    else
		p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;

	    b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon = 
		pgn_int_to_piece(g->turn, OPEN_SQUARE);
	    b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon = p;
	}
    }

    castle = 0;

    if (!check_testing) {
	if (pgn_piece_to_int(p) == KING)
	    find_king_squares(g, b, &kfile, &krank, &okfile, &okrank);

	switch (check_test(g, b)) {
	    case CHECK:
		check = CHECK;
		break;
	    case CHECK_MATE:
		check = CHECK_MATE;

		if (!validate) {
		    pgn_tag_add(&g->tag, "Result", 
			    (g->turn == WHITE) ? "1-0" : "0-1");
		    SET_FLAG(g->flags, GF_GAMEOVER);
		}
		break;
	    default:
		break;
	}

	// FIXME RAV
	if (!validate && (pgn_fen_tag > 0 && !done_fen_tag) && 
		!pgn_history_total(g->hp) && srank >= 7)
	    SET_FLAG(g->flags, GF_BLACK_OPENING);
    }

#ifdef DEBUG
    if (!validate && !check_testing)
	PGN_DUMP("%s:%d: END finalizing\n", __FILE__, __LINE__);
#endif

    if (!validate) {
	p = pgn_piece_to_int(p);

	if (p == PAWN || promo || capture)
	    g->ply = 0;
	else
	    g->ply++;

	if (g->ply >= 50) {
	    if (g->tag[6]->value[0] == '*') {
		pgn_tag_add(&g->tag, "Result", "1/2-1/2");
		SET_FLAG(g->flags, GF_GAMEOVER);
	    }
	}
    }

    return E_PGN_OK;
}

static void black_opening(GAME g, BOARD b, int rank)
{
    if (!g->ravlevel && !g->hindex && pgn_tag_find(g->tag, "FEN") == -1) {
	if (rank > 4) {
	    g->turn = BLACK;
	    find_king_squares(g, b, &kfile, &krank, &okfile, &okrank);

	    if (!validate)
		SET_FLAG(g->flags, GF_BLACK_OPENING);
	}
	else {
	    g->turn = WHITE;

	    if (!validate)
		CLEAR_FLAG(g->flags, GF_BLACK_OPENING);
	}
    }

#ifdef DEBUG
    if (TEST_FLAG(g->flags, GF_BLACK_OPENING))
	PGN_DUMP("%s:%d: black opening\n", __FILE__, __LINE__);
#endif
}

static char *format_santofrfr(int promo, int sfile, int srank, int file, 
	int rank)
{
    static char frfr[6] = {0};

    snprintf(frfr, sizeof(frfr), "%c%c%c%c", INTTOFILE(sfile), 
	    INTTORANK(srank), INTTOFILE(file), INTTORANK(rank));

    if (promo)
	frfr[4] = pgn_int_to_piece(BLACK, promo);

    return frfr;
}

/*
 * Converts a2a3 formatted moves to SAN format. The promotion piece should be
 * appended (a7a8q).
 */
static int frfrtosan(GAME g, BOARD b, char **m, char **dst)
{
    char *bp = *m;
    int icon, p, dp, promo = 0;
    int sfile, srank, file, rank;
    int n;
    int fc, rc;
    int ff = 0, rr = 0;
    
#ifdef DEBUG
    PGN_DUMP("%s:%d: converting to SAN format\n", __FILE__, __LINE__);
#endif

    sfile = FILETOINT(bp[0]);
    srank = RANKTOINT(bp[1]);
    file = FILETOINT(bp[2]);
    rank = RANKTOINT(bp[3]);

    black_opening(g, b, rank);

    if (bp[4]) {
	if ((promo = pgn_piece_to_int(bp[4])) == -1 || promo == OPEN_SQUARE)
	    return E_PGN_PARSE;
#ifdef DEBUG
	PGN_DUMP("%s:%d: promotion to %c\n", __FILE__, __LINE__,
		pgn_int_to_piece(g->turn, promo));
#endif
    }

    icon = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;

    if ((p = pgn_piece_to_int(icon)) == -1 || p == OPEN_SQUARE)
	return E_PGN_PARSE;

    if (p != PAWN && promo)
	return E_PGN_INVALID;

    if (p == PAWN) {
	if (find_source_square(g, b, p, &sfile, &srank, file, rank) != 1)
	    return E_PGN_INVALID;

	goto capture;
    }

    if (validate_piece(g, b, p, sfile, srank, file, rank) != E_PGN_OK)
	return E_PGN_INVALID;

    if (p == KING && abs(sfile - file) > 1) {
	strcpy(bp, (file > FILETOINT('e')) ? "O-O" : "O-O-O");

	if (finalize_move(g, b, promo, sfile, srank, file, rank) != E_PGN_OK)
	    return E_PGN_INVALID;

	if (check)
	    strcat(bp, (check == CHECK) ? "+" : "#");

	*dst = format_santofrfr(promo, sfile, srank, file, rank);
	return E_PGN_OK;
    }

    *bp++ = toupper(icon);
    fc = rc = 0;
    n = find_source_square(g, b, p, &fc, &rc, file, rank);
    
    if (!n)
	return E_PGN_INVALID;
    else if (n > 1) {
	fc = find_source_square(g, b, p, &sfile, &rr, file, rank);
	rc = find_source_square(g, b, p, &ff, &srank, file, rank);

	if (fc == 1)
	    *bp++ = INTTOFILE(sfile);
	else if (!fc && rc)
	    *bp++ = INTTORANK(srank);
	else if (fc && rc) {
	    if (rc == 1)
		*bp++ = INTTORANK(srank);
	    else {
		*bp++ = INTTOFILE(sfile);
		*bp++ = INTTORANK(srank);
	    }
	}
	else
	    return E_PGN_PARSE; // not reached.
    }

capture:
    icon = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

    if ((dp = pgn_piece_to_int(icon)) == -1)
	return E_PGN_PARSE;

    /*
     * [Pf][fr]x
     */
    if (dp != OPEN_SQUARE || (dp == OPEN_SQUARE && p == PAWN && sfile != file)) {
	if (p == PAWN)
	    *bp++ = INTTOFILE(sfile);

	*bp++ = 'x';
    }

    /*
     * [Pf][fr][x]fr
     */

    *bp++ = INTTOFILE(file);
    *bp++ = INTTORANK(rank);

    if (p == PAWN && !promo && (rank == 8 || rank == 1)) 
	promo = pgn_piece_to_int('q');

    /*
     * [Pf][fr][x]fr[=P]
     */
    if (promo) {
	if (p != PAWN || (g->turn == WHITE && (srank != 7 || rank != 8)) ||
		(g->turn == BLACK && (srank != 2 || rank != 1)))
	    return E_PGN_INVALID;

	*bp++ = '=';
	*bp++ = pgn_int_to_piece(WHITE, promo);
    }

    *bp = 0;

    if (find_source_square(g, b, p, &sfile, &srank, file, rank) != 1)
	return E_PGN_INVALID;

    if (finalize_move(g, b, promo, sfile, srank, file, rank) != E_PGN_OK)
	return E_PGN_INVALID;

    if (check)
	*bp++ = (check == CHECK) ? '+' : '#';

    *bp = check = 0;
    *dst = format_santofrfr(promo, sfile, srank, file, rank);
#ifdef DEBUG
    PGN_DUMP("%s:%d: END validating %s\n", __FILE__, __LINE__, *m);
#endif
    return E_PGN_OK;
}

static int do_santofrfr(GAME g, BOARD b, char **san, int *promo, int *sfile, 
	int *srank, int *file, int *rank)
{
    static char frfr[6];
    char *p;
    int piece;
    int i = 0;
    char *m = *san;

again:
    if (strlen(m) < 2)
	return E_PGN_PARSE;

    p = (m) + strlen(m);

    while (!isdigit(*--p) && *p != 'O') {
	if (*p == '=') {
	    *promo = pgn_piece_to_int(i);
	    i = 0;
	    break;
	}

	i = *p;
	*p = '\0';
    }

    /* Alternate promotion text (e8Q). Convert to SAN. */
    if (i && pgn_piece_to_int(i) != E_PGN_ERR) {
	p = (m) + strlen(m);
	*p++ = '=';
	*p++ = toupper(i);
	*p = '\0';
	goto again;
    }

    p = m;

    /* Skip 'P' (pawn). */
    if (pgn_piece_to_int(*p) == PAWN)
	p++;

    /* Pawn. */
    if (VALIDCOL(*p)) {
	for (i = 0; *p; i++) {
	    if (VALIDCOL(*p)) {
		if (i > 0)
		    *file = FILETOINT(*p++);
		else
		    *file = *sfile = FILETOINT(*p++);
	    }
	    else if (VALIDROW(*p)) {
		if (1 > 1)
		    *rank = RANKTOINT(*p++);
		else
		    *rank = RANKTOINT(*p++);
	    }
	    else if (*p == 'x') {
		*file = FILETOINT(*++p);
		*rank = RANKTOINT(*++p);
		capture++;
	    }
	    else if (*p == '=') {
		if (*promo == -1 || *promo == KING || *promo == PAWN)
		    return E_PGN_PARSE;

		*p++ = '=';
		*p++ = toupper(pgn_int_to_piece(g->turn, *promo));
		*p = '\0';
		break;
	    }
	    else {
#ifdef DEBUG
		PGN_DUMP("Pawn (move: '%s'): %c\n", m, *p++);
#else
		p++;
#endif
	    }
	}

	black_opening(g, b, *rank);

	if (find_source_square(g, b, PAWN, sfile, srank, *file, *rank) != 1)
	    return E_PGN_INVALID;

	if (!*promo && (*rank == 8 || *rank == 1)) {
	    *promo = pgn_piece_to_int('q');
	    *p++ = '=';
	    *p++ = pgn_int_to_piece(WHITE, *promo);
	    *p = 0;
	}
    }
    /* Not a pawn. */
    else {
	p = m;

	/*
	 * P[fr][x]fr
	 * 
	 * The first character is the piece but only if not a pawn.
	 */
	if ((piece = pgn_piece_to_int(*p++)) == -1)
	    return E_PGN_PARSE;

	/*
	 * [fr]fr
	 */
	if (strlen(m) > 3) {
	    /*
	     * rfr
	     */
	    if (isdigit(*p))
		*srank = RANKTOINT(*p++);
	    /*
	     * f[r]fr
	     */
	    else if (VALIDCOL(*p)) {
		*sfile = FILETOINT(*p++);

		/*
		 * frfr
		 */
		if (isdigit(*p))
		    *srank = RANKTOINT(*p++);
	    }

	    /*
	     * xfr
	     */
	    if (*p == 'x') {
		capture++;
		p++;
	    }
	}

	/*
	 * fr
	 *
	 * The destination square.
	 */
	*file = FILETOINT(*p++);
	*rank = RANKTOINT(*p++);

	if (*p == '=')
	    *promo = *++p;

	black_opening(g, b, *rank);

	if ((i = find_source_square(g, b, piece, sfile, srank, *file, *rank))
		!= 1 && !check)
	    return (i == 0) ? E_PGN_INVALID : E_PGN_AMBIGUOUS;

#if 0
	/*
	 * The move is a valid one. Find the source file and rank so we
	 * can later update the board positions.
	 */
	if (find_source_square(*g, b, piece, sfile, srank, *file, *rank)
		!= 1 && !check)
	    return E_PGN_INVALID;
#endif
    }

    *p = 0;
    snprintf(frfr, sizeof(frfr), "%c%c%c%c", INTTOFILE(*sfile), 
	    INTTORANK(*srank), INTTOFILE(*file), INTTORANK(*rank));

    *san = m;
    return E_PGN_OK;
}

/* 
 * Valididate move 'mp' against the game state 'g' and game board 'b' and
 * update board 'b'. 'mp' is updated to SAN format for moves which aren't
 * (frfr or e8Q for example). Returns E_PGN_PARSE if there was a move text
 * parsing error, E_PGN_INVALID if the move is invalid or E_PGN_OK if
 * successful.
 */
pgn_error_t pgn_parse_move(GAME g, BOARD b, char **mp, char **dst)
{
    int srank = 0, sfile = 0, rank, file;
    char *m = *mp, *p;
    int i;
    int promo = -1;

    /*
     * This may be an empty move with only an annotation. Kinda strange.
     */
    if (!m)
	return E_PGN_OK;

#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN validating '%s' (%s)...\n", __FILE__, __LINE__, m, 
	    (g->turn == WHITE) ? "white" : "black");
#endif

    check_testing = 0;
    srank = rank = file = sfile = promo = 0;
    find_king_squares(g, b, &kfile, &krank, &okfile, &okrank);

    if (m[strlen(m) - 1] == '+')
	m[strlen(m) - 1] = 0;

    if (VALIDCOL(*m) && VALIDROW(*(m + 1)) && VALIDCOL(*(m + 2)) && 
	    VALIDROW(*(m + 3)))
	return frfrtosan(g, b, mp, dst);
    else if (*m == 'O') {
	if (strcmp(m, "O-O") == 0)
	    i = KINGSIDE;
	else if (strcmp(m, "O-O-O") == 0)
	    i = QUEENSIDE;
	else
	    return E_PGN_PARSE;

	if (parse_castle_move(g, b, i, &sfile, &srank, &file, &rank) !=
		E_PGN_OK)
	    return E_PGN_INVALID;

	*m++ = INTTOFILE(sfile);
	*m++ = INTTORANK(srank);
	*m++ = INTTOFILE(file);
	*m++ = INTTORANK(rank);
	*m = 0;
	return frfrtosan(g, b, mp, dst);
    }

    if ((i = do_santofrfr(g, b, &m, &promo, &sfile, &srank, &file, &rank))
	    != E_PGN_OK)
	return i;

    p = m + strlen(m);

    if (finalize_move(g, b, promo, sfile, srank, file, rank) != E_PGN_OK)
	return E_PGN_INVALID;

    if (check)
	*p++ = (check == CHECK) ? '+' : '#';

    *p = check = 0;
    *dst = format_santofrfr(promo, sfile, srank, file, rank);

#ifdef DEBUG
    PGN_DUMP("%s:%d: END validating %s\n", __FILE__, __LINE__, m);
#endif
    return E_PGN_OK;
}

/*
 * Like pgn_parse_move() but don't modify game flags in 'g' or board 'b'.
 */
pgn_error_t pgn_validate_move(GAME g, BOARD b, char **m, char **dst)
{
    int ret;

#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN validate only\n", __FILE__, __LINE__);
#endif
    validate = 1;
    ret = pgn_parse_move(g, b, m, dst);
    validate = 0;
#ifdef DEBUG
    PGN_DUMP("%s:%d: END validate only\n", __FILE__, __LINE__);
#endif
    return ret;
}

/* 
 * Sets valid moves from game 'g' using board 'b'. The valid moves are for the
 * piece on the board 'b' at 'rank' and 'file'. Returns nothing.
 */
void pgn_find_valid_moves(GAME g, BOARD b, int file, int rank)
{
    register int p = pgn_piece_to_int(b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon);
    register int r, f;

#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN valid destination squares for %c%c\n", __FILE__,
	    __LINE__, INTTOFILE(file), INTTORANK(rank));
#endif

    validate_find = 1;
    find_king_squares(g, b, &kfile, &krank, &okfile, &okrank);

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    if (val_piece_side(g->turn, b[RANKTOBOARD(r)][FILETOBOARD(f)].icon))
		continue;

	    if (find_source_square(g, b, p, &file, &rank, f, r) != 0) {
		b[RANKTOBOARD(r)][FILETOBOARD(f)].valid = 1;
#ifdef DEBUG
		PGN_DUMP("%s:%d: %c%c is valid\n", __FILE__, __LINE__, 
			INTTOFILE(f), INTTORANK(r));
#endif
	    }
	}
    }
    
#ifdef DEBUG
    PGN_DUMP("%s:%d: END valid destination squares for %c%c\n", __FILE__,
	    __LINE__, INTTOFILE(file), INTTORANK(rank));
#endif
    check_testing = 0;
    validate_find = 0;
}
