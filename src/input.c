/* $Id: input.c,v 1.5 2002-12-09 18:51:43 bjk Exp $ */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_FORM_H
#include <form.h>
#endif

#include "common.h"
#include "input.h"

static void cleanup(WINDOW *win, PANEL *panel, FORM *f, FIELD **fields)
{
    int i;

    unpost_form(f);
    free_form(f);

    for (i = 0; fields[i]; i++)
	free_field(fields[i]);

    del_panel(panel);
    delwin(win);

    return;
}

/* This function prompts for one line of input. The init argument is the
 * initial value. The type argument is the type of validation for the input.
 * Remaining arguments are values for the type argument. See field_type(3X).
 */
char *get_input(const char *prompt, const char *init, int type, ...)
{
    WINDOW *win;
    PANEL *panel;
    FIELD *fields[2];
    FORM *finput;
    int width, len;
    int y, x;
    static unsigned char dst[255];
    char *tmp;
    va_list ap;

    bzero(dst, sizeof(dst));

    len = strlen(prompt);
    width = (len + 4 > INPUT_WIDTH && len + 4 < COLS - 2) ?
	    len + 4 : INPUT_WIDTH;

    fields[0] = new_field(1, width - 4, 0, 0, 0, 0);

    va_start(ap, type);

    switch (type) {
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

    field_opts_off(fields[0], O_STATIC|O_WRAP|O_BLANK);
    fields[1] = NULL;
    finput = new_form(fields);

    scale_form(finput, &y, &x);

    win = newwin(y + 3, x + 4, CALCPOSY(y), CALCPOSX(x));
    set_form_win(finput, win);
    set_form_sub(finput, derwin(win, y, x, 2, 1));
    post_form(finput);
    nl();
    noecho();
    cbreak();
    keypad(win, TRUE);
    curs_set(1);
    panel = new_panel(win);
    draw_window_title(win, prompt, width);

    while (1) {
	int c;

	update_panels();
	doupdate();

	c = wgetch(win);
	c &= A_CHARTEXT;

	switch (c) {
	    case '':
		form_driver(finput, REQ_PREV_WORD);
		break;
	    case '':
		form_driver(finput, REQ_NEXT_WORD);
		break;
	    case '':
		form_driver(finput, REQ_BEG_LINE);
		break;
	    case '':
		form_driver(finput, REQ_END_LINE);
		break;
	    case '':
		form_driver(finput, REQ_CLR_EOL);
		break;
	    case '':
		form_driver(finput, REQ_CLR_FIELD);
		break;
	    case '':
		help(INPUT_HELP, inputhelp);
		break;
	    case KEY_LEFT:
		form_driver(finput, REQ_PREV_CHAR);
		break;
	    case KEY_RIGHT:
		form_driver(finput, REQ_NEXT_CHAR);
		break;
	    case '\010':
	    case KEY_BACKSPACE:
		form_driver(finput, REQ_DEL_PREV);
		break;
	    case '\n':
		goto done;
	    case KEY_ESCAPE:
		goto quit;
		/*
	    case ' ':
		form_driver(finput, REQ_INS_CHAR);
		break;
		*/
	    default:
		form_driver(finput, c);
		form_driver(finput, REQ_VALIDATION);
		break;
	}
    }

done:
    tmp = trim(field_buffer(fields[0], 0));
    strncpy(dst, (tmp) ? tmp : "", sizeof(dst));

quit:
    cleanup(win, panel, finput, fields);
    noecho();
    nonl();
    curs_set(0);
    return (dst[0]) ? dst : NULL;
}

char *get_input_str(const char *prompt, const char *init)
{
    return get_input(prompt, init, -1, 20);
}
