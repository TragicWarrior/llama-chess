/* $Id: input.c,v 1.11 2002-12-12 19:16:51 bjk Exp $ */
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
#include <stdarg.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_FORM_H
#include <form.h>
#endif

#include "common.h"
#include "input.h"

static bool validate_pgn_tag_name(int c, const void *arg)
{
    if (!isalpha(c) && !isdigit(c) && c != '_')
	return FALSE;

    return TRUE;
}

static bool validate_pgn_date(int c, const void *arg)
{
    if (!isdigit(c) && c != '.' && c != '?')
	return FALSE;

    return TRUE;
}

static bool validate_pgn_round(int c, const void *arg)
{
    if (!isdigit(c) && c != '.' && c != '-' && c != '?')
	return FALSE;

    return TRUE;
}

/* This function titles for one line of input. The init argument is the
 * initial value. The lines argument is how many lines the field is. If zero,
 * then it is dynamically determined based on the init argument. The clear
 * argument is whether pressing ESC restores the init argument, if any, or
 * returns NULL. The type argument is the type of validation for the input
 * defined in common.h. Remaining arguments are values for the type argument.
 * See field_type(3X) for validation types.
 *
 * For some reason TYPE_ALNUM and TYPE_ALPHA don't like spaces. In this case
 * just use -1 as the type with no arguments.
 */
