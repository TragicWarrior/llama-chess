/* $Id: message.c,v 1.1 2002-12-05 20:38:47 bjk Exp $ */
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
#include <stdarg.h>
#include <string.h>
#include <panel.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

#define CALCWIDTH(str)	(strlen(str) + 4)
#define CALCHEIGHT()	(4)

int message(const char *title, const char *prompt, const char *format, ...)
{
    va_list ap;
    char *line, *s, *tmp, buf[LINE_MAX];
    WINDOW *win;
    PANEL *p;
    int n;
    int x, y, c = 0, i = 0;

    va_start(ap, format);
#ifdef HAVE_VASPRINTF
    vasprintf(&line, format, ap);
#else
    line = Malloc(LINE_MAX);
    vsnprintf(line, LINE_MAX, format, ap);
#endif
    va_end(ap);

    y = CALCHEIGHT();
    y += (title)? 1 : 0;
    tmp = strdup(line);
    strcpy(buf, line);

    while ((s = strsep(&tmp, "\n")) != NULL) {
	int n = strlen(s);

	y++;

	if (n > i) {
	    i = n;
	    strcpy(buf, s);
	}
    }

    tmp = line;

    if (prompt && strlen(prompt) > i)
	x = CALCWIDTH(prompt);
    else
	x = CALCWIDTH(buf);

    if (title && x < strlen(title))
	x = CALCWIDTH(title);

    if (!prompt)
	y--;

    if (!title)
	y--;

    cbreak();
    noecho();

    win = newwin(y, x, CALCPOSY(y), CALCPOSX(x));
    p = new_panel(win);

    wattron(win, MESSAGE_CP);

    for (i = 0; i < y; i++) {
	for (n = 0; n < x; n++)
	    mvwprintw(win, i, n, " ");
    }

    box(win, ACS_VLINE, ACS_HLINE);

    if (title) {
	draw_window_title(win, title, x);
    }

    i = (title) ? 2 : 1;

    while ((s = strsep(&tmp, "\n")) != NULL)
	mvwprintw(win, i++, CENTERX(x, s), "%s", s);

    free(tmp);
    free(line);

    if (prompt)
	mvwprintw(win, y - 2, CENTERX(x, prompt), "%s", prompt);

    wattroff(win, MESSAGE_CP);
    update_panels();
    doupdate();

    c = wgetch(win);
    del_panel(p);
    delwin(win);

    return c;
}
