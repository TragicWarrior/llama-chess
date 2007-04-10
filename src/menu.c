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
#include <stdlib.h>
#include <unistd.h>
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

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "chess.h"
#include "conf.h"
#include "colors.h"
#include "strings.h"
#include "misc.h"
#include "window.h"
#include "menu.h"

static void set_menu_vars(int c, int rows, int items, int *item, int *top)
{
    int selected = *item;
    int toppos = *top;

    switch (c) {
	case KEY_HOME:
	    selected = toppos = 0;
	    break;
	case KEY_END:
	    selected = items;
	    toppos = items - rows + 1;
	    break;
	case KEY_UP:
	    if (selected - 1 < 0) {
		selected = items;

		toppos = selected - rows + 1;
	    }
	    else {
		selected--;

		if (toppos && selected <= toppos)
		    toppos = selected;
	    }
	    break;
	case KEY_DOWN:
	    if (selected + 1 > items )
		selected = toppos = 0;
	    else {
		selected++;

		if (selected - toppos >= rows)
		    toppos++;
	    }
	    break;
	case KEY_PPAGE:
	    selected -= rows;

	    if (selected < 0)
		selected = 0;

	    toppos = selected - rows + 1;

	    if (toppos < 0)
		toppos = 0;
	    break;
	case KEY_NPAGE:
	    selected += rows;

	    if (selected > items)
		selected = items;

	    toppos = selected - rows + 1;

	    if (toppos < 0)
		toppos = 0;
	    break;
	default:
	    if (selected == MAX_MENU_HEIGHT - 4)
		toppos = 1;
	    else if (selected <= rows)
		toppos = 0;
	    else {
		if (selected - toppos > rows)
		    toppos = selected - rows + 1;
	    }
	    break;
    }

    if (toppos < 0)
	toppos = 0;

    if (selected >= items) {
	selected = items;
	toppos = selected - rows + 1;
    }

    if (toppos < 0)
	toppos = 0;

    *item = selected;
    *top = toppos;
}

static void fix_menu_vals(WIN *win)
{
    struct menu_input_s *m = win->data;
    char buf[COLS - 4];
    int i = 0;
#ifdef HAVE_WRESIZE
    int n, nlen = 0, vlen = 0;
#endif

    for (i = 0; m->items[i]; i++);
    m->total = i;
    snprintf(buf, sizeof(buf), "Item %i %s %i  %s", m->selected + 1,
	    N_OF_N_STR, m->total, HELP_PROMPT);

#ifdef HAVE_WRESIZE
    if (!m->cstatic) {
	win->cols = 0;

	for (i = 0; m->items[i]; i++) {
	    n = strlen(m->items[i]->name);

	    if (nlen < n)
		nlen = n;

	    if (m->items[i]->value) {
	 	n = strlen(m->items[i]->value);

		if (vlen < n)
		    vlen = n;

		n = vlen + nlen;
	    }
	    else
		n = (!m->name_only) ? strlen(UNKNOWN) + nlen : nlen;

	    if (win->cols < n)
		win->cols = n;
	}
    }

    if (!m->rstatic)
	win->rows = i;

    if (!m->rstatic && win->title)
	win->rows++;

    if (!m->cstatic)
	win->cols += (!m->name_only) ? 5 : 2; // 2 box, 3 separator

    if (!m->rstatic)
	win->rows += 3; // 2 box, 1 prompt

    if (!m->rstatic && win->rows > MAX_MENU_HEIGHT)
	win->rows = MAX_MENU_HEIGHT;

    if (win->cols < strlen(buf))
	win->cols = strlen(buf) + 2; // 2 box

    if (win->cols > MAX_MENU_WIDTH)
	win->cols = MAX_MENU_WIDTH;

    wresize(win->w, win->rows, win->cols);
    replace_panel(win->p, win->w);
#endif
    move_panel(win->p, (m->ystatic == -1) ? CALCPOSY(win->rows) : m->ystatic, 
	    (m->xstatic == -1) ? CALCPOSX(win->cols) : m->xstatic);
    keypad(win->w, TRUE);
    wmove(win->w, 0, 0);
    wclrtobot(win->w);
    window_draw_title(win->w, win->title, win->cols, CP_INPUT_TITLE, 
	    CP_INPUT_BORDER);
    window_draw_prompt(win->w, win->rows - 2, win->cols, buf, CP_INPUT_PROMPT);
}

