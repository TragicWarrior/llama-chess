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

#include "common.h"
#include "move.h"

int int_to_piece(char turn, int which)
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

    return (turn == WHITE) ? toupper(p) : p;
}

int piece_side(GAME g, int c)
{
    if (c == int_to_piece(g.turn, OPEN_SQUARE))
	return -1;
    
    if (c < 'A')
	c = int_to_piece(g.turn, c);

    return (isupper(c)) ? WHITE : BLACK;
}

int val_piece_side(GAME g, int c)
{
    if ((isupper(c) && g.turn == WHITE) ||
	    (islower(c) && g.turn == BLACK))
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
int piece_by_col(GAME g, int piece, int row, int col, int *r, int *c)
{
    int i;
    int count = 0;

    for (i = col - 1; VALIDFILE(i); i--) {
	int n = g.b[ROWTOBOARD(row)][COLTOBOARD(i)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(g, n)) {
		*c = i;
		*r = row;
		count++;
	    }

	    break;
	}
    }

    for (i = col + 1; VALIDFILE(i); i++) {
	int n = g.b[ROWTOBOARD(row)][COLTOBOARD(i)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(g, n)) {
		*c = i;
		*r = row;
		count++;
	    }

	    break;
	}
    }

    return count;
}

int piece_by_row(GAME g, int piece, int row, int col, int *r, int *c)
{
    int i;
    int count = 0;

    for (i = row + 1; VALIDFILE(i); i++) {
	int n = g.b[ROWTOBOARD(i)][COLTOBOARD(col)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(g, n)) {
		*r = i;
		*c = col;
		count++;
	    }

	    break;
	}
    }

    for (i = row - 1; VALIDFILE(i); i--) {
	int n = g.b[ROWTOBOARD(i)][COLTOBOARD(col)].icon;

	if (piece_to_int(n) != OPEN_SQUARE) {
	    if (piece_to_int(n) == piece && val_piece_side(g, n)) {
		*r = i;
		*c = col;
		count++;
	    }

	    break;
	}
    }

    return count;
}

int piece_by_xy(GAME g, int piece, int row, int col, int *srow, int *scol)
{
    int count = 0;

    count = piece_by_row(g, piece, row, col, srow, scol);
    count += piece_by_col(g, piece, row, col, srow, scol);
    return (count != 1) ? 0 : 1;
}

int piece_test(GAME g, int piece, int row, int col, int *dstr, int *dstc)
{
    int p;

    if (!VALIDFILE(row) || !VALIDFILE(col))
	return 2;

    p = g.b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if (piece_to_int(p) != OPEN_SQUARE) {
	if (piece_to_int(p) == piece && val_piece_side(g, p)) {
	    *dstr = row;
	    *dstc = col;
	    return 1;
	}

	return 2;
    }

    return 0;
}

