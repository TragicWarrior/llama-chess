/* $Id: move.c,v 1.18 2003-01-29 17:05:50 bjk Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "move.h"

int int_to_piece(int which)
{
    int p = 0;

    switch (which) {
	case PAWN:
	    p = 'p';
	    break;
	case ROOK:
	    p = 'r';
	    break;
	case KNIGHT:
	    p = 'n';
	    break;
	case BISHOP:
	    p = 'b';
	    break;
	case QUEEN:
	    p = 'q';
	    break;
	case KING:
	    p = 'k';
	    break;
	case OPEN_SQUARE:
	    p = '.';
	    break;
	default:
	    break;
    }

    return (status.turn == WHITE) ? toupper(p) : p;
}

int piece_side(int c)
{
    if (c == int_to_piece(OPEN_SQUARE))
	return -1;
    
    if (c < 'A')
	c = int_to_piece(c);

    return (isupper(c)) ? WHITE : BLACK;
}

int val_piece_side(int c)
{
    if ((isupper(c) && status.turn == WHITE) ||
	    (islower(c) && status.turn == BLACK))
	return 1;

    return 0;
}

int piece_to_int(int p)
{
    if (p == '.')
	return OPEN_SQUARE;

    p = tolower(p);

    switch (p) {
	case 'p':
	    return PAWN;
	case 'r':
	    return ROOK;
	case 'n':
	    return KNIGHT;
	case 'b':
	    return BISHOP;
	case 'q':
	    return QUEEN;
	case 'k':
	    return KING;
	default:
	    break;
    }

    return -1;
}

/*
 * Get the source row and column for a given piece.
 *
 * The following two functions find 'piece' from the given square 'col' and
 * 'row' and store the resulting column or row in 'c' and 'r'. The return
 * value is the number of 'piece' found (on the current status.turns side) or
 * zero. Search for 'piece' stops when a non-empty square is found.
 */
int piece_by_col(BOARD b, int piece, int row, int col, 
	int *r, int *c)
{
    int i;
    int count = 0;

    for (i = col - 1; VALIDFILE(i); i--) {
	int n = b[ROWTOBOARD(row)][COLTOBOARD(i)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(n)) {
		*c = i;
		*r = row;
		count++;
	    }

	    break;
	}
    }

    for (i = col + 1; VALIDFILE(i); i++) {
	int n = b[ROWTOBOARD(row)][COLTOBOARD(i)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(n)) {
		*c = i;
		*r = row;
		count++;
	    }

	    break;
	}
    }

    return count;
}

int piece_by_row(BOARD b, int piece, int row, int col, 
	int *r, int *c)
{
    int i;
    int count = 0;

    for (i = row + 1; VALIDFILE(i); i++) {
	int n = b[ROWTOBOARD(i)][COLTOBOARD(col)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(n)) {
		*r = i;
		*c = col;
		count++;
	    }

	    break;
	}
    }

    for (i = row - 1; VALIDFILE(i); i--) {
	int n = b[ROWTOBOARD(i)][COLTOBOARD(col)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(n)) {
		*r = i;
		*c = col;
		count++;
	    }

	    break;
	}
    }

    return count;
}

int piece_by_xy(BOARD b, int piece, int row, int col, 
	int *srow, int *scol)
{
    int count = 0;

    count = piece_by_row(b, piece, row, col, srow, scol);
    count += piece_by_col(b, piece, row, col, srow, scol);

    return (count != 1) ? 0 : 1;
}

int piece_test(BOARD b, int piece, int row, int col, 
	int *dstr, int *dstc)
{
    int p;

    if (!VALIDFILE(row) || !VALIDFILE(col))
	return 2;

    p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if (piece_to_int(p) != OPEN_SQUARE) {
	if (piece_to_int(p) == piece && val_piece_side(p)) {
	    *dstr = row;
	    *dstc = col;
	    return 1;
	}

	return 2;
    }

    return 0;
}