char *get_input(const char *title, const char *init, int lines, int clear,
	int type, ...)
{
    WINDOW *win;
    PANEL *panel;
    FIELD *fields[2];
    FORM *form;
    int width, len;
    int y, x;
    int i, n;
    static unsigned char dst[MAX_PGN_LINE_LEN];
    char *tmp;
    va_list ap;
    FIELDTYPE *TYPE_PGN_TAG_NAME;
    FIELDTYPE *TYPE_PGN_DATE;
    FIELDTYPE *TYPE_PGN_ROUND;
    int quit = 0;

    bzero(dst, sizeof(dst));

    len = strlen(title);
    width = (len + 4 > INPUT_WIDTH && len + 4 < COLS - 2) ?
	    len + 4 : INPUT_WIDTH;

    fields[0] = new_field((lines) ? lines : sizeof(dst) / width + 1, 
	    width - 2, 0, 0, 0, 0);

    TYPE_PGN_TAG_NAME = new_fieldtype(NULL, validate_pgn_tag_name);
    TYPE_PGN_DATE = new_fieldtype(NULL, validate_pgn_date);
    TYPE_PGN_ROUND = new_fieldtype(NULL, validate_pgn_round);

    va_start(ap, type);

    switch (type) {
	case FIELD_TYPE_PGN_ROUND:
	    set_field_type(fields[0], TYPE_PGN_ROUND);
	    break;
	case FIELD_TYPE_PGN_DATE:
	    set_field_type(fields[0], TYPE_PGN_DATE);
	    break;
	case FIELD_TYPE_PGN_TAG_NAME:
	    set_field_type(fields[0], TYPE_PGN_TAG_NAME);
	    break;
	case FIELD_TYPE_ALNUM:
	    set_field_type(fields[0], TYPE_ALNUM, va_arg(ap, int));
	    break;
	case FIELD_TYPE_ALPHA:
	    set_field_type(fields[0], TYPE_ALPHA, va_arg(ap, int));
	    break;
	case FIELD_TYPE_ENUM:
	    set_field_type(fields[0], TYPE_ENUM, va_arg(ap, char **),
		    va_arg(ap, int), va_arg(ap, int));
	    break;
	case FIELD_TYPE_INTEGER:
	    set_field_type(fields[0], TYPE_INTEGER, va_arg(ap, int),
		    va_arg(ap, long), va_arg(ap, long));
	    break;
	case FIELD_TYPE_NUMERIC:
	    set_field_type(fields[0], TYPE_NUMERIC, va_arg(ap, int),
		    va_arg(ap, double), va_arg(ap, double));
	    break;
	case FIELD_TYPE_REGEXP:
	    set_field_type(fields[0], TYPE_REGEXP, va_arg(ap, char *));
	    break;
	case FIELD_TYPE_IPV4:
	    set_field_type(fields[0], TYPE_IPV4);
	    break;
	default:
	    break;
    }

    va_end(ap);

    if (init)
	set_field_buffer(fields[0], 0, init);

    field_opts_off(fields[0], O_BLANK|O_AUTOSKIP);
    fields[1] = NULL;
    form = new_form(fields);
    form_opts_off(form, O_BS_OVERLOAD);

    scale_form(form, &y, &x);

    win = newwin(y + 5, x + 2, CALCPOSY(y - 5), CALCPOSX(x));
    set_form_win(form, win);
    set_form_sub(form, derwin(win, y, x, 2, 1));
    post_form(form);
    nl();
    noecho();
    cbreak();
    keypad(win, TRUE);
    curs_set(1);
    panel = new_panel(win);
    draw_window_title(win, title, width);
    mvwprintw(win, y + 3, CENTERX(x, INPUT_HELP_PROMPT), "%s", 
	    INPUT_HELP_PROMPT);
    form_driver(form, REQ_END_FIELD);

    while (1) {
	int c;

	update_panels();
	doupdate();

	c = wgetch(win);

	switch (c) {
	    case CTRL('X'):
		form_driver(form, REQ_DEL_WORD);
		break;
	    case CTRL('B'):
		form_driver(form, REQ_PREV_WORD);
		break;
	    case CTRL('W'):
		form_driver(form, REQ_NEXT_WORD);
		break;
	    case CTRL('A'):
		form_driver(form, REQ_BEG_LINE);
		break;
	    case CTRL('E'):
		form_driver(form, REQ_END_LINE);
		break;
	    case CTRL('K'):
		form_driver(form, REQ_CLR_EOL);
		break;
	    case CTRL('U'):
		form_driver(form, REQ_CLR_FIELD);
		break;
	    case CTRL('G'):
		help(INPUT_HELP, inputhelp);
		break;
	    case KEY_LEFT:
		form_driver(form, REQ_LEFT_CHAR);
		break;
	    case KEY_RIGHT:
		form_driver(form, REQ_RIGHT_CHAR);
		break;
	    case KEY_UP:
		form_driver(form, REQ_UP_CHAR);
		break;
	    case KEY_DOWN:
		form_driver(form, REQ_DOWN_CHAR);
		break;
	    case '\010':
	    case KEY_BACKSPACE:
		form_driver(form, REQ_DEL_PREV);
		break;
	    case '\n':
		goto done;
		break;
	    case KEY_ESCAPE:
		quit = 1;
		goto done;
		break;
	    default:
		form_driver(form, (c & A_CHARTEXT));
		form_driver(form, REQ_VALIDATION);
		break;
	}
    }

done:
    if (quit) {
	if (clear) {
	    dst[0] = 0;
	    goto cleanup;
	}
	else
	    set_field_buffer(fields[0], 0, init);
    }

    form_driver(form, REQ_VALIDATION);
    tmp = trim(field_buffer(fields[0], 0));

    /* Remove multiple whitespace. Happens on a multiline field. */
    if (tmp) {
	for (i = 0, n = 0; i < strlen(tmp); i++) {
	    if (isspace(tmp[i]) && isspace(tmp[i + 1]))
		continue;

	    dst[n++] = tmp[i];
	}

	dst[n] = 0;
    }
    else
	dst[0] = 0;

    dst[sizeof(dst) - 1] = 0;

cleanup:
    unpost_form(form);
    free_form(form);

    for (i = 0; fields[i]; i++)
	free_field(fields[i]);

    del_panel(panel);
    delwin(win);
    free_fieldtype(TYPE_PGN_TAG_NAME);
    free_fieldtype(TYPE_PGN_DATE);
    free_fieldtype(TYPE_PGN_ROUND);
    noecho();
    nonl();
    curs_set(0);
    return (dst[0]) ? dst : NULL;
}

char *get_input_str(const char *title, const char *init)
{
    return get_input(title, init, 1, 0, -1, 20);
}

char *get_input_str_clear(const char *title, const char *init)
{
    return get_input(title, init, 1, 1, -1, 20);
}
