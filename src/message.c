/* $Id: message.c,v 1.2 2002-12-10 22:14:18 bjk Exp $ */
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
#include "message.h"

int message(const char *title, const char *prompt, const char *format, ...)
{
    WINDOW *win;
    PANEL *panel;
    FIELD *fields[2];
    FORM *form;
    va_list ap;
    char *line;
    int width, height;
    int c;

    va_start(ap, format);
#ifdef HAVE_VASPRINTF
    vasprintf(&line, format, ap);
#else
    line = Malloc(LINE_MAX);
    vsnprintf(line, LINE_MAX, format, ap);
#endif
    va_end(ap);

    width = (strlen(line) < MSG_WIDTH) ? strlen(line) : MSG_WIDTH;
    height = (width < MSG_WIDTH) ? 1 : strlen(line) / MSG_WIDTH + 1;

    fields[0] = new_field(height, width, 0, 0, 0, 0);
    set_field_buffer(fields[0], 0, line);
    set_field_just(fields[0], JUSTIFY_CENTER);
    field_opts_off(fields[0], O_EDIT);
    fields[1] = NULL;
    form = new_form(fields);
    scale_form(form, &height, &width);

    if (width < strlen(prompt))
	width = strlen(prompt);

    if (title && width < strlen(title))
	width = strlen(title);

    width += 2;

    win = newwin((title) ? height + 5 : height + 4, width + 2,
	    CALCPOSY(((title) ? height + 5 : height + 4)),
	    CALCPOSX(width));

    draw_window_title(win, title, width + 2);

    panel = new_panel(win);
    set_form_win(form, win);
    set_form_sub(form, derwin(win, height, width, (title) ? 2 : 1, 1));
    mvwprintw(win, (title) ? height + 3 : height + 2, 
	    CENTERX(width, prompt), "%s", prompt);
    post_form(form);

    update_panels();
    doupdate();

    c = wgetch(win);

    unpost_form(form);
    free_form(form);
    free_field(fields[0]);
    del_panel(panel);
    delwin(win);
    free(line);

    return c;
}
