/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2011 Ben Kibbey <bjk@luxsci.net>

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

static int capture;
static int castle;

enum {
    CHECK = 1,
    CHECK_SELF,
    CHECK_MATE
};

static int check, check_testing;
static int kfile, krank, okfile, okrank;
static int validate_find;

enum {
    WHITEWINS, BLACKWINS, DRAW
};

enum {
    KINGSIDE = 1, QUEENSIDE
};

static int finalize_move(GAME g, BOARD b, int promo, int sfile, int srank, 
	int file, int rank);
static int find_source_square(GAME, BOARD, int, int *, int *, int, int);
static int check_self(GAME g, BOARD b, int file, int rank);
static int validate_pawn(GAME g, BOARD b, int sfile, int srank, int file,
	int rank);
static int find_source_square(GAME, BOARD, int, int *, int *, int, int);

#endif
