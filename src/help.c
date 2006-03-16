/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/* 
    Copyright (C) 2002-2006 Ben Kibbey <bjk@luxsci.net>

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
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "colors.h"

void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);

int help(const char *title, const char *prompt, const char **text)
{
    WINDOW *win;
    PANEL *panel;
    int y = 2, x = 0, n;
    int i;
    int c;

    for (i = 0; text[i]; i++) {
	if ((n = strlen(text[i])) > x)
	    x = n;
    }

    if (x < strlen(prompt))
	x = strlen(prompt);

    x += 4;
    n = i + 4;

    win = newwin(n, x, LINES / 2 - n / 2, CALCPOSX(x));
    panel = new_panel(win);

    wbkgd(win, CP_MESSAGE_WINDOW);
    draw_window_title(win, title, x, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);

    for (i = 0; text[i]; i++)
	mvwprintw(win, y++, 2, "%s", text[i]);

    draw_prompt(win, y, x, prompt, CP_MESSAGE_PROMPT);

    update_panels();
    doupdate();

    c = wgetch(win);

    del_panel(panel);
    delwin(win);

    return c;
}