int piece_by_diag(BOARD b, int piece, int row, int col,
	int *srow, int *scol)
{
    int i, n;
    int ul = 1, ur = 1, dl = 1, dr = 1;
    int count = 0;

    for (i = 1; VALIDFILE(i); i++) {
	if (dr) {
	    n = piece_test(b, piece, row - i, col + i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		dr = 0;
	}

	if (dl) {
	    n = piece_test(b, piece, row - i, col - i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		dl = 0;
	} 

	if (ur) {
	    n = piece_test(b, piece, row + i, col + i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		ur = 0;
	}

	if (ul) {
	    n = piece_test(b, piece, row + i, col - i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		ul = 0;
	}
    }

    return (count) ? 1 : 0;
}

int valid_move(BOARD b, int row, int col, int srow, int scol)
{
    int p1, p2;

    if (!VALIDFILE(srow) || !VALIDFILE(scol))
	return 0;

    if (row == srow && col == scol)
	return 0;

    p1 = b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;
    p2 = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if (piece_side(p1) == piece_side(p2))
	return 0;

    if (piece_to_int(p1) == PAWN && piece_to_int(p2) != OPEN_SQUARE &&
	    scol == col)
	return 0;

    return 1;
}

int castle_move(BOARD b, int which)
{
    int row;
    int n;
    int p, p2, p3, p4;

    row = (status.turn == WHITE) ? 1 : 8;
    n = COLTOINT('e');

    if (which == KINGSIDE) {
	if ((status.turn == WHITE && (game[gindex].wk || game[gindex].rkw)) || 
		(status.turn == BLACK && (game[gindex].bk || game[gindex].rkb)))
	    return 1;

	p = b[ROWTOBOARD(row)][COLTOBOARD((n + 1))].icon;
	p2 = b[ROWTOBOARD(row)][COLTOBOARD((n + 2))].icon;
	p3 = b[ROWTOBOARD(row)][COLTOBOARD((n + 3))].icon;

	if (piece_to_int(p) != OPEN_SQUARE || piece_to_int(p2) != OPEN_SQUARE
		|| (piece_to_int(p3) != ROOK && val_piece_side(p3)))
	    return 1;

	if (!validate_move) {
	    b[ROWTOBOARD(row)][COLTOBOARD(COLTOINT('e'))].icon =
		int_to_piece(OPEN_SQUARE);
	    b[ROWTOBOARD(row)][COLTOBOARD(6)].icon = int_to_piece(ROOK);
	    b[ROWTOBOARD(row)][COLTOBOARD(7)].icon = int_to_piece(KING);
	    b[ROWTOBOARD(row)][COLTOBOARD(8)].icon = int_to_piece(OPEN_SQUARE);

	    if (status.turn == WHITE) {
		game[gindex].wk = game[gindex].rkw = 1;
		status.notify = NOTIFY_WCASTLEK;
	    }
	    else {
		game[gindex].bk = game[gindex].rkb = 1;
		status.notify = NOTIFY_BCASTLEK;
	    }
	}
    }
    else {
	if ((status.turn == WHITE && (game[gindex].wk || game[gindex].rqw)) || 
		(status.turn == BLACK && (game[gindex].bk || game[gindex].rqb)))
	    return 1;

	p = b[ROWTOBOARD(row)][COLTOBOARD((n - 1))].icon;
	p2 = b[ROWTOBOARD(row)][COLTOBOARD((n - 2))].icon;
	p3 = b[ROWTOBOARD(row)][COLTOBOARD((n - 3))].icon;
	p4 = b[ROWTOBOARD(row)][COLTOBOARD((n - 4))].icon;

	if (piece_to_int(p) != OPEN_SQUARE || piece_to_int(p2) != OPEN_SQUARE
		|| piece_to_int(p3) != OPEN_SQUARE ||
		(piece_to_int(p4) != ROOK && val_piece_side(p4)))
	    return 1;

	if (!validate_move) {
	    b[ROWTOBOARD(row)][COLTOBOARD(1)].icon = int_to_piece(OPEN_SQUARE);
	    b[ROWTOBOARD(row)][COLTOBOARD(COLTOINT('e'))].icon =
		int_to_piece(OPEN_SQUARE);
	    b[ROWTOBOARD(row)][COLTOBOARD(3)].icon = int_to_piece(KING);
	    b[ROWTOBOARD(row)][COLTOBOARD(4)].icon = int_to_piece(ROOK);

	    if (status.turn == WHITE) {
		game[gindex].wk = game[gindex].rqw = 1;
		status.notify = NOTIFY_WCASTLEQ;
	    }
	    else {
		game[gindex].bk = game[gindex].rqb = 1;
		status.notify = NOTIFY_BCASTLEQ;
	    }
	}
    }

    return 0;
}

int get_source_yx(BOARD b, int piece, int row, int col, int *srow, int *scol)
{
    int p = 0;
    int count = 0;
    int r, c;
    int i;
    int dist = 0;

    /* FIXME valid move ambiguities. */
    switch (piece) {
	case PAWN:
	    if (*srow == 0 && *scol == col) {
		i = (status.turn == WHITE) ? -1 : 1;

		/* Find the first pawn in the current column. */
		for (r = row + i, dist = 0; VALIDFILE(r); r += i, dist++) {
		    int n = b[ROWTOBOARD(r)][COLTOBOARD(col)].icon;

		    p = piece_to_int(n);

		    if (p == PAWN && val_piece_side(n))
			break;
		}

		if (p != PAWN || dist > 2)
		    return 1;

		*srow = r;
		dist = abs(*srow - row);

		if (status.turn == WHITE) {
		    if ((*srow == 2 && dist > 2) || (*srow > 2 && dist > 1))
			return 1;
		}
		else {
		    if ((*srow == 7 && dist > 2) || (*srow < 7 && dist > 1))
			return 1;
		}

		if (dist == 2) {
		    p = piece_to_int(b[ROWTOBOARD(*srow + i)][COLTOBOARD(col)].icon);
		    if (p != OPEN_SQUARE)
			return 1;
		}
	    }
	    else if (*scol != col) {
		if (abs(*scol - col) != 1)
		    return 1;

		*srow = (status.turn == WHITE) ? row - 1 : row + 1;

		if (piece_to_int(b[ROWTOBOARD(*srow)][COLTOBOARD(*scol)].icon)
			!= PAWN)
		    return 1;

		piece = piece_to_int(b[ROWTOBOARD(row)][COLTOBOARD(col)].icon);

		/* En Passant. */
		if (piece == OPEN_SQUARE) {
		    /* Previous move was not 2 squares and a pawn. */
		    if (game[gindex].enpassant == 0)
			return 1;

		    r = (status.turn == WHITE) ? 6 : 3;

		    if (row != r)
			return 1;

		    r = (status.turn == WHITE) ? row - 1 : row + 1;
		    piece = b[ROWTOBOARD(r)][COLTOBOARD(col)].icon;

		    if (piece_to_int(piece) != PAWN)
			return 1;

		    if (!validate_move) {
			b[ROWTOBOARD(r)][COLTOBOARD(col)].icon =
			    int_to_piece(OPEN_SQUARE);
			status.notify = NOTIFY_ENPASSANT;
		    }
		}
	    }
	    break;
	case ROOK:
	    if (piece_by_xy(b, ROOK, row, col, srow, scol) == 0)
		    return 1;

	    if (!validate_move && *scol == 1) {
		if (status.turn == WHITE)
		    game[gindex].rqw = 1;
		else
		    game[gindex].rqb = 1;
	    }
	    else if (!validate_move && *scol == 8) {
		if (status.turn == WHITE)
		    game[gindex].rkw = 1;
		else
		    game[gindex].rkb = 1;
	    }
	    break;
	case KNIGHT:
	    r = row - 2;
	    c = col - 1;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row - 2;
	    c = col + 1;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {

		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 2;
	    c = col - 1;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 2;
	    c = col + 1;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row - 1;
	    c = col - 2;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row - 1;
	    c = col + 2;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 1;
	    c = col + 2;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 1;
	    c = col - 2;
	    p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    if ((count != 1 && !validate_move) || (validate_move && !count))
		return 1;

	    break;
	case BISHOP:
	    if (piece_by_diag(b, BISHOP, row, col, srow, scol) == 0)
		return 1;
	    break;
	case QUEEN:
	    if (piece_by_xy(b, QUEEN, row, col, srow, scol) == 0) {
		if (piece_by_diag(b, QUEEN, row, col, srow, scol) == 0)
		    return 1;
	    }
	    break;
	case KING:
	    if (piece_by_xy(b, KING, row, col, srow, scol) == 0) {
		if (piece_by_diag(b, KING, row, col, srow, scol) == 0)
		    return 1;
	    }

	    if (abs(*srow - row) > 1)
		return 1;

	    dist = abs(*scol - col);

	    if (*scol == COLTOINT('e')) {
		if (dist > 2)
		    return 1;

		if (validate_move) {
		    if (dist == 2) {
			if (col == 3) {
			    if (castle_move(b, QUEENSIDE))
				return 1;
			}
			else if (col == 7) {
			    if (castle_move(b, KINGSIDE))
				return 1;
			}
			else
			    return 1;

			break;
		    }

		    if (dist > 1)
			return 1;

		    break;
		}

		if (status.turn == WHITE)
		    game[gindex].wk = 1;
		else
		    game[gindex].bk = 1;
	    }

	    if (dist > 1)
		return 1;

	    break;
	default:
	    return 1;
    }

    if (valid_move(b, row, col, *srow, *scol) == 0)
	return 1;

    return 0;
}

/* This function converts a2a4 formatted moves to SAN format. Minimal checks
 * are performed here. The real checks are in parse_move_text() after the
 * conversion.
 */
char *a2a4tosan(BOARD b, char *move)
{
    static char buf[MAX_PGN_MOVE_LEN + 1] = {0}, *cp = buf;
    char *p = move;
    int scol, srow, col, row;
    int piece, piecei, spiece;
    int trow, tcol;
    int rowc, colc;
    int promo = 0;
    int tenpassant = 0;
    int n;

    if (!VALIDCOL(*p) || !VALIDROW(*(p + 1)) || !VALIDCOL(*(p + 2))
	    || !VALIDROW(*(p + 3)))
	return move;

    scol = COLTOINT(*p);
    srow = ROWTOINT(*(p + 1));
    col = COLTOINT(*(p + 2));
    row = ROWTOINT(*(p + 3));

    if (p[4]) {
	if ((promo = piece_to_int(p[4])) == -1)
	    return NULL;
    }

    piece = b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;

    if ((piecei = piece_to_int(piece)) == -1 || piecei == OPEN_SQUARE)
	return NULL;

    spiece = piecei;
    cp = buf;
    colc = abs(scol - col);

    if (srow == row && (row == 1 || row == 8) && scol == COLTOINT('e')
	    && colc > 1 && piecei == KING) {
	if (scol - col < 0)
	    return "O-O";
	else if (scol - col > 0)
	    return "O-O-O";

	return NULL;
    }

    if (piecei != PAWN)
	*cp++ = toupper(piece);
    else {
	/* En Passant. */
	if (scol != col && piece_to_int(b[row][col].icon) == OPEN_SQUARE)
	    tenpassant = 1;
    }

    colc = piece_by_col(b, piecei, row, col, &trow, &tcol);
    rowc = piece_by_row(b, piecei, row, col, &trow, &tcol);
    n = colc + rowc;

    if (piecei == KNIGHT) {
	if (get_source_yx(b, KNIGHT, row, col, &trow, &tcol) == 1)
	    *cp++ = INTTOCOL(scol);
    }
    else if (n > 1 && piecei != PAWN) {
	if (colc > 1 && rowc > 1) {
	    *cp++ = INTTOCOL(scol);
	    *cp++ = INTTOROW(srow);
	}
	else if (colc >= 1)
	    *cp++ = INTTOCOL(scol);
	else if (rowc >= 1)
	    *cp++ = INTTOROW(srow);
    }

    piece = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if ((piecei = piece_to_int(piece)) != OPEN_SQUARE || tenpassant) {
	if (tenpassant || spiece == PAWN)
	    *cp++ = INTTOCOL(scol);

	*cp++ = 'x';
    }

    *cp++ = INTTOCOL(col);
    *cp++ = INTTOROW(row);

    if (promo) {
	*cp++ = '=';
	*cp++ = toupper(int_to_piece(promo));
    }

    *cp = '\0';

    return buf;
}

void switch_turn()
{
    if (status.turn == WHITE)
	status.turn = BLACK;
    else
	status.turn = WHITE;

    return;
}

static void kingsquare(BOARD b, int *kr, int *kc, int *okr,
	int *okc)
{
    int row, col;

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

	    if (piece_to_int(p) == KING) {
		if (val_piece_side(p)) {
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

    return;
}

static int selfchecktest(BOARD b, int kr, int kc)
{
    int row, col;

    if (!VALIDFILE(kr) || !VALIDFILE(kc))
	return 0;

    switch_turn();

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int srow = 0, scol = 0;
	    int p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;
	    int pi = piece_to_int(p);

	    if (pi == OPEN_SQUARE || !val_piece_side(p))
		continue;

	    if (get_source_yx(b, pi, kr, kc, &srow, &scol) == 0) {
		switch_turn();
		return 1;
	    }
	}
    }

    switch_turn();
    return 0;
}

int checktest(BOARD b, int kr, int kc, int okr, int okc)
{
    int row, col;

    /* See if the move would leave ourselves in check. */

    if (selfchecktest(b, kr, kc))
	return -1;

    switch_turn();

    /* See if the move would put our opponent in check. */
    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int srow = 0, scol = 0;
	    int p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;
	    int pi = piece_to_int(p);

	    if (pi == OPEN_SQUARE || !val_piece_side(p))
		continue;

	    if (pi == PAWN)
		scol = col;

	    if (get_source_yx(b, pi, okr, okc, &srow, &scol) == 0) {
		switch_turn();
		return 1;
	    }
	}
    }

    switch_turn();
    return 0;
}

void reset_valid_moves(BOARD b)
{
    int row, col;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++) {
	    b[row][col].valid = 0;
	    b[row][col].movecount = 0;
	}
    }

    return;
}

void get_valid_moves(BOARD b, int p, int srow, int scol, int *minr, int *maxr,
	int *minc, int *maxc)
{
    int row, col;

    validate_move = 1;
    *minr = *maxr = *minc = *maxc = 1;

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int sr = 0, sc = 0;

	    if (get_source_yx(b, p, row, col, &sr, &sc)) {
		sr = 0;
		sc = scol;

		if (get_source_yx(b, p, row, col, &sr, &sc)) {
		    sc = 0;
		    sr = srow;

		    if (get_source_yx(b, p, row, col, &sr, &sc)) {
			continue;
		    }
		}
	    }

	    if (sr != srow || sc != scol)
		continue;

	    b[ROWTOBOARD(row)][COLTOBOARD(col)].valid = 1;

	    /*
	    if (row < *minr)
		*minr = row;

	    if (row > *maxr)
		*maxr = row;

	    if (col < *minc)
		*minc = col;

	    if (col > *maxc)
		*maxc = col;
	    */
	}
    }

    validate_move = 0;
    return;
}

