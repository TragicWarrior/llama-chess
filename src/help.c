/* $Id: help.c,v 1.1.1.1 2002-12-05 20:38:47 bjk Exp $ */
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
#include <panel.h>
#include "help.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

void help(void)
{
    WINDOW *win;
    PANEL *panel;
    int y = 2, x = 0, n;
    int i;

    for (i = 0; i < NARRAY(helptext); i++) {
	if ((n = strlen(helptext[i])) > x)
	    x = n;
    }

    x += 4;
    n = NARRAY(helptext) + 5;

    win = newwin(n, x, LINES / 2 - n / 2, CALCPOSX(x));
    panel = new_panel(win);

    draw_window_title(win, help_title, x);

    for (i = 0; i < NARRAY(helptext); i++)
	mvwprintw(win, y++, 2, "%s", helptext[i]);

    mvwprintw(win, ++y, CENTERX(x, ANYKEY), "%s", ANYKEY);

    update_panels();
    doupdate();

    wgetch(win);

    del_panel(panel);
    delwin(win);

    return;
}
