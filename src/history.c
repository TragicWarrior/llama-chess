/* $Id: history.c,v 1.3 2002-12-11 17:45:17 bjk Exp $ */
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
    if (index < 0 || index > history_total - 1)
	return "none";

    return history[index].move;
}

void reset_history()
{
    history_index = history_total = 0;
    return;
}

void update_history()
{
    char buf[16];

    if (history_total)
	snprintf(buf, sizeof(buf), "%u%s of %u", history_index, 
		(history[history_index].comment[0]) ? "*" : "",
		history_total);
    else
	strncpy(buf, UNKNOWN, sizeof(buf));

    mvwprintw(historyw, 2, 1, "     Move: %-*s", HISTORY_WIDTH - 13, buf);
    mvwprintw(historyw, 3, 1, "Next move: %-*s", HISTORY_WIDTH - 13, 
	    get_history_by_index(history_index));
    mvwprintw(historyw, 4, 1, "Last move: %-*s", HISTORY_WIDTH - 13,
	    get_history_by_index(history_index - 1));
    return;
}

void add_to_history(int *n, const char *str)
{
    int index = *n;

    history = Realloc(history, (index + 2) * sizeof(struct history_s));
    strncpy(history[index].move, str, sizeof(history[index].move));

    history_total = index + 1;
    memset(&history[index + 1], 0, sizeof(struct history_s));

    *n = ++index;
    return;
}