static int checkmate_pawn_test(BOARD b, int row, int col, int *srow, int *scol)
{
    int r, c;

    r = 0;
    c = col;

    if (!get_source_yx(b, PAWN, row, col, &r, &c))
	return 0;

    c = col - 1;

    if (!get_source_yx(b, PAWN, row, col, &r, &c))
	return 0;

    c = col + 1;

    if (!get_source_yx(b, PAWN, row, col, &r, &c))
	return 0;

    return 1;
}

static int checkmatetest(BOARD b, int kr, int kc, int okr, int okc)
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
		BOARD t;

		srow = scol = 0;

		if (n == PAWN) {
		    if (checkmate_pawn_test(b, row, col, &srow, &scol))
			continue;
		}
		else {
		    if (get_source_yx(b, n, row, col, &srow, &scol))
			continue;
		}

		/* Valid move. */
		copy_board(b, t);
		p = t[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;
		t[ROWTOBOARD(row)][COLTOBOARD(col)].icon = p;
		t[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon = 
		    int_to_piece(OPEN_SQUARE);

		if (piece_to_int(p) == KING) {
		    if (piece_side(p) == status.turn) {
			nokr = row;
			nokc = col;
		    }
		    else {
			nkr = row;
			nkc = col;
		    }
		}

		check = checktest(t, nkr, nkc, nokr, nokc);

		if (check == 0)
		    goto done;
	    }
	}
    }

    check = 1;

    if (status.turn == WHITE)
	result = BLACKWINS;
    else
	result = WHITEWINS;