int piece_by_diag(GAME g, int piece, int row, int col, int *srow, int *scol)
{
    int i, n;
    int ul = 1, ur = 1, dl = 1, dr = 1;
    int count = 0;

    for (i = 1; VALIDFILE(i); i++) {
	if (dr) {
	    n = piece_test(g, piece, row - i, col + i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		dr = 0;
	}

	if (dl) {
	    n = piece_test(g, piece, row - i, col - i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		dl = 0;
	} 

	if (ur) {
	    n = piece_test(g, piece, row + i, col + i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		ur = 0;
	}

	if (ul) {
	    n = piece_test(g, piece, row + i, col - i, srow, scol);

	    if (n == 1 && count++)
		return 1;
	    else if (n == 2)
		ul = 0;
	}
    }

    return (count) ? 1 : 0;
}

int valid_move(GAME g, int row, int col, int srow, int scol)
{
    int p1, p2;

    if (!VALIDFILE(srow) || !VALIDFILE(scol))
	return 0;

    if (row == srow && col == scol)
	return 0;

    p1 = g.b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;
    p2 = g.b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if (piece_to_int(p1) == OPEN_SQUARE)
	return 0;

    if (piece_side(g, p1) == piece_side(g, p2))
	return 0;

    if (piece_to_int(p1) == PAWN && scol == col &&
	    piece_to_int(p2) != OPEN_SQUARE)
	return 0;

    return 1;
}

int castle_move(GAME *g, char which)
{
    int row;
    int n;
    int p, p2, p3, p4;

    row = ((*g).turn == WHITE) ? 1 : 8;
    n = COLTOINT('e');

    if (which == KINGSIDE) {
	if (((*g).turn == WHITE && (!TEST_FLAG((*g).flags, GF_WK_CASTLE))) ||
		((*g).turn == BLACK && (!TEST_FLAG((*g).flags, GF_BK_CASTLE)))) 
	    return 1;

	p = (*g).b[ROWTOBOARD(row)][COLTOBOARD((n + 1))].icon;
	p2 = (*g).b[ROWTOBOARD(row)][COLTOBOARD((n + 2))].icon;
	p3 = (*g).b[ROWTOBOARD(row)][COLTOBOARD((n + 3))].icon;

	if (piece_to_int(p) != OPEN_SQUARE || piece_to_int(p2) != OPEN_SQUARE
		|| (piece_to_int(p3) != ROOK && val_piece_side(*g, p3)))
	    return 1;

	if (!validate) {
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(COLTOINT('e'))].icon =
		int_to_piece((*g).turn, OPEN_SQUARE);
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(6)].icon = int_to_piece((*g).turn, ROOK);
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(7)].icon = int_to_piece((*g).turn, KING);
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(8)].icon = int_to_piece((*g).turn, OPEN_SQUARE);

	    if ((*g).turn == WHITE) {
		CLEAR_FLAG((*g).flags, GF_WK_CASTLE);
		update_status_notify(*g, "%s", NOTIFY_WCASTLEK);
	    }
	    else if ((*g).turn == BLACK) {
		CLEAR_FLAG((*g).flags, GF_BK_CASTLE);
		update_status_notify(*g, "%s", NOTIFY_BCASTLEK);
	    }
	}
    }
    else {
	if (((*g).turn == WHITE && (!TEST_FLAG((*g).flags, GF_WQ_CASTLE))) ||
		((*g).turn == BLACK && (!TEST_FLAG((*g).flags, GF_BQ_CASTLE)))) 
	    return 1;

	p = (*g).b[ROWTOBOARD(row)][COLTOBOARD((n - 1))].icon;
	p2 = (*g).b[ROWTOBOARD(row)][COLTOBOARD((n - 2))].icon;
	p3 = (*g).b[ROWTOBOARD(row)][COLTOBOARD((n - 3))].icon;
	p4 = (*g).b[ROWTOBOARD(row)][COLTOBOARD((n - 4))].icon;

	if (piece_to_int(p) != OPEN_SQUARE || piece_to_int(p2) != OPEN_SQUARE
		|| piece_to_int(p3) != OPEN_SQUARE ||
		(piece_to_int(p4) != ROOK && val_piece_side(*g, p4)))
	    return 1;

	if (!validate) {
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(1)].icon = int_to_piece((*g).turn, OPEN_SQUARE);
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(COLTOINT('e'))].icon =
		int_to_piece((*g).turn, OPEN_SQUARE);
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(3)].icon = int_to_piece((*g).turn, KING);
	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(4)].icon = int_to_piece((*g).turn, ROOK);

	    if ((*g).turn == WHITE) {
		CLEAR_FLAG((*g).flags, GF_WQ_CASTLE);
		update_status_notify(*g, "%s", NOTIFY_WCASTLEQ);
	    }
	    else if ((*g).turn == BLACK) {
		CLEAR_FLAG((*g).flags, GF_BQ_CASTLE);
		update_status_notify(*g, "%s", NOTIFY_BCASTLEQ);
	    }
	}
    }

    return 0;
}

