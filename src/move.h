/* $Id: move.h,v 1.4 2003-01-07 20:35:21 bjk Exp $ */
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
#ifndef MOVE_H
#define MOVE_H

#define VALIDFILE(f)	((f >= 1 && f <= 8) ? 1 : 0)
#define VALIDROW(r)	((r >= '1' && r <= '8') ? 1 : 0)
#define VALIDCOL(c)	((c >= 'a' && c <= 'h') ? 1 : 0)
#define ROWTOINT(r)	(r - '0')
#define COLTOINT(c)	(c - ('a' - 1))
#define INTTOROW(r)	(r + '0')
#define INTTOCOL(c)	(c + ('a' - 1))
#define ROWTOBOARD(r)	(8 - r)
#define COLTOBOARD(c)	(c - 1)

enum {
    OPEN_SQUARE, PAWN, BISHOP, ROOK, KNIGHT, QUEEN, KING
};

enum {
    KINGSIDE = 1, QUEENSIDE
};

/* These are used to determine whether a castle is legal.
 */
int wk, bk, rqw, rkw, rqb, rkb;

char *random_agony(void);

#endif