static void draw_menu(WIN *win)
{
    int i;
    int y = 0;
    struct menu_input_s *m = win->data;

    if (!m->items)
	return;

    for (i = m->top, y = 2; m->items[i] && y < win->rows - 2; i++, y++) {
	if (i == m->selected)
	    wattron(win->w, CP_MENU_SELECTED);
	else if (m->items[i]->selected)
	    wattron(win->w, CP_MENU_HIGHLIGHT);

	if (m->print_func) {
	    m->print_line = y;
	    m->item = m->items[i];
	    (*m->print_func)(win);
	}

	if (i == m->selected)
	    wattroff(win->w, CP_MENU_SELECTED);
	else if (m->items[i]->selected)
	    wattroff(win->w, CP_MENU_HIGHLIGHT);
    }
}

static int display_menu(WIN *win)
{
    struct menu_input_s *m = win->data;
    int i, n;
    int key = 0;
    char *p;

    cbreak();
    noecho();
    keypad(win->w, TRUE);
    nl();

    if (m->keys) {
	for (i = 0; m->keys[i]; i++) {
	    if (win->c == m->keys[i]->c) {
		(*m->keys[i]->func)(m);
		m->items = (*m->func)(win);
		key = 1;
		m->search[0] = 0;
		goto end;
	    }
	}
    }

    switch (win->c) {
	case REFRESH_MENU:
	    m->items = (*m->func)(win);
	    pushkey = 0;
	    break;
	case -1:
	    pushkey = 0;
	    goto done;
	case KEY_HOME:
	case KEY_END:
	case KEY_UP:
	case KEY_DOWN:
	case KEY_NPAGE:
	case KEY_PPAGE:
	    m->search[0] = 0;
	    break;
	default:
	    if (!win->c)
		break;

	    if (strlen(m->search) + 1 > sizeof(m->search) - 1)
		m->search[0] = 0;

	    p = m->search;

	    while (*p)
		p++;

	    *p++ = win->c;
	    *p = 0;
	    n = m->selected;

	    if (m->items) {
		for (i = 0; m->items[i]; i++) {
		    if (strncasecmp(m->search, m->items[i]->name, 
				strlen(m->search)) == 0) {
			m->selected = i;
			break;
		    }
		}
	    }

	    if (n == m->selected)
		m->search[0] = 0;
    }

end:
    set_menu_vars(win->c, win->rows - 4, m->total - 1, &m->selected, &m->top);
    fix_menu_vals(win);
    draw_menu(win);
    
    if (m->draw_exit_func)
	(*m->draw_exit_func)(m);

    return 1;

done:
    win->data = m->data;

    if (m->items) {
	for (i = 0; m->items[i]; i++) {
	    if (!m->nofree)
		free(m->items[i]->name);

	    if (!m->nofree && m->items[i]->value)
		free(m->items[i]->value);

	    free(m->items[i]);
	}

	free(m->items);
    }

    if (m->keys) {
	for (i = 0; m->keys[i]; i++)
	    free(m->keys[i]);

	free(m->keys);
    }

    free(m);
    return 0;
}

WIN *construct_menu(int rows, int cols, int y, int x, const char *title, 
	int name_only, menu_items *func, struct menu_key_s **keys, void *data, 
	menu_print_func *pfunc, window_exit_func *efunc)
{
    WIN *win;
    struct menu_input_s *m;

#ifndef HAVE_WRESIZE
    if (rows <= 0)
	rows = MAX_MENU_HEIGHT;

    if (cols <= 0)
	cols = MAX_MENU_WIDTH;
#endif

    m = Calloc(1, sizeof(struct menu_input_s));
    win = window_create(title, (rows <= 0) ? 1 : rows, (cols <= 0) ? 1 : cols, 
	    (y >= 0) ? y : 0, (x >= 0) ? x : 0, display_menu, m, efunc);
    m = win->data;
    m->ystatic = y;
    m->xstatic = x;

#ifndef HAVE_WRESIZE
    m->rstatic = 1;
    m->cstatic = 1;
#else
    if (rows > 0)
	m->rstatic = 1;

    if (cols > 0)
	m->cstatic = 1;
#endif

    m->print_func = pfunc;
    m->func = func;
    m->keys = keys;
    m->data = data;
    m->name_only = name_only;
    wbkgd(win->w, CP_MENU);
    cbreak();
    noecho();
    keypad(win->w, TRUE);
    nl();
    m->items = (*m->func)(win);
    fix_menu_vals(win);
    (*win->func)(win);
    return win;
}

void add_menu_key(struct menu_key_s ***dst, int c, menu_key func)
{
    int n = 0;
    struct menu_key_s **keys = *dst;

    if (keys)
	for (; keys[n]; n++);

    keys = Realloc(keys, (n + 2) * sizeof(struct menu_key_s *));
    keys[n] = Malloc(sizeof(struct menu_key_s));
    keys[n]->c = c;
    keys[n++]->func = func;
    keys[n] = NULL;
    *dst = keys;
}
