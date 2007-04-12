/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2007 Ben Kibbey <bjk@luxsci.net>

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
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_CURSES_H
#include <curses.h>
#endif

#ifdef HAVE_PANEL_H
#include <panel.h>
#endif

#include "misc.h"
#include "window.h"

/*
 * Creates a new window on the 'wins' stack. Returns the newly create window
 * structure. The 'func' parameter is a function pointer that is called from
 * game_loop(). 'efunc' is called just before the window is destroyed.
 */
WIN *window_create(const char *title, int h, int w, int y, int x, 
	window_func func, void *data, window_exit_func efunc)
{
    int i = 0;

    if (wins)
	for (i = 0; wins[i]; i++);

    wins = Realloc(wins, (i + 2) * sizeof(WIN *));
    wins[i] = Calloc(1, sizeof(WIN));
    wins[i]->w = newwin(h, w, y, x);
    wins[i]->p = new_panel(wins[i]->w);
    wins[i]->data = data;
    wins[i]->rows = h;
    wins[i]->cols = w;
    wins[i]->func = func;
    wins[i]->efunc = efunc;
    wins[i]->title = (title) ? strdup(title) : NULL;
    wins[i+1] = NULL;
    return wins[i];
}

void window_destroy(WIN *win)
{
    int i, n;
    WIN **new = NULL;

    if (!wins)
	return;

    for (i = 0; wins[i]; i++);

    while (i > 0 && wins[--i]->keep) {
	if (win && memcmp(win, wins[i], sizeof(WIN)) == 0)
	    win = NULL;

	if (wins[i]->title)
	    free(wins[i]->title);

	if (wins[i]->p)
	    del_panel(wins[i]->p);

	delwin(wins[i]->w);

	if (wins[i]->freedata && wins[i]->data)
	    free(wins[i]->data);

	free(wins[i]);
	wins[i] = NULL;
    }

    for (i = n = 0; wins[i]; i++) {
	if (win && !win->keep && memcmp(win, wins[i], sizeof(WIN)) == 0) {
	    if (win->p)
		del_panel(win->p);

	    if (win->title)
		free(win->title);

	    delwin(win->w);
	    free(win);
	    continue;
	}

	if (win && win->keep && memcmp(win, wins[i], sizeof(WIN)) == 0) {
	    del_panel(win->p);
	    win->p = NULL;
	}

	new = Realloc(new, (n + 2) * sizeof(WIN *));
	new[n] = wins[i];
	n++;
    }

    free(wins);
    wins = NULL;

    if (new) {
	if (n) {
	    new[n] = NULL;
	    wins = new;
	}
	else
	    free(new);
    }
}

void window_draw_title(WINDOW *win, const char *title, int width, chtype attr,
	chtype battr)
{
    int i;

    if (title) {
	const char *p;

	wattron(win, attr);

	for (i = 1; i < width - 1; i++)
	    mvwaddch(win, 1, i, ' ');

	if (strlen(title) > width) {
	    p = str_etc(title, width - 2, 1);
	}
	else
	    p = title;

	mvwprintw(win, 1, CENTERX(width, p), "%s", p);
	wattroff(win, attr);
    }

    wattron(win, battr);
    box(win, ACS_VLINE, ACS_HLINE);
    wattroff(win, battr);
}

void window_draw_prompt(WINDOW *win, int y, int width, const char *str,
	chtype attr)
{
    int i;

    wattron(win, attr);

    for (i = 1; i < width - 1; i++)
	mvwaddch(win, y, i, ' ');

    mvwprintw(win, y, CENTERX(width, str), "%s", str);
    wattroff(win, attr);
}
