/* $Id: input.c,v 1.1.1.1 2002-12-05 20:38:47 bjk Exp $ */
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

#define MAXINPUTSIZE	32

static void updateinput(WINDOW *win, const char str[], size_t size, unsigned y, int x)
{
    int n = 0;

    wmove(win, y, x);

    while (n != size - 1)
	mvwaddch(win, y, x + n++, ' ');

    mvwaddstr(win, y, x, str);
    return;
}

char *get_input(const char *prompt)
{
    WINDOW *win;
    PANEL *p;
    int x = strlen(prompt) + 1 + MAXINPUTSIZE+ 4;
    static char dst[MAXINPUTSIZE];
    int c, i = 0;
    int y = 2;

    bzero(dst, sizeof(dst));
    win = newwin(4, x, CALCPOSY(3), CALCPOSX(x));
    p = new_panel(win);

    draw_window_title(win, prompt, x);

    nl();
    echo();
    curs_set(1);

    x = 1;

    wmove(win, y, x);
    update_panels();
    doupdate();

    while ((c = wgetch(win)) != '\n' && c != ERR) {
	if (c == KEY_ESCAPE) {
	    dst[0] = 0;
	    break;
	}

	updateinput(win, dst, sizeof(dst), y, x);

	if (c == '\010') {
	    if (!i)
		continue;

	    dst[--i] = 0;
	    updateinput(win, dst, sizeof(dst), y, x);
	    continue;
	}


	if (i < sizeof(dst))
	    dst[i++] = c;
	else
	    beep();

	updateinput(win, dst, sizeof(dst), y, x);
    }

    del_panel(p);
    delwin(win);
    noecho();
    nonl();
    curs_set(0);
    return (!dst[0] || dst[0] == '\n') ? NULL : dst;
}
