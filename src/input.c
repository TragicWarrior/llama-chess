/* $Id: input.c,v 1.3 2002-12-07 15:54:52 bjk Exp $ */
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
#include "input.h"

static void cleanup(WINDOW *win, PANEL *panel)
{
    del_panel(panel);
    delwin(win);
    return;
}

char *get_input(const char *prompt, char *init)
{
    int y, x, width;
    static char dst[MAXINPUTSIZE];
    int i = 0, pos = 0, len;

    bzero(dst, sizeof(dst));

    nl();
    echo();
    curs_set(1);

    len = strlen(prompt);
    width = (len + 4 > INPUT_WIDTH && len + 4 < COLS - 2) ?
	len + 4 : INPUT_WIDTH;

    x = 1;
    y = 2;

    if (init) {
	len = strlen(init);

	if (len + 4 > width && len + 4 < COLS - 2)
	    width = strlen(init) + 4;

	strncpy(dst, init, sizeof(dst));
	i = pos = strlen(dst);
    }

    while (1) {
	WINDOW *win;
	PANEL *panel;
	int c, n;

	win = newwin(INPUT_HEIGHT, width, CALCPOSY(INPUT_HEIGHT), 
		CALCPOSX(width));
	panel = new_panel(win);
	draw_window_title(win, prompt, width);

	for (n = 0; dst[pos + n]; n++)
	    mvwaddch(win, y, x + n, dst[pos + n]);

	update_panels();
	doupdate();

	c = wgetch(win);

	switch (c) {
	    case KEY_ESCAPE:
		if (init)
		    strncpy(dst, init, sizeof(dst));
		else
		    dst[0] = 0;
	    case '\n':
		cleanup(win, panel);
		goto done;
	    case '\010':
		if (i)
		    dst[--i] = 0;

		if (pos)
		    pos--;

		cleanup(win, panel);
		continue;
	    default:
		break;
	}

	if (i < sizeof(dst) - 1) {
	    dst[i++] = c;

	    if (i >= width - 2)
		pos++;
	}

	cleanup(win, panel);
    }

done:
    noecho();
    nonl();
    curs_set(0);
    return (!dst[0] || dst[0] == '\n') ? NULL : dst;
}
