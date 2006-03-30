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
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_FORM_H
#include <form.h>
#endif

#ifdef HAVE_PANEL_H
#include <panel.h>
#endif

#include "chess.h"
#include "conf.h"
#include "colors.h"
#include "window.h"
#include "strings.h"
#include "misc.h"
#include "input.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

const char *inputhelp[] = {
    "UP/DOWN/LEFT/RIGHT - position cursor",
    "            CTRL-A - move cursor to the beginning of line",
    "            CTRL-E - move cursor to the end of line",
    "            CTRL-B - move cursor to previous word",
    "            CTRL-W - move cursor to next word",
    "            CTRL-X - delete word under cursor",
    "            CTRL-K - delete from cursor to end of line",
    "            CTRL-U - clear entire input field",
    "         BACKSPACE - delete previous character",
    "            ESCAPE - quit without changes",
    "             ENTER - quit with changes",
    NULL
};

static bool validate_pgn_tag_name(int c, const void *arg)
{
    if (!isalnum(c) && c != '_')
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

/*
 * This function prompts for input. The init argument is the initial value.
 * The lines argument is how many lines the field is. If zero, then it is
 * dynamically determined based on the init argument or INPUT_WIDTH if init is
 * NULL.
 *
 * The reset argument is whether pressing ESC returns the initial value or 
 * NULL. 
 *
 * The extra_help argument is an extra line of help prompt normally used with 
 * the custom_func argument. The custom_func argument is a pointer to a 
 * function of type void which takes one pointer-to-void argument. This
 * function is called when the ckey argument is pressed.
 *
 * The type argument is the type of validation for the input defined in
 * common.h. Remaining arguments are values for the type argument. See
 * field_type(3X) for validation types.
 *
 * FIXME form validation is buggy (integers).
 */
char *get_input(const char *title, const char *init, int lines, int reset,
	const char *extra_help, char *(*custom_func)(void *), void *arg, 
	chtype ckey, int type, ...)
{
    WINDOW *win, *swin;
    PANEL *panel;
    FIELD *fields[2];
    FORM *form;
    int width, len;
    int y, x;
    int i, n;
    static char dst[MAX_PGN_LINE_LEN];
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

    if (lines == 1)
	field_opts_off(fields[0], O_STATIC);

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

    win = newwin((extra_help) ? y + 5 : y + 4, x + 2, 
	    CALCPOSY(((extra_help) ? y + 5 : y + 4)), CALCPOSX(x));
    set_form_win(form, win);
    swin = derwin(win, y, x, 2, 1);
    set_form_sub(form, swin);
    post_form(form);
    nl();
    noecho();
    cbreak();
    keypad(win, TRUE);
    curs_set(1);
    panel = new_panel(win);
    wbkgd(win, CP_INPUT_WINDOW);
    draw_window_title(win, title, width, CP_INPUT_TITLE, CP_INPUT_BORDER);

    if (extra_help)
	draw_prompt(win, y + 2, width, extra_help, CP_INPUT_PROMPT);

    draw_prompt(win, (extra_help) ? y + 3 : y + 2, width, INPUT_HELP_PROMPT,
	    CP_INPUT_PROMPT);

    form_driver(form, REQ_END_FIELD);

    while (1) {
	chtype c;

	update_panels();
	doupdate();

	c = wgetch(win);

	if (c == ckey && custom_func) {
	    form_driver(form, REQ_VALIDATION);

	    if ((tmp = custom_func(arg)) != NULL) {
		set_field_buffer(fields[0], 0, tmp);
		form_driver(form, REQ_END_LINE);
	    }

	    continue;
	}

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
	    case KEY_F(1):
		help(INPUT_HELP_TITLE, ANYKEY, inputhelp);
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
	if (reset) {
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
    delwin(swin);
    delwin(win);
    free_fieldtype(TYPE_PGN_TAG_NAME);
    free_fieldtype(TYPE_PGN_DATE);
    free_fieldtype(TYPE_PGN_ROUND);
    noecho();
    nonl();
    curs_set(0);
    tmp = dst;
    return (dst[0]) ? dst : NULL;
}
