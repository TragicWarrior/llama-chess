/* $Id: move.c,v 1.1 2002-12-30 18:59:19 bjk Exp $ */
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
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "move.h"

static int piece_to_int(chtype piece)
{
    int p = piece & A_CHARTEXT;

    switch (p) {
	case 'P':
	case 'p':
	    return PAWN;
	case 'R':
	case 'r':
	    return ROOK;
	case 'N':
	case 'n':
	    return KNIGHT;
	case 'B':
	case 'b':
	    return BISHOP;
	case 'Q':
	case 'q':
	    return QUEEN;
	case 'K':
	case 'k':
	    return KING;
	case '.':
	    return OPEN_SQUARE;
	default:
	    break;
    }

    return -1;
}

int validate_move(struct board_matrix matrix[][8], char *str)
{
    char *p = (str) + strlen(str);
    char move[MAX_PGN_MOVE_LEN + 1] = {0}, *pm = move;
    int from = 0, to = 0;

    /* FIXME En Passant from engine. */
    /* Remove trailing things from the move like check etc... */
    while (*--p != 'O' && !isdigit(*p))
	*p = 0;

    /* a2a4 format. */
    if (strlen(str) >= 4 && VALIDCOL(*str) && VALIDROW(*(str + 1)) &&
	    VALIDCOL(*(str + 2)) && VALIDROW(*(str + 3))) {
	from = piece_to_int(matrix[ROWTOBOARD(*(str + 1))][COLTOBOARD(*str)].icon);
	to = piece_to_int(matrix[ROWTOBOARD(*(str + 3))][COLTOBOARD(*(str + 2))].icon);
    }

    return 0;
}
