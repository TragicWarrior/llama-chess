/* $Id: history.c,v 1.5 2002-12-13 21:55:30 bjk Exp $ */
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
#include <limits.h>
#include <string.h>
#include <panel.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "history.h"

int get_history_by_index(int index, struct history *h)
{
    if (index < 0 || index > game[gindex].htotal - 1)
	return 1;

    *h = game[gindex].history[index];
    return 0;
}

void reset_history()
{
    game[gindex].hindex = game[gindex].htotal = 0;
    return;
}

void add_to_history(struct history **h, int *n, int *t, const char *str)
{
    struct history *history = *h;
    int index = *n;

    history = Realloc(history, (index + 2) * sizeof(struct history));
    strncpy(history[index].move, str, sizeof(history[index].move));

    memset(&history[++index], 0, sizeof(struct history));

    *n = *t = index;
    *h = history;
    return;
}

void move_piece(char *move)
{
    int row, srow;
    int col = 0, scol = 0;
    int n;
    char dst[MAX_MOVE_LEN + 1], *d = dst;
    char src[MAX_MOVE_LEN + 1], *s = src;
    char tsrc[2], *t = tsrc;
    char tdst[2], *tt = tdst;
    char *p = move;

    *s++ = *p++;
    *s++ = *t++ = *p++;
    *s = *t = 0;
    *d++ = *p++;
    *d++ = *tt++ = *p++;
    *d = *tt = 0;
    d -= 2;
    s -= 2;
    t--;
    tt--;

    srow = 8 - (int)strtol(t, NULL, 10);
    row = 8 - (int)strtol(tt, NULL, 10);

    for (n = 0; n < strlen(x_grid_chars); n++) {
	if (s[0] == x_grid_chars[n])
	    scol = n;

	if (d[0] == x_grid_chars[n])
	    col = n;
    }

    if (board[row][col].icon != '.' && !browse_history) {
	if (isupper(board[row][col].icon))
	    game[gindex].bcaptures++;
	else
	    game[gindex].wcaptures++;
    }

    board[row][col].icon = board[srow][scol].icon;
    board[srow][scol].icon = '.';
    return;
}

static void parse_history_move(int index)
{
    int i;

    init_board();

    for (i = 0; i < index; i++) {
	struct history h;

	if (get_history_by_index(i, &h))
	    break;

	move_piece(h.move);
    }

    return;
}

void history_previous(int n)
{
    if (game[gindex].hindex - n < 0)
	game[gindex].hindex = game[gindex].htotal;
    else
	game[gindex].hindex -= n;

    parse_history_move(game[gindex].hindex);
    return;
}

void history_next(int n)
{
    if (game[gindex].hindex + n > game[gindex].htotal)
	game[gindex].hindex = 0;
    else
	game[gindex].hindex += n;

    parse_history_move(game[gindex].hindex);
    return;
}

void init_history()
{
    parse_history_move(game[gindex].hindex);
    status.engine = HISTORY_MODE;
    browse_history = 1;
    update_status();
    return;
}
