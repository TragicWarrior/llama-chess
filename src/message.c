/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2015 Ben Kibbey <bjk@luxsci.net>

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
#include <wctype.h>

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

static void wordwrap_lines (wchar_t ***olines, int *nlines, int *width)
{
    int i;
    wchar_t *buf = NULL, **lines = *olines;
    int total = *nlines, w = *width;

    for (i = 0; i < total; i++) {
	size_t len = wcslen (lines[i]);

	if (buf) {
	    size_t blen = wcslen (buf);

	    lines[i] = Realloc (lines[i], len+blen+1*sizeof(wchar_t *));
	    wmemmove (&lines[i][blen], lines[i], len);
	    wmemcpy (lines[i], buf, blen);
	    lines[i][blen+len] = 0;
	    free (buf);
	    buf = NULL;
	    len = wcslen (lines[i]);
	}

	if (len > w)
	  w = len;

	if (len-- > MSG_WIDTH) {
	    wchar_t *p;

	    for (p = lines[i]+len; *p && len > 0; p--, len--) {
		if (iswspace (*p) && len <= MSG_WIDTH) {
		    *p++ = 0;
		    buf = wcsdup (p);
		    break;
		}
	    }

	    /* Its a very long line without any unicode space. Create a
	     * new message line. */
	    if (!buf) {
	        wchar_t *p, c, *bp;
		size_t len;

		p = lines[i]+MSG_WIDTH;
		c = *p;
		*p++ = 0;
		len = wcslen (p)+2;
		buf = Malloc (len*sizeof(wchar_t));
		bp = buf;
		*bp++ = c;
		wmemcpy (bp, p, len-1);
	    }
	}
    }

    *width = w;

    if (buf) {
        lines = Realloc (lines, (total+2)*sizeof(wchar_t *));
	lines[total++] = buf;
	lines[total] = NULL;
	*nlines = total;
	*olines = lines;
	wordwrap_lines (olines, nlines, width);
    }
}

static void build_message_lines(const char *title, const char *prompt,
				int force_trim, const char *extra, int *h,
				int *w, wchar_t ***str, const char *fmt,
				va_list ap)
{
    int n;
    char *line;
    wchar_t **lines = NULL;
    int width = 0, height = 0, len;
    int total = 0;
    wchar_t *wc, *wc_tmp, *wc_line, wc_delim[] = { '\n', 0 };

#ifdef HAVE_VASPRINTF
    vasprintf(&line, fmt, ap);
#else
    line = Malloc(LINE_MAX);
    vsnprintf(line, LINE_MAX, fmt, ap);
#endif

    wc = str_to_wchar (line);
    free (line);
    total = n = 0;
    for (wc_line = wcstok (wc, wc_delim, &wc_tmp); wc_line; wc_line = wcstok (NULL, wc_delim, &wc_tmp)) {
	lines = Realloc (lines, (total+2)*sizeof(wchar_t *));
	lines[total++] = wcsdup (wc_line);
	lines[total] = NULL;
    }

    free (wc);
    wordwrap_lines (&lines, &total, &width);

    if (width > MSG_WIDTH)
        width = MSG_WIDTH;

    if (prompt) {
        wc = str_to_wchar (prompt);
	len = wcslen (wc);
	width = len > width ? len : width;
	free (wc);
    }

    if (extra) {
        wc = str_to_wchar (extra);
	len = wcslen (wc);
	width = len > width ? len : width;
	free (wc);
    }

    if (title) {
        wc = str_to_wchar (title);
	len = wcslen (wc);
	width = len > width ? len : width;
	free (wc);
    }

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
    int n_lines = 0;
    int r = 0;

    keypad(win->w, TRUE);
    window_draw_title(win->w, win->title, m->w, CP_MESSAGE_TITLE,
	    CP_MESSAGE_BORDER);

    for (i = 0; m->lines[i]; i++) {
	n_lines++;

	if (m->offset && i < m->offset)
	    continue;

	mvwprintw(win->w, (win->title) ? 2 + r: 1 + r,
		  (m->center || (!i && !m->lines[i+1])) ?
		  CENTERX(m->w, m->lines[i]) : 1, "%ls", m->lines[i]);
	r++;
	if (r >= LINES-5)
	    break;
    }

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
	if (win->c == KEY_DOWN || win->c == 'k' || win->c == KEY_UP
	    || win->c == 'j') {
	    int n = 3;

	    n += m->extra ? 1 : 0;

	    if ((n_lines + n) - m->offset >= LINES-2)
		m->offset = win->c == KEY_DOWN || win->c == 'k'
		    ? m->offset+1 : m->offset-1;
	    else if (win->c == KEY_UP || win->c == 'j')
		m->offset--;

	    if (m->offset < 0)
		m->offset = 0;

	    werase (win->w);
	    win->c = 0;
	    return display_message (win);
	}

	for (i = 0; m->lines[i]; i++)
	    free(m->lines[i]);

	free(m->lines);
	free(m->prompt);
	free(m->extra);
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
		       int force_trim, const char *extra_help,
		       message_func *func, void *arg, window_exit_func *efunc,
		       wint_t ckey, int freedata, const char *fmt, ...)
{
    wchar_t **lines = NULL;
    va_list ap;
    struct message_s *m = NULL;
    WIN *win = NULL;
    int h, w;

    va_start(ap, fmt);
    build_message_lines(title, prompt, force_trim, extra_help, &h, &w, &lines,
			fmt, ap);
    va_end(ap);

    m = Calloc(1, sizeof(struct message_s));
    m->lines = lines;
    m->w = w;
    m->h = h > LINES-2 ? LINES-2 : h;
    m->center = center;
    m->c = ckey;
    m->func = func;
    m->arg = arg;

    if (prompt)
	m->prompt = strdup(prompt);

    if (extra_help)
	m->extra = strdup(extra_help);

    win = window_create(title, m->h, m->w, CALCPOSY(m->h), CALCPOSX(m->w),
			display_message, m, efunc);

    win->freedata = freedata;
    wbkgd(win->w, CP_MESSAGE_WINDOW);
    (*win->func)(win);
    return win;
}