done:
    return (check != 0) ? 1 : 0;
}

int parse_move_text(BOARD b, char *move, int reset)
{
    char *p;
    int piece;
    int i = 0;
    int srow = 0, scol = 0, row, col;
    int dist = 0;
    int promo = -1;
    int kr, kc, okr, okc;
    static int firstrun;

    if (strlen(move) < 2)
	return 1;

    if (reset) {
	if (browse_history) {
	    if (!firstrun) {
		game[gindex].enpassant = 0;
		firstrun = 1;
	    }
	}
	else
	    firstrun = 0;

	game[gindex].castle = 0;
	game[gindex].wk = 0;
	game[gindex].rkw = 0;
	game[gindex].rqw = 0;
	game[gindex].bk = 0;
	game[gindex].rkb = 0;
	game[gindex].rqb = 0;
    }

    status.notify = NULL;
    srow = row = col = scol = promo = piece = 0;
    p = (move) + strlen(move);

    while (!isdigit(*--p) && *p != 'O') {
	if (*p == '=') {
	    promo = piece_to_int(i);
	    break;
	}

	i = *p;
	*p = '\0';
    }

    if (strlen(move) < 2)
	return 1;

    p = move;

    /* Skip 'P'. */
    if (piece_to_int(*p) == PAWN)
	p++;

    /* Pawn. */
    if (VALIDCOL(*p)) {
	for (i = 0; *p; i++) {
	    if (VALIDCOL(*p)) {
		if (i > 0)
		    col = COLTOINT(*p++);
		else
		    col = scol = COLTOINT(*p++);
	    }
	    else if (VALIDROW(*p)) {
		if (1 > 1)
		    row = ROWTOINT(*p++);
		else
		    row = ROWTOINT(*p++);
	    }
	    else if (*p == 'x') {
		col = COLTOINT(*++p);
		row = ROWTOINT(*++p);
	    }
	    else if (*p == '=') {
		if (promo == -1 || promo == KING || promo == PAWN)
		    return 1;

		*p++ = '=';
		*p++ = toupper(int_to_piece(promo));
		*p = '\0';
		break;
	    }
	    else
#ifdef DEBUG
		DUMP("Pawn (move: '%s'): %c\n", move, *p++);
#else
	        p++;
#endif
	}

	if (get_source_yx(b, PAWN, row, col, &srow, &scol))
	    return 1;
    }
    /* Not a pawn. */
    else {
	if (strcmp(move, "O-O") == 0)
	    game[gindex].castle = KINGSIDE;
	else if (strcmp(move, "O-O-O") == 0)
	    game[gindex].castle = QUEENSIDE;
	else {
	    p = move;

	    if ((piece = piece_to_int(*p++)) == -1)
		return 1;

	    if (strlen(move) > 3) {
		if (isdigit(*p))
		    srow = ROWTOINT(*p++);
		else if (VALIDCOL(*p))
		    scol = COLTOINT(*p++);

		if (*p == 'x')
		    p++;
	    }

	    col = COLTOINT(*p++);
	    row = ROWTOINT(*p++);

	    /* Get the source row and column. */
	    if (srow == 0) {
		if (scol > 0) {
		    for (i = 1; VALIDFILE(i); i++) {
			int fpiece = b[ROWTOBOARD(i)][COLTOBOARD(scol)].icon;

			if (piece == piece_to_int(fpiece) && 
				val_piece_side(fpiece)) {
			    srow = i;
			    break;
			}
		    }

		    if (srow == 0)
			return 1;
		}
		else {
		    if (get_source_yx(b, piece, row, col, &srow, &scol))
			return 1;
		}
	    }
	    else if (scol == 0) {
		if (srow > 0) {
		    for (i = 1; VALIDFILE(i); i++) {
			int fpiece = piece_to_int(b[ROWTOBOARD(srow)][COLTOBOARD(i)].icon);

			if (piece == fpiece) {
			    scol = i;
			    break;
			}
		    }

		    if (scol == 0)
			return 1;
		}
		else {
		    if (get_source_yx(b, piece, row, col, &srow, &scol))
			return 1;
		}
	    }
	}
    }

    piece = piece_to_int(b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon);
    dist = abs(srow - row);

    if (!validate_move) {
	if (piece == PAWN && dist == 2)
	    game[gindex].enpassant = 1;
	else
	    game[gindex].enpassant = 0;
    }

    if (game[gindex].castle) {
	if (castle_move(b, game[gindex].castle))
	    return 1;

	goto done;
    }

    piece = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if (piece_to_int(piece) != OPEN_SQUARE) {
	if (val_piece_side(piece))
	    return 2;

	if (!validate_move) {
	    if (piece_side(piece) == WHITE)
		game[gindex].bcaptures++;
	    else
		game[gindex].wcaptures++;

	    status.notify = random_agony();
	}
    }

    if (!validate_move) {
	if (promo) {
	    piece = int_to_piece(promo);
	    status.notify = NOTIFY_PROMOTION;
	}
	else
	    piece = b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;
    }

    b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon = int_to_piece(OPEN_SQUARE);
    b[ROWTOBOARD(row)][COLTOBOARD(col)].icon = piece;

done:
    kingsquare(b, &kr, &kc, &okr, &okc);
    switch_turn();

    if (game[gindex].castle) {
	p = move + strlen(move);
	game[gindex].castle = 0;
    }

    validate_move = 1;

    /* FIXME draw. */
    switch (checktest(b, kr, kc, okr, okc)) {
	case 0:
	    break;
	case -1:
	    validate_move = 0;
	    switch_turn();
	    return 1;
	default:
	    if (checkmatetest(b, kr, kc, okr, okc)) {
		*p++ = '#';

		if (result == WHITEWINS) {
		    game[gindex].tag[TAG_RESULT].value = 
			Realloc(game[gindex].tag[TAG_RESULT].value, 4);
		    strncpy(game[gindex].tag[TAG_RESULT].value, "1-0", 4);
		    status.notify = NOTIFY_CHECKMATE_WHITE_WINS;
		}
		else if (result == BLACKWINS) {
		    game[gindex].tag[TAG_RESULT].value = 
			Realloc(game[gindex].tag[TAG_RESULT].value, 4);
		    strncpy(game[gindex].tag[TAG_RESULT].value, "0-1", 4);
		    status.notify = NOTIFY_CHECKMATE_BLACK_WINS;
		}

		if (curses_initialized)
		    update_tag_window();
	    }
	    else {
		*p++ = '+';

		if ((status.turn == WHITE && status.side == WHITE) ||
			(status.turn == BLACK && status.side == BLACK))
		    status.notify = NOTIFY_CHECK;
	    }

	    *p = '\0';
	    break;
    }

    switch_turn();
    validate_move = 0;
    return 0;
}
