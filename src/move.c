/* $Id: move.c,v 1.4 2003-01-07 14:14:17 bjk Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "move.h"

int val_piece_side(int c)
{
    if ((isupper(c) && status.turn != WHITE) ||
	    (islower(c) && status.turn != BLACK))
	return 0;

    return 1;
}

static int int_to_piece(int which)
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

static int piece_to_int(int p)
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

int piece_side(int c)
{
    if (c < 'A')
	c = int_to_piece(c);

    return (isupper(c)) ? WHITE : BLACK;
}

/*
 * Get the source row and column for a given piece.
 *
 * The following two functions find 'piece' from the given square 'col' and
 * 'row' and store the resulting column or row in 'c' and 'r'. The return
 * value is the number of 'piece' found (on the current status.turns side) or zero.
 * Search for 'piece' stops when a non-empty square is found.
 */
int piece_by_col(struct board_matrix b[][8], int piece, int row, int col, 
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

int piece_by_row(struct board_matrix b[][8], int piece, int row, int col, 
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

int piece_by_xy(struct board_matrix b[][8], int piece, int row, int col, 
	int *srow, int *scol)
{
    int count = 0;

    count = piece_by_row(b, piece, row, col, srow, scol);
    count += piece_by_col(b, piece, row, col, srow, scol);

    return (count != 1) ? 0 : 1;
}

int piece_test(struct board_matrix b[][8], int piece, int row, int col, 
	int *dstr, int *dstc)
{
    int p;

    if (!VALIDFILE(row) || !VALIDFILE(col))
	return 0;

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

int piece_by_diag(struct board_matrix b[][8], int piece, int row, int col,
	int *srow, int *scol)
{
    int i, n;
    int ul = 1, ur = 1, dl = 1, dr = 1;
    int count = 0;

    for (i = 1; VALIDFILE(i); i++) {
	if (dr) {
	    n = piece_test(b, piece, abs(row - i), col + i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		dr = 0;
	}

	if (dl) {
	    n = piece_test(b, piece, abs(row - i), abs(col - i), srow, scol);

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
	    n = piece_test(b, piece, row + i, abs(col - i), srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		ul = 0;
	}
    }

    return (count) ? 1 : 0;
}

int move_from(struct board_matrix b[][8], int piece, int row, int col, 
	int *srow, int *scol)
{
    int p;
    int count = 0;
    int dstr[4], dstc[4];
    int r, c;

    switch (piece) {
	case ROOK:
	    if (piece_by_xy(b, ROOK, row, col, srow, scol) == 0)
		    return 1;

	    if (*scol == 1) {
		if (status.turn == WHITE)
		    rqw = 1;
		else
		    rqb = 1;
	    }
	    else if (*scol == 8) {
		if (status.turn == WHITE)
		    rkw = 1;
		else
		    rkb = 1;
	    }
	    break;
	case KNIGHT:
	    p = b[ROWTOBOARD((row - 2))][COLTOBOARD((col - 1))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row - 2;
		dstc[count++] = col - 1;
	    }

	    p = b[ROWTOBOARD((row - 2))][COLTOBOARD((col + 1))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row - 2;
		dstc[count++] = col + 1;
	    }

	    p = b[ROWTOBOARD((row + 2))][COLTOBOARD((col - 1))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row + 2;
		dstc[count++] = col - 1;
	    }

	    p = b[ROWTOBOARD((row + 2))][COLTOBOARD((col + 1))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row + 2;
		dstc[count++] = col + 1;
	    }

	    p = b[ROWTOBOARD((row - 1))][COLTOBOARD((col - 2))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row - 1;
		dstc[count++] = col - 2;
	    }

	    p = b[ROWTOBOARD((row - 1))][COLTOBOARD((col + 2))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row - 1;
		dstc[count++] = col + 2;
	    }

	    p = b[ROWTOBOARD((row + 1))][COLTOBOARD((col + 2))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row + 1;
		dstc[count++] = col + 2;
	    }

	    p = b[ROWTOBOARD((row + 1))][COLTOBOARD((col - 2))].icon;

	    if (piece_to_int(p) == KNIGHT && val_piece_side(p)) {
		dstr[count] = row + 1;
		dstc[count++] = col - 2;
	    }

	    if (count != 1)
		return 1;

	    *srow = dstr[0];
	    *scol = dstc[0];
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
	    for (r = 1; VALIDFILE(r); r++) {
		for (c = 1; VALIDFILE(c); c++) {
		    int p = b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

		    if (piece_to_int(p) == KING && val_piece_side(p)) {
			*srow = r;
			*scol = c;

			if (*scol == COLTOINT('e')) {
			    if (status.turn == WHITE)
				wk = 1;
			    else
				bk = 1;
			}

			return 0;
		    }
		}
	    }
	    return 1;
	    break;
	default:
	    return 1;
    }

    return 0;
}

int castle_move(struct board_matrix b[][8], int which)
{
    int row;
    int n;
    int p, p2, p3, p4;

    row = (status.turn == WHITE) ? 1 : 8;
    n = COLTOINT('e');

    if (which == KINGSIDE) {
	if ((status.turn == WHITE && (wk || rkw)) || 
		(status.turn == BLACK && (bk || rkb)))
	    return 1;

	p = b[ROWTOBOARD(row)][COLTOBOARD((n + 1))].icon;
	p2 = b[ROWTOBOARD(row)][COLTOBOARD((n + 2))].icon;
	p3 = b[ROWTOBOARD(row)][COLTOBOARD((n + 3))].icon;

	if (piece_to_int(p) != OPEN_SQUARE || piece_to_int(p2) != OPEN_SQUARE
		|| (piece_to_int(p3) != ROOK && val_piece_side(p3)))
	    return 1;

	b[ROWTOBOARD(row)][COLTOBOARD(COLTOINT('e'))].icon =
	    int_to_piece(OPEN_SQUARE);
	b[ROWTOBOARD(row)][COLTOBOARD(6)].icon = int_to_piece(ROOK);
	b[ROWTOBOARD(row)][COLTOBOARD(7)].icon = int_to_piece(KING);
	b[ROWTOBOARD(row)][COLTOBOARD(8)].icon = int_to_piece(OPEN_SQUARE);

	if (status.turn == WHITE) {
	    wk = rkw = 1;
	    status.notify = "White castles king side";
	}
	else {
	    bk = rkb = 1;
	    status.notify = "Black castles king side";
	}
    }
    else {
	if ((status.turn == WHITE && (wk || rqw)) || (status.turn == BLACK && (bk || rqb)))
	    return 1;

	p = b[ROWTOBOARD(row)][COLTOBOARD((n - 1))].icon;
	p2 = b[ROWTOBOARD(row)][COLTOBOARD((n - 2))].icon;
	p3 = b[ROWTOBOARD(row)][COLTOBOARD((n - 3))].icon;
	p4 = b[ROWTOBOARD(row)][COLTOBOARD((n - 4))].icon;

	if (piece_to_int(p) != OPEN_SQUARE || piece_to_int(p2) != OPEN_SQUARE
		|| piece_to_int(p3) != OPEN_SQUARE ||
		(piece_to_int(p4) != ROOK && val_piece_side(p4)))
	    return 1;

	b[ROWTOBOARD(row)][COLTOBOARD(1)].icon = int_to_piece(OPEN_SQUARE);
	b[ROWTOBOARD(row)][COLTOBOARD(COLTOINT('e'))].icon =
	    int_to_piece(OPEN_SQUARE);
	b[ROWTOBOARD(row)][COLTOBOARD(2)].icon = int_to_piece(KING);
	b[ROWTOBOARD(row)][COLTOBOARD(3)].icon = int_to_piece(ROOK);

	if (status.turn == WHITE) {
	    wk = rqw = 1;
	    status.notify = "White castles queen side";
	}
	else {
	    bk = rqb = 1;
	    status.notify = "Black castles queen side";
	}
    }

    return 0;
}

int parse_move_text(struct board_matrix b[][8], char *move, int reset)
{
    char *p;
    int piece;
    int i;
    int srow, scol, row, col;
    int dist = 0;
    int promo;
    int trow;
    static int enpassant, castle;

    if (reset) {
	enpassant = 0;
	wk = bk = rqw = rkw = rqb = rkb = castle = 0;
    }

    srow = row = col = scol = promo = piece = 0;
    p = (move) + strlen(move);

    while (!isdigit(*--p) && *p != 'O') {
	if (*p == '=') {
	    p++;
	    break;
	}
    }

    *++p = '\0';
    p = move;

    if (strlen(move) < 2)
	return 1;

    /* a2a4 format. */
    if (VALIDCOL(*p) && VALIDROW(*(p + 1)) && VALIDCOL(*(p + 2))
	    && VALIDROW(*(p + 3))) {
	scol = COLTOINT(*p);
	srow = ROWTOINT(*(p + 1));
	col = COLTOINT(*(p + 2));
	row = ROWTOINT(*(p + 3));

	if (p[4]) {
	    if ((promo = piece_to_int(p[4])) == -1)
		return 1;
	}
    }
    /* Pawn. */
    else if (VALIDCOL(*p)) {
	i = 0;

	while (*p) {
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
		if ((promo = piece_to_int(*++p)) == -1)
		    return 1;

		break;
	    }
	    else
		printf("ACK: %c\n", *p++);

	    i++;
	}

	/* a4 format; get the source row and column. */
	if (srow == 0 && scol == col) {
	    trow = (status.turn == WHITE) ? row - 1 : row + 1;

	    while (1) {
		piece = piece_to_int(b[ROWTOBOARD(trow)][COLTOBOARD(col)].icon);

		if (piece == PAWN)
		    break;

		trow += (status.turn == WHITE) ? -1 : 1;

		if (trow > 8 || trow < 1)
		    return 1;

		dist++;
	    }

	    if (piece != PAWN || dist > 2)
		return 1;

	    srow = trow;
	    dist = abs(srow - row);

	    if (status.turn == WHITE) {
		if ((srow == 2 && dist > 2) || (srow > 2 && dist > 1))
		    return 1;
	    }
	    else {
		if ((srow == 7 && dist > 2) || (srow < 7 && dist > 1))
		    return 1;
	    }
	}
	/* Capture or En Passant. */
	else if (scol != col) {
	    srow = (status.turn == WHITE) ? row - 1 : row + 1;

	    if (piece_to_int(b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon)
		    != PAWN)
		return 1;

	    piece = piece_to_int(b[ROWTOBOARD(row)][COLTOBOARD(col)].icon);

	    /* En Passant. */
	    if (piece == OPEN_SQUARE) {
		if (enpassant == 0)
		    return 1;

		trow = (status.turn == WHITE) ? 6 : 3;

		if (row != trow)
		    return 1;
		
		trow = (status.turn == WHITE) ? row - 1 : row + 1;
		piece = piece_to_int(b[ROWTOBOARD(trow)][COLTOBOARD(col)].icon);

		if (piece != PAWN)
		    return 1;

		b[ROWTOBOARD(trow)][COLTOBOARD(col)].icon =
		    int_to_piece(OPEN_SQUARE);

		status.notify = "En Passant";
	    }
	}
    }
    /* Not a pawn. */
    else {
	if (strcmp(move, "O-O") == 0)
	    castle = KINGSIDE;
	else if (strcmp(move, "O-O-O") == 0)
	    castle = QUEENSIDE;
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
		    if (move_from(b, piece, row, col, &srow, &scol))
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
		    if (move_from(b, piece, row, col, &srow, &scol))
			return 1;
		}
	    }
	}
    }

    if (castle) {
	if (castle_move(b, castle))
	    return 1;

	castle = 0;
	goto done;
    }

    piece = piece_to_int(b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon);

    if (piece == PAWN && abs(srow - row) == 2)
	enpassant = 1;
    else
	enpassant = 0;

    piece = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if (piece_to_int(piece) != OPEN_SQUARE) {
	if (val_piece_side(piece))
	    return 2;

	if (piece_side(piece) == WHITE)
	    game[gindex].bcaptures++;
	else
	    game[gindex].wcaptures++;

	status.notify = random_agony();
    }

    if (promo) {
	piece = int_to_piece(promo);
	status.notify = "Promotion!";
    }
    else
	piece = b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;

    b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon = int_to_piece(OPEN_SQUARE);
    b[ROWTOBOARD(row)][COLTOBOARD(col)].icon = piece;

done:
    return 0;
}
