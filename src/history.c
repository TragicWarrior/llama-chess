/* $Id: history.c,v 1.4 2002-12-12 15:07:49 bjk Exp $ */
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
#include <panel.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "history.h"

char *get_history_by_index(int index)
{
    if (index < 0 || index > game[gindex].htotal - 1)
	return "none";

    return game[gindex].history[index].move;
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
