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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "chess.h"
#include "pgn.h"
#include "move.h"

#ifdef DEBUG
#include "debug.h"
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

static int piece_side(GAME g, int c)
{
    if (c == pgn_int_to_piece(g.turn, OPEN_SQUARE))
	return -1;
    
    if (c < 'A')
	c = pgn_int_to_piece(g.turn, c);

    return (isupper(c)) ? WHITE : BLACK;
}

static int val_piece_side(char turn, int c)
{
    if ((isupper(c) && turn == WHITE) ||
	    (islower(c) && turn == BLACK))
	return 1;

    return 0;
}

static int count_piece(GAME g, BOARD b, int piece, int sfile, int srank, int
	file, int rank, int *count)
{
    int n = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

    if (!VALIDRANK(rank) || !VALIDFILE(file))
	return 0;

    if (pgn_piece_to_int(n) != OPEN_SQUARE) {
	if (pgn_piece_to_int(n) == piece && val_piece_side(g.turn, n)) {
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
 * value is the number of 'piece' found (on the current g.side) or zero.
 * Search for 'piece' stops when a non-empty square is found.
 */
static int count_by_diag(GAME g, BOARD b, int piece, int sfile, int srank,
	int file, int rank)
{
    int count = 0;
    int ul = 0, ur = 0, dl = 0, dr = 0;
    int i;
    int f, r;

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

static int count_by_rank(GAME g, BOARD b, int piece, int sfile, int srank, 
	int file, int rank)
{
    int i;
    int count = 0;
    int u = 0, d = 0;

    for (i = 1; VALIDRANK(i); i++) {
	if (!u && VALIDRANK((rank + i))) {
	    if (count_piece(g, b, piece, sfile, srank, file, rank + i, &count))
		u++;
	}

	if (!d && VALIDRANK((rank - i))) {
	    if (count_piece(g, b, piece, sfile, srank, file, rank - i, &count))
		d++;
	}
    }

    return count;
}

static int count_by_file(GAME g, BOARD b, int piece, int sfile, int srank,
	int file, int rank)
{
    int i;
    int count = 0;
    int l = 0, r = 0;

    for (i = 1; VALIDFILE(i); i++) {
	if (!r && VALIDFILE((file + i))) {
	    if (count_piece(g, b, piece, sfile, srank, file + i, rank, &count))
		r++;
	}

	if (!l && VALIDFILE((file - i))) {
	    if (count_piece(g, b, piece, sfile, srank, file - i, rank, &count))
		l++;
	}
    }

    return count;
}

static int count_by_rank_file(GAME g, BOARD b, int piece, int sfile, int srank,
	int file, int rank)
{
    int count;

    count = count_by_rank(g, b, piece, sfile, srank, file, rank);
    return count + count_by_file(g, b, piece, sfile, srank, file, rank);
}

/*
 * Returns the number of pieces of type 'p' that can move to the destination
 * square located at 'file' and 'rank'. The source square 'sfile' and 'srank'
 * (if available in the move text) should be determined before calling this
 * function or set to 0 if unknown. Returns 0 if the move is impossible for
 * the piece 'p'.
 */
static int find_ambiguous(GAME g, BOARD b, int p, int sfile, int srank,
	int file, int rank)
{
    int count = 0;

    switch (p) {
	case PAWN:
	    count = 1;
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

static void find_king_squares(GAME g, BOARD b, int *file, int *rank, int *ofile,
	int *orank)
{
    int f, r;

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    int p = b[RANKTOBOARD(r)][FILETOBOARD(f)].icon;

	    if (pgn_piece_to_int(p) != KING)
		continue;

	    if (val_piece_side(g.turn, p))
		*file = f, *rank = r;
	    else
		*ofile = f, *orank = r;
	}
    }
}

// FIXME in-check test
static int validate_castle_move(GAME g, BOARD b, int side, int sfile, 
	int srank, int file, int rank)
{
    if (side == KINGSIDE) {
	if ((g.turn == WHITE && !TEST_FLAG(g.flags, GF_WK_CASTLE)) ||
		(g.turn == BLACK && !TEST_FLAG(g.flags, GF_BK_CASTLE)))
	    return E_PGN_INVALID;
    }
    else {
	if ((g.turn == WHITE && !TEST_FLAG(g.flags, GF_WQ_CASTLE)) ||
		(g.turn == BLACK && !TEST_FLAG(g.flags, GF_BQ_CASTLE)))
	    return E_PGN_INVALID;
    }

    if (file > FILETOINT('e')) {
	if (b[RANKTOBOARD(srank)][FILETOBOARD((sfile + 1))].icon 
		!= pgn_int_to_piece(g.turn, OPEN_SQUARE) ||
		b[RANKTOBOARD(srank)][FILETOBOARD((sfile + 2))].icon 
		!= pgn_int_to_piece(g.turn, OPEN_SQUARE))
	    return E_PGN_INVALID;
    }
    else {
	if (b[RANKTOBOARD(srank)][FILETOBOARD((sfile - 1))].icon != 
		pgn_int_to_piece(g.turn, OPEN_SQUARE) ||
		b[RANKTOBOARD(srank)][FILETOBOARD((sfile - 2))].icon != 
		pgn_int_to_piece(g.turn, OPEN_SQUARE) ||
		b[RANKTOBOARD(srank)][FILETOBOARD((sfile - 3))].icon != 
		pgn_int_to_piece(g.turn, OPEN_SQUARE))
	    return E_PGN_INVALID;
    }

    return E_PGN_OK;
}

static int parse_castle_move(GAME g, BOARD b, int side, int *sfile, 
	int *srank, int *file, int *rank)
{
    *srank = *rank = (g.turn == WHITE) ? 1 : 8;
    *sfile = FILETOINT('e');

    if (side == KINGSIDE)
	*file = FILETOINT('g');
    else
	*file = FILETOINT('c');

    return validate_castle_move(g, b, side, *sfile, *srank, *file, *rank);
}

static int find_source_square(GAME g, BOARD b, int piece, int *sfile, 
	int *srank, int file, int rank)
{
    int p = 0;
    int r, f;
    int i;
    int dist = 0;
    int count = 0;

    if (piece == PAWN) {
	if (!*srank && *sfile == file) {
	    /* Find the first pawn in the current column. */
	    i = (g.turn == WHITE) ? -1 : 1;

	    for (r = rank + i, dist = 0; VALIDFILE(r); r += i, dist++) {
		int n = b[RANKTOBOARD(r)][FILETOBOARD(file)].icon;

		p = pgn_piece_to_int(n);

		if (p == PAWN && val_piece_side(g.turn, n))
		    break;
	    }

	    if (p != PAWN || dist > 2)
		return 0;

	    *srank = r;
	    dist = abs(*srank - rank);

	    if (g.turn == WHITE) {
		if ((*srank == 2 && dist > 2) || (*srank > 2 && dist > 1))
		    return 0;
	    }
	    else {
		if ((*srank == 7 && dist > 2) || (*srank < 7 && dist > 1))
		    return 0;
	    }

	    p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

	    if (pgn_piece_to_int(p) != OPEN_SQUARE)
		return 0;
	}
	else if (*sfile != file) {
	    if (abs(*sfile - file) != 1)
		return 0;

	    *srank = (g.turn == WHITE) ? rank - 1 : rank + 1;
	    p = b[RANKTOBOARD(*srank)][FILETOBOARD(*sfile)].icon;

	    if (!val_piece_side(g.turn, p))
		return 0;

	    if (pgn_piece_to_int(p) != PAWN || abs(*srank - rank) != 1)
		return 0;

	    p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;

	    /* En Passant. */
	    if (pgn_piece_to_int(p) == OPEN_SQUARE) {
		/* Previous move was not 2 squares and a pawn. */
		if (!TEST_FLAG(g.flags, GF_ENPASSANT))
		    return 0;

		if (!b[RANKTOBOARD(rank)][FILETOBOARD(file)].enpassant)
		    return 0;

		r = (g.turn == WHITE) ? 6 : 3;

		if (rank != r)
		    return 0;

		r = (g.turn == WHITE) ? rank - 1 : rank + 1;
		p = b[RANKTOBOARD(r)][FILETOBOARD(file)].icon;

		if (pgn_piece_to_int(p) != PAWN)
		    return 0;
	    }

	    if (val_piece_side(g.turn, p))
		return 0;
	}

	count = 1;
    }
    else {
	for (r = 1; VALIDRANK(r); r++) {
	    for (f = 1; VALIDFILE(f); f++) {
		int n;

		if ((*sfile && f != *sfile) || (*srank && r != *srank))
		    continue;

		n = find_ambiguous(g, b, piece, f, r, file, rank);

		if (n) {
		    count += n;
		    *sfile = f;
		    *srank = r;
		}
	    }
	}
    }

    if (count != 1)
	return count;

    if (piece == KING) {
	if (abs(*srank - rank) > 1 || abs(*sfile - file) > 2)
	    return 0;

	if (abs(*sfile - file) == 2) {
	    if (*sfile != FILETOINT('e'))
		return 0;
	    else {
		if (validate_castle_move(g, b, (file > FILETOINT('e')) ?
			    KINGSIDE : QUEENSIDE, *sfile, *srank, file, rank)
			!= E_PGN_OK)
		    return 0;
	    }
	}
    }

    return count;
}

static int finalize_move(GAME *g, BOARD b, int promo, int sfile, int srank, 
	int file, int rank)
{
    int p, pi;

    p = b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon;
    pi = pgn_piece_to_int(p);

    if (pi != OPEN_SQUARE) {
	if (val_piece_side(g->turn, p))
	    return E_PGN_INVALID;
    }

    if (!validate) {
	if (p == PAWN || capture)
	    g->ply = 0;
	else
	    g->ply++;

	pgn_reset_enpassant(b);
	p = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;
	pi = pgn_piece_to_int(p);

	if (pi == PAWN) {
	    if (sfile != file && TEST_FLAG(g->flags, GF_ENPASSANT)) {
		p = (g->turn == WHITE) ? rank - 1 : rank + 1;
		b[RANKTOBOARD(p)][FILETOBOARD(file)].icon =
		    pgn_int_to_piece(g->turn, OPEN_SQUARE);
	    }

	    if (abs(srank - rank) > 1) {
		SET_FLAG(g->flags, GF_ENPASSANT);
		b[RANKTOBOARD(((g->turn == WHITE) ? rank - 1 : rank + 1))][FILETOBOARD(file)].enpassant = 1;
	    }
	    else
		CLEAR_FLAG(g->flags, GF_ENPASSANT);
	}
	else if (pi == ROOK) {
	    if (g->turn == WHITE)
		CLEAR_FLAG(g->flags, (file > FILETOINT('e')) ? GF_WK_CASTLE :
			GF_WQ_CASTLE);
	    else
		CLEAR_FLAG(g->flags, (file > FILETOINT('e')) ? GF_BK_CASTLE :
			GF_BQ_CASTLE);
	}

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

	    castle = 0;
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

    return E_PGN_OK;
#if 0
    _kingsquare(*g, b, &kr, &kc, &okr, &okc);
    pgn_switch_turn(g);

    if (g->castle) {
	p = m + strlen(m);
	g->castle = 0;
    }

    CLEAR_FLAG(g->flags, GF_GAMEOVER);
    i = validate;
    validate = 1;

    if (_drawtest(g, b)) {
	pgn_tag_add(&g->tag, "Result", "1/2-1/2");
	SET_FLAG(g->flags, GF_GAMEOVER);
    }
    else {
	switch (_checktest(g, b, kr, kc, okr, okc, 0)) {
	    case 0:
		break;
	    case -1:
		validate = i;
		pgn_switch_turn(g);
		return E_PGN_INVALID;
	    default:
		if (_checkmatetest(g, b, kr, kc, okr, okc)) {
		    *p++ = '#';

		    if (result == WHITEWINS)
			pgn_tag_add(&g->tag, "Result", "1-0");
		    else if (result == BLACKWINS)
			pgn_tag_add(&g->tag, "Result", "1-0");

		    SET_FLAG(g->flags, GF_GAMEOVER);
		}
		else
		    *p++ = '+';

		*p = '\0';
		break;
	}
    }
    validate = i;
#endif

//    pgn_switch_turn(g);

    return E_PGN_OK;
}

/*
 * Converts a2a3 formatted moves to SAN format. The promotion piece should be
 * appended (a7a8q).
 */
static int frfrtosan(GAME *g, BOARD b, char **m)
{
    char *bp = *m;
    int icon, p, dp, promo = 0;
    int sfile, srank, file, rank;
    int n;
    int fc, rc;
    
    sfile = FILETOINT(bp[0]);
    srank = RANKTOINT(bp[1]);
    file = FILETOINT(bp[2]);
    rank = RANKTOINT(bp[3]);

    if (bp[4]) {
	if ((promo = pgn_piece_to_int(bp[4])) == -1)
	    return E_PGN_PARSE;
    }

    icon = b[RANKTOBOARD(srank)][FILETOBOARD(sfile)].icon;

    if ((p = pgn_piece_to_int(icon)) == -1 || p == OPEN_SQUARE)
	return 0;

    if (p != PAWN && promo)
	return E_PGN_INVALID;

    if (p == PAWN) {
	if (find_source_square(*g, b, p, &sfile, &srank, file, rank) != 1)
	    return E_PGN_INVALID;

	goto capture;
    }

    if (p == KING && abs(sfile - file) > 1) {
	if (abs(sfile - file) > 2)
	    return E_PGN_INVALID;

	if (validate_castle_move(*g, b, (file > FILETOINT('e')) ? 
		    KINGSIDE : QUEENSIDE, sfile, srank, file, rank) != E_PGN_OK)
	    return E_PGN_INVALID;

	castle = 1;
	strcpy(bp, (file > FILETOINT('e')) ? "O-O" : "O-O-O");
	return finalize_move(g, b, promo, sfile, srank, file, rank);
    }

    *bp++ = toupper(icon);
    n = find_ambiguous(*g, b, p, 0, 0, file, rank);
    
    if (!n)
	return E_PGN_INVALID;
    else if (n > 1) {
	fc = find_ambiguous(*g, b, p, sfile, 0, file, rank);
	rc = find_ambiguous(*g, b, p, 0, srank, file, rank);

	if (fc == 1)
	    *bp++ = INTTOFILE(sfile);
	else if (!fc && rc)
	    *bp++ = INTTORANK(srank);
	else if (fc && rc) {
	    *bp++ = INTTOFILE(sfile);
	    *bp++ = INTTORANK(srank);
	}
	else
	    return E_PGN_PARSE; // FIXME probable a bug in the parser.
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

    /*
     * [Pf][fr][x]fr[=P]
     */
    if (promo) {
	if (p != PAWN || (g->turn == WHITE && (srank != 7 || rank != 8)) ||
		(g->turn == BLACK && (srank != 2 || rank != 1)))
	    return E_PGN_INVALID;

	*bp++ = '=';
	*bp++ = pgn_int_to_piece(g->turn, promo);
    }

    *bp = 0;
    return finalize_move(g, b, promo, sfile, srank, file, rank);
}

/* 
 * Valididate move 'mp' against the game state 'g' and game board 'b' and
 * update board 'b'. 'mp' is updated to SAN format for moves which aren't
 * (frfr or e8Q for example). Returns E_PGN_PARSE if there was a move text
 * parsing error, E_PGN_INVALID if the move is invalid or E_PGN_OK if
 * successful.
 */
int pgn_parse_move(GAME *g, BOARD b, char **mp)
{
    char *p;
    int piece;
    int i = -1;
    int srank = 0, sfile = 0, rank, file;
    int promo = -1;
    char *m = *mp;

    capture = 0;
    srank = rank = file = sfile = promo = piece = 0;

    if (VALIDCOL(*m) && VALIDROW(*(m + 1)) && VALIDCOL(*(m + 2)) && 
	    VALIDROW(*(m + 3)))
	return frfrtosan(g, b, mp);
    else if (strcmp(m, "O-O") == 0)
	i = KINGSIDE;
    else if (strcmp(m, "O-O-O") == 0)
	i = QUEENSIDE;

    if (i >= 0) {
	if (parse_castle_move(*g, b, i, &sfile, &srank, &file, &rank) !=
		E_PGN_OK)
	    return E_PGN_INVALID;

	*m++ = INTTOFILE(sfile);
	*m++ = INTTORANK(srank);
	*m++ = INTTOFILE(file);
	*m++ = INTTORANK(rank);
	*m = 0;
	return frfrtosan(g, b, mp);
    }

again:
    if (strlen(m) < 2)
	return E_PGN_PARSE;

    p = (m) + strlen(m);

    while (!isdigit(*--p) && *p != 'O') {
	if (*p == '=') {
	    promo = pgn_piece_to_int(i);
	    i = 0;
	    break;
	}

	i = *p;
	*p = '\0';
    }

    // Old promotion text (e8Q). Convert to SAN.
    if (pgn_piece_to_int(i) != -1) {
	p = (m) + strlen(m);
	*p++ = '=';
	*p++ = i;
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
		    file = FILETOINT(*p++);
		else
		    file = sfile = FILETOINT(*p++);
	    }
	    else if (VALIDROW(*p)) {
		if (1 > 1)
		    rank = RANKTOINT(*p++);
		else
		    rank = RANKTOINT(*p++);
	    }
	    else if (*p == 'x') {
		file = FILETOINT(*++p);
		rank = RANKTOINT(*++p);
		capture++;
	    }
	    else if (*p == '=') {
		if (promo == -1 || promo == KING || promo == PAWN)
		    return E_PGN_PARSE;

		*p++ = '=';
		*p++ = toupper(pgn_int_to_piece(g->turn, promo));
		*p = '\0';
		break;
	    }
	    else {
#ifdef DEBUG
		DUMP("Pawn (move: '%s'): %c\n", m, *p++);
#else
		p++;
#endif
	    }
	}

	if (find_source_square(*g, b, PAWN, &sfile, &srank, file, rank) != 1)
	    return E_PGN_INVALID;
    }
    /* Not a pawn. */
    else {
	if (strcmp(m, "O-O") == 0)
	    castle = KINGSIDE;
	else if (strcmp(m, "O-O-O") == 0)
	    castle = QUEENSIDE;
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
		    srank = RANKTOINT(*p++);
		/*
		 * f[r]fr
		 */
		else if (VALIDCOL(*p)) {
		    sfile = FILETOINT(*p++);

		    /*
		     * frfr
		     */
		    if (isdigit(*p))
			srank = RANKTOINT(*p++);
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
	    file = FILETOINT(*p++);
	    rank = RANKTOINT(*p++);

	    if (*p == '=')
		promo == *++p;

	    if ((i = find_ambiguous(*g, b, piece, sfile, srank, file, rank))
		    != 1)
		return (i == 0) ? E_PGN_INVALID : E_PGN_AMBIGUOUS;

	    /*
	     * The move is a valid one. Find the source file and rank so we
	     * can later update the board positions.
	     */
	    if (find_source_square(*g, b, piece, &sfile, &srank, file, rank) != 1)
		return E_PGN_INVALID;
	}
    }

    *p = 0;
    *mp = m;
    return finalize_move(g, b, promo, sfile, srank, file, rank);
}

/*
 * Like pgn_parse_move() but don't modify game flags in 'g' or board 'b'.
 */
int pgn_validate_move(GAME *g, BOARD b, char **m)
{
    int ret;

    validate = 1;
    ret = pgn_parse_move(g, b, m);
    validate = 0;
    return ret;
}

/* 
 * Sets valid moves from game 'g' using board 'b'. The valid moves are for the
 * piece on the board 'b' at 'rank' and 'file'. Returns nothing.
 */
void pgn_find_valid_moves(GAME g, BOARD b, int file, int rank)
{
    int p = pgn_piece_to_int(b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon);
    int r, f;

    validate = 1;

    for (r = 1; VALIDRANK(r); r++) {
	for (f = 1; VALIDFILE(f); f++) {
	    int n = 0;

	    if (val_piece_side(g.turn, b[RANKTOBOARD(r)][FILETOBOARD(f)].icon))
		continue;

	    if (find_source_square(g, b, p, &file, (p == PAWN) ? &n : &rank, f,
			r) != 0)
		b[RANKTOBOARD(r)][FILETOBOARD(f)].valid = 1;
	}
    }

    validate = 0;
}

#if 0
void _kingsquare(GAME g, BOARD b, int *kr, int *kc, int *okr, int *okc)
{
    int row, col;

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

	    if (pgn_piece_to_int(p) == KING) {
		if (val_piece_side(g.turn, p)) {
		    *kr = row;
		    *kc = col;
		}
		else {
		    /* Opponent. */
		    *okr = row;
		    *okc = col;
		}
	    }
	}
    }
}

int _checktest(GAME *g, BOARD b, int kr, int kc, int okr, int okc, int matetest)
{
    int row, col;

    pgn_switch_turn(g);

    /* See if the move would put our opponent in check. */
    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int srow = 0, scol = 0;
	    int p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;
	    int pi = pgn_piece_to_int(p);

	    if (pi == OPEN_SQUARE)
		continue;

	    if (pi == PAWN)
		scol = col;

	    /* See if the move would leave ourselves in check. */
	    if (!matetest) {
		pgn_switch_turn(g);

		if (find_source_square(g, b, pi, kr, kc, &srow, &scol) == 0)
		    return -1;

		pgn_switch_turn(g);
	    }

	    if (find_source_square(g, b, pi, okr, okc, &srow, &scol) == 0) {
		pgn_switch_turn(g);
		return 1;
	    }
	}
    }

    pgn_switch_turn(g);
    return 0;
}

