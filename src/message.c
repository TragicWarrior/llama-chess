/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2013 Ben Kibbey <bjk@luxsci.net>

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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "common.h"
#include "conf.h"
#include "colors.h"
#include "misc.h"
#include "window.h"
#include "message.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

static void build_message_lines(const char *title, const char *prompt,
	int force_trim, const char *extra, int *h, int *w, char ***str,
	const char *fmt, va_list ap)
{
    int i, n, pos;
    char *line, **lines = NULL;
    int width = 0, height = 0;
    char buf[LINE_MAX];
    char *p;
    int total = 0;

#ifdef HAVE_VASPRINTF
    vasprintf(&line, fmt, ap);
#else
    line = Malloc(LINE_MAX);
    vsnprintf(line, LINE_MAX, fmt, ap);
#endif

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

    buf[n] = '\0';
    p = buf;
    lines = split_str(p, "\n", &total, &width, force_trim);

    if (prompt && width < strlen(prompt))
	width = strlen(prompt);

    if (extra && width < strlen(extra))
	width = strlen(extra);

    if (title && width < strlen(title))
	width = strlen(title);

    height = total;

    if (extra)
	height++;

    if (title)
	height++;

    height += 4; // 1 padding, 2 box, 1 prompt
    width += 4; // 2 padding, 2 box
    *h = height;
    *w = width;
    *str = lines;
}

static int display_message(WIN *win)
{
    struct message_s *m = win->data;
    int i;
    void *p = NULL;

    keypad(win->w, TRUE);
    window_draw_title(win->w, win->title, m->w, CP_MESSAGE_TITLE, 
	    CP_MESSAGE_BORDER);

    for (i = 0; m->lines[i]; i++)
	mvwprintw(win->w, (win->title) ? 2 + i: 1 + i, 
		(m->center || (!i && !m->lines[i+1])) ?
		CENTERX(m->w, m->lines[i]) : 1, "%s", m->lines[i]);

    if (m->extra)
	window_draw_prompt(win->w, (m->prompt) ? m->h - 3 : m->h - 2, m->w,
		m->extra, CP_MESSAGE_PROMPT);

    if (m->prompt)
	window_draw_prompt(win->w, m->h - 2, m->w, m->prompt, CP_MESSAGE_PROMPT);

    if (m->func && win->c == m->c) {
	(*m->func)(m->arg);
	return 1;
    }

    if (win->c != 0) {
	for (i = 0; m->lines[i]; i++)
	    free(m->lines[i]);

	free(m->lines);
	
	if (m->prompt)
	    free(m->prompt);

	if (m->extra)
	    free(m->extra);

	if (m->arg)
	    p = m->arg;

	free(m);
	win->data = p;
	return 0;
    }

    return 1;
}

/*
 * The force_trim parameter will trim whitespace reguardless if there is more
 * than one line or not (help text vs. tag viewing).
 */
WIN *construct_message(const char *title, const char *prompt, int center,
	int force_trim, const char *extra_help, message_func *func, void *arg,
	window_exit_func *efunc, int ckey, int freedata, const char *fmt, ...)
{
    char **lines = NULL;
    va_list ap;
    struct message_s *m = NULL;
    WIN *win = NULL;
    int h, w;
    
    va_start(ap, fmt);
    build_message_lines(title, prompt, force_trim, extra_help, &h, &w, &lines, fmt, ap);
    va_end(ap);

    m = Calloc(1, sizeof(struct message_s));
    m->lines = lines;
    m->w = w;
    m->h = h;
    m->center = center;
    m->c = ckey;
    m->func = func;
    m->arg = arg;

    if (prompt)
	m->prompt = strdup(prompt);

    if (extra_help)
	m->extra = strdup(extra_help);

    win = window_create(title, h, w, CALCPOSY(h), CALCPOSX(w), display_message, m, 
	    efunc);
    
    win->freedata = freedata;
    wbkgd(win->w, CP_MESSAGE_WINDOW);
    (*win->func)(win);
    return win;
}
