/* $Id: message.c,v 1.7 2002-12-30 19:00:11 bjk Exp $ */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "colors.h"
#include "message.h"

int dump_message(const char *title, const char *prompt, int center,
	const char *extra_help, void(*custom_func)(void*), void *arg, int ckey,
	const char *format, ...)
{
    WINDOW *win;
    PANEL *panel;
    char *line, **lines = NULL;
    int width = 0, height;
    int i, n, pos;
    int total = 0;
    char buf[LINE_MAX], *p;
    char *tmp;
    va_list ap;

    va_start(ap, format);

#ifdef HAVE_VASPRINTF
    vasprintf(&line, format, ap);
#else
    line = Malloc(LINE_MAX);
    vsnprintf(line, LINE_MAX, format, ap);
#endif

    va_end(ap);

    /* Get the longest line to dynamically adjust the message box width. */
    for (i = n = pos = 0; line[i]; i++, n++) {
	if (line[i] == '\n') {
	    if (n > pos)
		pos = n;

	    n = 0;
	}
    }

    pos = (n > pos) ? n : pos;

    if (pos) {
	if (pos > MSG_WIDTH)
	    width = MSG_WIDTH;
	else
	    width = pos;
    }

    for (i = n = pos = 0; line[i]; i++, n++, pos++) {
	if (line[i] == '\t')
	    continue;

	if (line[i] == '\n')
	    pos = 0;

	if (pos > width) {
	    while (line[--i] != ' ')
		buf[--n] = ' ';

	    buf[n++] = '\n';
	    pos = 0;
	}

	buf[n] = line[i];
    }

    free(line);

    buf[n] = 0;
    p = buf;

    while ((tmp = strsep(&p, "\n")) != NULL) {
	tmp = trim(tmp);

	if (!*tmp)
	    continue;

	lines = Realloc(lines, (total + 2) * sizeof(char *));
	lines[total++] = strdup(tmp);
    }

    lines[total] = NULL;

    if (prompt && width < strlen(prompt))
	width = strlen(prompt);

    if (extra_help && width < strlen(extra_help))
	width = strlen(extra_help);

    if (title && width < strlen(title))
	width = strlen(title);

    width += 2;
    height = total;

    if (extra_help)
	height++;

    win = newwin((title) ? height + 5 : height + 4, width,
	    CALCPOSY(((title) ? height + 5 : height + 4)), CALCPOSX(width));
    panel = new_panel(win);
    wbkgd(win, CP_MESSAGE_WINDOW);
    draw_window_title(win, title, width, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);

    for (i = 0; lines[i]; i++)
	mvwprintw(win, (title) ? 2 + i: 1 + i, 
		(center) ? CENTERX(width, lines[i]) : 1, "%s", lines[i]);

    if (extra_help)
	draw_prompt(win, (title) ? height + 2 : height + 1, width, extra_help,
		CP_MESSAGE_PROMPT);

    draw_prompt(win, (title) ? height + 3 : height + 2, width, prompt,
	    CP_MESSAGE_PROMPT);

    while (1) {
	update_panels();
	doupdate();

	n = wgetch(win);

	if (!custom_func || n != ckey)
	    break;

	custom_func(arg);
    }

    del_panel(panel);
    delwin(win);

    for (i = 0; i < total; i++)
	free(lines[i]);

    free(lines);
    return n;
}