int get_source_yx(GAME *g, int piece, int row, int col, int *srow, int *scol)
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
		i = ((*g).turn == WHITE) ? -1 : 1;

		/* Find the first pawn in the current column. */
		for (r = row + i, dist = 0; VALIDFILE(r); r += i, dist++) {
		    int n = (*g).b[ROWTOBOARD(r)][COLTOBOARD(col)].icon;

		    p = piece_to_int(n);

		    if (p == PAWN && val_piece_side(*g, n))
			break;
		}

		if (p != PAWN || dist > 2)
		    return 1;

		*srow = r;
		dist = abs(*srow - row);

		if ((*g).turn == WHITE) {
		    if ((*srow == 2 && dist > 2) || (*srow > 2 && dist > 1))
			return 1;
		}
		else {
		    if ((*srow == 7 && dist > 2) || (*srow < 7 && dist > 1))
			return 1;
		}

		if (dist == 2) {
		    p = piece_to_int((*g).b[ROWTOBOARD(*srow + i)][COLTOBOARD(col)].icon);
		    if (p != OPEN_SQUARE)
			return 1;
		}
	    }
	    else if (*scol != col) {
		if (abs(*scol - col) != 1)
		    return 1;

		*srow = ((*g).turn == WHITE) ? row - 1 : row + 1;

		if (piece_to_int((*g).b[ROWTOBOARD(*srow)][COLTOBOARD(*scol)].icon)
			!= PAWN)
		    return 1;

		piece = piece_to_int((*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].icon);

		/* En Passant. */
		if (piece == OPEN_SQUARE) {
		    /* Previous move was not 2 squares and a pawn. */
		    if (!TEST_FLAG((*g).flags, GF_ENPASSANT))
			return 1;

		    if (!(*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].enpassant)
			return 1;

		    r = ((*g).turn == WHITE) ? 6 : 3;

		    if (row != r)
			return 1;

		    r = ((*g).turn == WHITE) ? row - 1 : row + 1;
		    piece = (*g).b[ROWTOBOARD(r)][COLTOBOARD(col)].icon;

		    if (piece_to_int(piece) != PAWN)
			return 1;

		    if (!validate) {
			(*g).b[ROWTOBOARD(r)][COLTOBOARD(col)].icon =
			    int_to_piece((*g).turn, OPEN_SQUARE);

			if (((*g).turn == WHITE && (*g).side != WHITE) ||
				((*g).turn == BLACK && (*g).side != BLACK))
			    update_status_notify(*g, "%s", NOTIFY_ENPASSANT);
		    }
		}
	    }
	    break;
	case ROOK:
	    if (piece_by_xy(*g, ROOK, row, col, srow, scol) == 0)
		    return 1;

	    if (!validate && *scol == 1) {
		if ((*g).turn == WHITE)
		    CLEAR_FLAG((*g).flags, GF_WQ_CASTLE);
		else
		    CLEAR_FLAG((*g).flags, GF_BQ_CASTLE);
	    }
	    else if (!validate && *scol == 8) {
		if ((*g).turn == WHITE)
		    CLEAR_FLAG((*g).flags, GF_WK_CASTLE);
		else
		    CLEAR_FLAG((*g).flags, GF_BK_CASTLE);
	    }
	    break;
	case KNIGHT:
	    r = row - 2;
	    c = col - 1;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row - 2;
	    c = col + 1;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {

		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 2;
	    c = col - 1;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 2;
	    c = col + 1;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row - 1;
	    c = col - 2;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row - 1;
	    c = col + 2;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 1;
	    c = col + 2;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    r = row + 1;
	    c = col - 2;
	    p = (*g).b[ROWTOBOARD(r)][COLTOBOARD(c)].icon;

	    if (VALIDFILE(r) && VALIDFILE(c)) {
		if (piece_to_int(p) == KNIGHT && val_piece_side(*g, p)) {
		    *srow = r;
		    *scol = c;
		    count++;

		    if ((*srow && *srow == row) || (*scol && *scol == col))
			break;
		}
	    }

	    if ((count != 1 && !validate) || (validate && !count))
		return 1;

	    break;
	case BISHOP:
	    if (piece_by_diag(*g, BISHOP, row, col, srow, scol) == 0)
		return 1;
	    break;
	case QUEEN:
	    if (piece_by_xy(*g, QUEEN, row, col, srow, scol) == 0) {
		if (piece_by_diag(*g, QUEEN, row, col, srow, scol) == 0)
		    return 1;
	    }
	    break;
	case KING:
	    if (piece_by_xy(*g, KING, row, col, srow, scol) == 0) {
		if (piece_by_diag(*g, KING, row, col, srow, scol) == 0)
		    return 1;
	    }

	    if (abs(*srow - row) > 1)
		return 1;

	    dist = abs(*scol - col);

	    if (*scol == COLTOINT('e')) {
		if (dist > 2)
		    return 1;

		if (validate) {
		    if (dist == 2) {
			if (col == 3) {
			    if (castle_move(g, QUEENSIDE))
				return 1;
			}
			else if (col == 7) {
			    if (castle_move(g, KINGSIDE))
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
	    }

	    if (dist > 1)
		return 1;

	    break;
	default:
	    return 1;
    }

    if (valid_move(*g, row, col, *srow, *scol) == 0)
	return 1;

    if (piece == KING) {
	if (!validate) {
	    if ((*g).turn == WHITE) {
		CLEAR_FLAG((*g).flags, GF_WK_CASTLE);
		CLEAR_FLAG((*g).flags, GF_WQ_CASTLE);
	    }
	    else {
		CLEAR_FLAG((*g).flags, GF_BK_CASTLE);
		CLEAR_FLAG((*g).flags, GF_BQ_CASTLE);
	    }
	}
    }

    return 0;
}

/* This function converts a2a4 formatted moves to SAN format. Minimal checks
 * are performed here. The real checks are in parse_move_text() after the
 * conversion.
 */
char *a2a4tosan(GAME *g, char *m)
{
    static char buf[MAX_SAN_MOVE_LEN + 1] = {0}, *cp = buf;
    char *p = m;
    int scol, srow, col, row;
    int piece, piecei, spiece;
    int trow, tcol;
    int rowc, colc;
    int promo = 0;
    int tenpassant = 0;
    int n;

    // Not in a2a4 format. Probably already in SAN format.
    if (!VALIDCOL(*p) || !VALIDROW(*(p + 1)) || !VALIDCOL(*(p + 2))
	    || !VALIDROW(*(p + 3)))
	return m;

    scol = COLTOINT(*p);
    srow = ROWTOINT(*(p + 1));
    col = COLTOINT(*(p + 2));
    row = ROWTOINT(*(p + 3));

    if (p[4]) {
	if ((promo = piece_to_int(p[4])) == -1)
	    return NULL;
    }

    piece = (*g).b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;

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
	if (scol != col && piece_to_int((*g).b[row][col].icon) == OPEN_SQUARE)
	    tenpassant = 1;
    }

    colc = piece_by_col(*g, piecei, row, col, &trow, &tcol);
    rowc = piece_by_row(*g, piecei, row, col, &trow, &tcol);
    n = colc + rowc;

    if (piecei == KNIGHT) {
	if (get_source_yx(g, KNIGHT, row, col, &trow, &tcol) == 1)
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

    piece = (*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if ((piecei = piece_to_int(piece)) != OPEN_SQUARE || tenpassant) {
	if (tenpassant || spiece == PAWN)
	    *cp++ = INTTOCOL(scol);

	*cp++ = 'x';
    }

    *cp++ = INTTOCOL(col);
    *cp++ = INTTOROW(row);

    if (promo) {
	*cp++ = '=';
	*cp++ = toupper(int_to_piece((*g).turn, promo));
    }

    *cp = '\0';
    return buf;
}

void switch_turn(GAME *g)
{
    if ((*g).turn == WHITE)
	(*g).turn = BLACK;
    else
	(*g).turn = WHITE;
}

static void kingsquare(GAME g, int *kr, int *kc, int *okr, int *okc)
{
    int row, col;

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int p = g.b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

	    if (piece_to_int(p) == KING) {
		if (val_piece_side(g, p)) {
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

int checktest(GAME *g, int kr, int kc, int okr, int okc, int matetest)
{
    int row, col;

    switch_turn(&(*g));

    /* See if the move would put our opponent in check. */
    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int srow = 0, scol = 0;
	    int p = (*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;
	    int pi = piece_to_int(p);

	    if (pi == OPEN_SQUARE)
		continue;

	    if (pi == PAWN)
		scol = col;

	    /* See if the move would leave ourselves in check. */
	    if (!matetest) {
		switch_turn(&(*g));

		if (get_source_yx(g, pi, kr, kc, &srow, &scol) == 0)
		    return -1;

		switch_turn(&(*g));
	    }

	    if (get_source_yx(g, pi, okr, okc, &srow, &scol) == 0) {
		switch_turn(&(*g));
		return 1;
	    }
	}
    }

    switch_turn(&(*g));
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
}

void get_valid_moves(GAME *g, int p, int srow, int scol, int *minr, int *maxr,
	int *minc, int *maxc)
{
    int row, col;

    validate = 1;
    *minr = *maxr = *minc = *maxc = 1;

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int sr = 0, sc = 0;

	    if (get_source_yx(g, p, row, col, &sr, &sc)) {
		sr = 0;
		sc = scol;

		if (get_source_yx(g, p, row, col, &sr, &sc)) {
		    sc = 0;
		    sr = srow;

		    if (get_source_yx(g, p, row, col, &sr, &sc)) {
			continue;
		    }
		}
	    }

	    if (sr != srow || sc != scol)
		continue;

	    (*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].valid = 1;

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

    validate = 0;
}

static int checkmate_pawn_test(GAME *g, int row, int col, int *srow, int *scol)
{
    int r, c;

    r = 0;
    c = col;

    if (!get_source_yx(g, PAWN, row, col, &r, &c))
	return 0;

    c = col - 1;

    if (!get_source_yx(g, PAWN, row, col, &r, &c))
	return 0;

    c = col + 1;

    if (!get_source_yx(g, PAWN, row, col, &r, &c))
	return 0;

    return 1;
}

static int checkmatetest(GAME *g, int kr, int kc, int okr, int okc)
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
		    if (checkmate_pawn_test(g, row, col, &srow, &scol))
			continue;
		}
		else {
		    if (get_source_yx(g, n, row, col, &srow, &scol))
			continue;
		}

		/* Valid move. */
		memcpy(oldboard, (*g).b, sizeof(BOARD));
		p = (*g).b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;
		(*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].icon = p;
		(*g).b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon = 
		    int_to_piece((*g).turn, OPEN_SQUARE);

		if (piece_to_int(p) == KING) {
		    if (piece_side(*g, p) == (*g).turn) {
			nokr = row;
			nokc = col;
		    }
		    else {
			nkr = row;
			nkc = col;
		    }
		}

		check = checktest(g, nkr, nkc, nokr, nokc, 1);
		memcpy((*g).b, oldboard, sizeof(BOARD));

		if (check == 0)
		    goto done;
	    }
	}
    }

    check = 1;

    if ((*g).turn == WHITE)
	result = BLACKWINS;
    else
	result = WHITEWINS;

done:
    return (check != 0) ? 1 : 0;
}