int _checkmate_pawn_test(GAME *g, BOARD b, int row, int col, int *srow, int *scol)
{
    int r, c;

    r = 0;
    c = col;

    if (!_get_source_square(g, b, PAWN, row, col, &r, &c))
	return 0;

    c = col - 1;

    if (!_get_source_square(g, b, PAWN, row, col, &r, &c))
	return 0;

    c = col + 1;

    if (!_get_source_square(g, b, PAWN, row, col, &r, &c))
	return 0;

    return 1;
}

int _checkmatetest(GAME *g, BOARD b, int kr, int kc, int okr, int okc)
{
    int row, col;
    int srow, scol;
    int check;

    /* For each square on the board see if each peace has a valid move, and if
     * so, see if it would leave ourselves or the opponent in check.
     */
    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int n;

	    for (n = 0; n < MAX_PIECES; n++) {
		int p;
		int nkr = kr, nkc = kc, nokr = okr, nokc = okc;
		BOARD oldboard;

		srow = scol = 0;

		if (n == PAWN) {
		    if (_checkmate_pawn_test(g, b, row, col, &srow, &scol))
			continue;
		}
		else {
		    if (_get_source_square(g, b, n, row, col, &srow, &scol))
			continue;
		}

		/* Valid move. */
		memcpy(oldboard, b, sizeof(BOARD));
		p = b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;
		b[ROWTOBOARD(row)][COLTOBOARD(col)].icon = p;
		b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon = 
		    pgn_int_to_piece(g->turn, OPEN_SQUARE);

		if (pgn_piece_to_int(p) == KING) {
		    if (piece_side(*g, p) == g->turn) {
			nokr = row;
			nokc = col;
		    }
		    else {
			nkr = row;
			nkc = col;
		    }
		}

		check = _checktest(g, b, nkr, nkc, nokr, nokc, 1);
		memcpy(b, oldboard, sizeof(BOARD));

		if (check == 0)
		    goto done;
	    }
	}
    }

    check = 1;

    if (g->turn == WHITE)
	result = BLACKWINS;
    else
	result = WHITEWINS;

done:
    return (check != 0) ? 1 : 0;
}

/* FIXME */
int _drawtest(GAME *g, BOARD b)
{
    int row, col;
    int other = 0;

    if (pgn_config.fmd && g->ply >= 50)
	return 1;

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;
	    int piece = pgn_piece_to_int(p);

	    switch (piece) {
		case PAWN:
		    return 0;
		    break;
		case KING:
		    break;
		default:
		    other++;
		    break;
	    }
	}
    }

    if (!other)
	return 1;

    return 0;
}
#endif