/* FIXME */
static int drawtest(BOARD b)
{
    int row, col;
    int other = 0;

    /*
    if (game[gindex].ply >= 50)
	return 1;
	*/

    for (row = 1; VALIDFILE(row); row++) {
	for (col = 1; VALIDFILE(col); col++) {
	    int p = b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;
	    int piece = piece_to_int(p);

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

static void reset_enpassant(BOARD b)
{
    int r, c;

    for (r = 0; r < 8; r++) {
	for (c = 0; c < 8; c++)
	    b[r][c].enpassant = 0;
    }
}

int parse_move_text(GAME *g, char *m)
{
    char *p;
    int piece, dstpiece;
    int i = 0;
    int srow = 0, scol = 0, row, col;
    int dist = 0;
    int promo = -1;
    int kr = 0, kc = 0, okr = 0, okc = 0;
    int plyincr = 0;

    if (strlen(m) < 2)
	return 1;

    capture = 0;
    update_status_notify(*g, NULL);
    srow = row = col = scol = promo = piece = 0;
again:
    p = (m) + strlen(m);

    while (!isdigit(*--p) && *p != 'O') {
	if (*p == '=') {
	    promo = piece_to_int(i);
	    i = 0;
	    break;
	}

	i = *p;
	*p = '\0';
    }

    // Old promotion text (e8Q). Convert to SAN.
    if (piece_to_int(i) != -1) {
	p = (m) + strlen(m);
	*p++ = '=';
	*p++ = i;
	*p = '\0';
	goto again;
    }

    if (strlen(m) < 2)
	return 1;

    p = m;

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
		plyincr++;
		capture++;
	    }
	    else if (*p == '=') {
		if (promo == -1 || promo == KING || promo == PAWN)
		    return 1;

		*p++ = '=';
		*p++ = toupper(int_to_piece((*g).turn, promo));
		*p = '\0';
		break;
	    }
	    else {
#ifdef DEBUG
		if (debug)
		    DUMP("Pawn (move: '%s'): %c\n", m, *p++);
#else
		p++;
#endif
	    }
	}

	if (get_source_yx(g, PAWN, row, col, &srow, &scol))
	    return 1;
    }
    /* Not a pawn. */
    else {
	if (strcmp(m, "O-O") == 0)
	    (*g).castle = KINGSIDE;
	else if (strcmp(m, "O-O-O") == 0)
	    (*g).castle = QUEENSIDE;
	else {
	    p = m;

	    if ((piece = piece_to_int(*p++)) == -1)
		return 1;

	    if (strlen(m) > 3) {
		if (isdigit(*p))
		    srow = ROWTOINT(*p++);
		else if (VALIDCOL(*p))
		    scol = COLTOINT(*p++);

		if (*p == 'x') {
		    capture++;
		    p++;
		}
	    }

	    col = COLTOINT(*p++);
	    row = ROWTOINT(*p++);

	    /* Get the source row and column. */
	    if (srow == 0) {
		if (scol > 0) {
		    for (i = 1; VALIDFILE(i); i++) {
			int fpiece = (*g).b[ROWTOBOARD(i)][COLTOBOARD(scol)].icon;

			if (piece == piece_to_int(fpiece) && 
				val_piece_side(*g, fpiece)) {
			    srow = i;
			    break;
			}
		    }

		    if (srow == 0)
			return 1;
		}
		else {
		    if (get_source_yx(g, piece, row, col, &srow, &scol))
			return 1;
		}
	    }
	    else if (scol == 0) {
		if (srow > 0) {
		    for (i = 1; VALIDFILE(i); i++) {
			int fpiece = piece_to_int((*g).b[ROWTOBOARD(srow)][COLTOBOARD(i)].icon);

			if (piece == fpiece) {
			    scol = i;
			    break;
			}
		    }

		    if (scol == 0)
			return 1;
		}
		else {
		    if (get_source_yx(g, piece, row, col, &srow, &scol))
			return 1;
		}
	    }
	}
    }

    piece = piece_to_int((*g).b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon);
    dist = abs(srow - row);

    if (!validate) {
	reset_enpassant((*g).b);

	if (piece == PAWN && dist == 2) {
	    if ((*g).turn == WHITE)
		(*g).b[ROWTOBOARD(srow - 1)][COLTOBOARD(scol)].enpassant = 1;
	    else
		(*g).b[ROWTOBOARD(srow + 1)][COLTOBOARD(scol)].enpassant = 1;

	    SET_FLAG((*g).flags, GF_ENPASSANT);
	}
	else {
	    CLEAR_FLAG((*g).flags, GF_ENPASSANT);
	}
    }

    if (piece == PAWN)
	plyincr++;

    dstpiece = piece = (*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].icon;

    if ((*g).castle) {
	if (castle_move(g, (*g).castle)) {
	    (*g).castle = 0;
	    return 1;
	}

	goto done;
    }

    if (piece_to_int(piece) != OPEN_SQUARE) {
	if (val_piece_side(*g, piece))
	    return 2;

	if (!validate) {
	    if (piece_side(*g, piece) == WHITE)
		(*g).bcaptures++;
	    else
		(*g).wcaptures++;

	    update_status_notify(*g, random_agony(*g));
	}
    }

    if (!validate) {
	if (promo) {
	    piece = int_to_piece((*g).turn, promo);

	    if (((*g).turn == WHITE && (*g).side != WHITE) ||
		    ((*g).turn == BLACK && (*g).side != BLACK))
		update_status_notify(*g, "%s", NOTIFY_PROMOTION);
	}
	else 
	    piece = (*g).b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;
    }
    else 
	piece = (*g).b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon;

    (*g).b[ROWTOBOARD(srow)][COLTOBOARD(scol)].icon = int_to_piece((*g).turn, OPEN_SQUARE);
    (*g).b[ROWTOBOARD(row)][COLTOBOARD(col)].icon = piece;

done:
    if (!validate && capture && piece_to_int(dstpiece) == ROOK) {
	if (row == 1 && col == 1)
	    CLEAR_FLAG((*g).flags, GF_WQ_CASTLE);
	else if (row == 8 && col == 1)
	    CLEAR_FLAG((*g).flags, GF_BQ_CASTLE);
	else if (row == 1 && col == 8)
	    CLEAR_FLAG((*g).flags, GF_WK_CASTLE);
	else if (row == 8 && col == 8)
	    CLEAR_FLAG((*g).flags, GF_BK_CASTLE);
    }

    kingsquare(*g, &kr, &kc, &okr, &okc);
    switch_turn(&(*g));

    if ((*g).castle) {
	p = m + strlen(m);
	(*g).castle = 0;
    }

    CLEAR_FLAG((*g).flags, GF_GAMEOVER);
    i = validate;
    validate = 1;

    if (!plyincr)
	(*g).ply++;
    else
	(*g).ply = 0;

    if (drawtest((*g).b)) {
	(*g).tag[TAG_RESULT].value = Realloc((*g).tag[TAG_RESULT].value, 8);
	strncpy((*g).tag[TAG_RESULT].value, "1/2-1/2", 8);
	update_status_notify(*g, "%s", NOTIFY_GAMEOVER_DRAW);

	if (curses_initialized)
	    update_tag_window((*g).tag);

	SET_FLAG((*g).flags, GF_GAMEOVER);
    }
    else {
	switch (checktest(g, kr, kc, okr, okc, 0)) {
	    case 0:
		break;
	    case -1:
		validate = i;
		switch_turn(&(*g));
		return 1;
	    default:
		if (checkmatetest(g, kr, kc, okr, okc)) {
		    *p++ = '#';

		    if (result == WHITEWINS) {
			(*g).tag[TAG_RESULT].value = Realloc((*g).tag[TAG_RESULT].value, 4);
			strncpy((*g).tag[TAG_RESULT].value, "1-0", 4);
			update_status_notify(*g, "%s", NOTIFY_GAMEOVER_WWINS);
		    }
		    else if (result == BLACKWINS) {
			(*g).tag[TAG_RESULT].value = Realloc((*g).tag[TAG_RESULT].value, 4);
			strncpy((*g).tag[TAG_RESULT].value, "0-1", 4);
			update_status_notify(*g, "%s", NOTIFY_GAMEOVER_BWINS);
		    }

		    if (curses_initialized)
			update_tag_window((*g).tag);

		    SET_FLAG((*g).flags, GF_GAMEOVER);
		}
		else {
		    *p++ = '+';

		    if (((*g).turn == WHITE && (*g).side == WHITE) ||
			    ((*g).turn == BLACK && (*g).side == BLACK))
			update_status_notify(*g, "%s", NOTIFY_CHECK);
		}

		*p = '\0';
		break;
	}
    }

    switch_turn(&(*g));
    validate = i;
    return 0;
}
