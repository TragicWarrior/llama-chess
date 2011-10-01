/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2011 Ben Kibbey <bjk@luxsci.net>

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
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
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
#include "message.h"
#include "input.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

const char *inputhelp = {
    "UP/DOWN/LEFT/RIGHT - position cursor (multiline)\n" \
    "         UP/CTRL-P - previous input history\n" \
    "       DOWN/CTRL-N - next input history\n" \
    "       HOME/CTRL-A - move cursor to the beginning of line\n" \
    "        END/CTRL-E - move cursor to the end of line\n" \
    "          CTRL-B/W - move cursor to previous/next word\n" \
    "            CTRL-X - delete word under cursor\n" \
    "            CTRL-K - delete from cursor to end of line\n" \
    "            CTRL-U - clear entire input field\n" \
    "         BACKSPACE - delete previous character\n" \
    "            ESCAPE - quit without changes\n" \
    "             ENTER - quit with changes" 
};

static struct input_history_s {
    char *str;
    struct input_history_s *next;
    struct input_history_s *prev;
    struct input_history_s *head;
} *input_history[INPUT_HIST_MAX];

static void add_input_history(int which, const char *str)
{
    struct input_history_s *new;
    struct input_history_s *p = NULL;

    if (!str || !*str || which < 0)
	return;

    if (input_history[which])
	for (p = input_history[which]->head; p->next; p = p->next);

    new = calloc(1, sizeof(struct input_history_s));
    new->str = strdup(str);
    new->prev = p;

    if (p) {
	p->next = new;
	new->head = p->head;
    }
    else
	new->head = new;

    input_history[which] = p ? p->next : new;
}

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

static bool validate_pgn_result(int c, const void *arg)
{
    if (c != '1' && c != '0' && c != '2' && c != '*' && c != '/' && c != '-')
	return FALSE;

    return TRUE;
}

static int get_input(WIN *win)
{
    struct input_s *in = win->data;
    char *tmp;
    struct input_data_s *data = in->data;
    int i;

    curs_set(1);
    window_draw_title(win->w, win->title, in->w, CP_INPUT_TITLE,
	    CP_INPUT_BORDER);

    if (in->extra) {
	for (i = 0; in->extra[i]; i++)
	    window_draw_prompt(win->w, in->lines + 2 + i, in->w, in->extra[i], 
		    CP_INPUT_PROMPT);

	window_draw_prompt(win->w, in->lines + 2 + i, in->w, INPUT_HELP_PROMPT,
		CP_INPUT_PROMPT);
    }
    else
	window_draw_prompt(win->w, in->lines + 2, in->w, INPUT_HELP_PROMPT,
		CP_INPUT_PROMPT);

    if (in->func && in->c && win->c == in->c) {
	(*in->func)(in);
	return 1;
    }

    switch (win->c) {
	case CTRL('X'):
	    form_driver(in->f, REQ_DEL_WORD);
	    break;
	case CTRL('B'):
	    form_driver(in->f, REQ_PREV_WORD);
	    break;
	case CTRL('W'):
	    form_driver(in->f, REQ_NEXT_WORD);
	    break;
	case KEY_HOME:
	case CTRL('A'):
	    form_driver(in->f, REQ_BEG_LINE);
	    break;
	case KEY_END:
	case CTRL('E'):
	    form_driver(in->f, REQ_END_LINE);
	    break;
	case CTRL('K'):
	    form_driver(in->f, REQ_CLR_EOL);
	    break;
	case CTRL('U'):
	    form_driver(in->f, REQ_CLR_FIELD);
	    break;
	case KEY_F(1):
	    message(INPUT_HELP_TITLE, ANYKEY, "%s", inputhelp);
	    break;
	case KEY_LEFT:
	    form_driver(in->f, REQ_LEFT_CHAR);
	    break;
	case KEY_RIGHT:
	    form_driver(in->f, REQ_RIGHT_CHAR);
	    break;
	case CTRL('P'):
	case KEY_UP:
	    if (in->hist >= 0 && input_history[in->hist] && in->lines == 1) {
		set_field_buffer(in->fields[0], 0, input_history[in->hist]->str);
		form_driver(in->f, REQ_END_LINE);

		if (!input_history[in->hist]->prev)
		    input_history[in->hist] = input_history[in->hist]->head;
		else
		    input_history[in->hist] = input_history[in->hist]->prev;

		break;
	    }

	    form_driver(in->f, REQ_UP_CHAR);
	    break;
	case CTRL('N'):
	case KEY_DOWN:
	    if (in->hist >= 0 && input_history[in->hist] && in->lines == 1) {
		if (!input_history[in->hist]->next) {
		    set_field_buffer(in->fields[0], 0, NULL);
		    form_driver(in->f, REQ_CLR_FIELD);
		    break;
		}

		input_history[in->hist] = input_history[in->hist]->next;
		set_field_buffer(in->fields[0], 0, input_history[in->hist]->str);
		form_driver(in->f, REQ_END_LINE);
		break;
	    }

	    form_driver(in->f, REQ_DOWN_CHAR);
	    break;
	case '\010':
	case KEY_BACKSPACE:
	    form_driver(in->f, REQ_DEL_PREV);
	    break;
	case '\n':
	    tmp = field_buffer(in->fields[0], 0);
	    goto done;
	case KEY_ESCAPE:
	    if (in->reset)
		tmp = NULL;
	    else
		tmp = (in->buf[0]) ? in->buf : NULL;
	    goto done;
	default:
	    form_driver(in->f, (win->c & A_CHARTEXT));
	    break;
    }

    form_driver(in->f, REQ_VALIDATION);
    return 1;

done:
    if (tmp) {
	tmp = trim(tmp);

	if (tmp[0])
	    strncpy(in->buf, tmp, sizeof(in->buf));
	else
	    in->buf[0] = 0;
    }
    else
	in->buf[0] = 0;

    unpost_form(in->f);
    free_form(in->f);
    free_field(in->fields[0]);

    if (in->extra) {
	for (i = 0; in->extra[i]; i++)
	    free(in->extra[i]);

	free(in->extra);
    }

    data->str = (in->buf[0]) ? strdup(in->buf) : NULL;

    if (in->lines == 1)
	add_input_history(in->hist, in->buf);

    win->data = data;
    free_fieldtype(in->ft);
    free(in);
    curs_set(0);
    return 0;
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
 * The efunc parameter is the function that is ran when the window is done
 * getting input or NULL. This function is ran just before the window is
 * destroyed.
 *
 * The type argument is the type of validation for the input defined in
 * common.h. Remaining arguments are values for the type argument. See
 * field_type(3X) for validation types.
 *
 * FIXME form validation is buggy (integers).
 */
WIN *construct_input(const char *title, const char *init, int lines, int reset,
	const char *extra_help, input_func *func, void *arg, int key,
	struct input_data_s *id, int history, int type, ...)
{
    WIN *win;
    struct input_s *in;
    int l = (lines > 0) ? lines : 1;
    va_list ap;
    FIELDTYPE *ft = NULL;
    int eh = 0;

    l += 2;
    in = Calloc(1, sizeof(struct input_s));
    in->w = INPUT_WIDTH;

    if (extra_help) {
	char *tmp = strdup(extra_help);

	in->extra = split_str(tmp, "\n", &eh, &in->w, 0);
	l += eh;
	free(tmp);
    }

    if (title)
	l++;

    in->h = l + 1;
    in->func = func;
    in->arg = arg;
    in->c = key;
    in->lines = (lines) ? lines : 1;
    win = window_create(title, in->h, in->w, CALCPOSY(in->h), CALCPOSX(in->w), 
	    get_input, in, id->efunc);
    in = win->data;
    in->hist = history;
    in->data = id;
    in->reset = reset;
    in->fields[0] = new_field(in->lines, in->w - 2, 0, 0, 0, 0);
    va_start(ap, type);

    switch (type) {
	case FIELD_TYPE_PGN_ROUND:
	    ft = new_fieldtype(NULL, validate_pgn_round);
	    set_field_type(in->fields[0], ft);
	    break;
	case FIELD_TYPE_PGN_RESULT:
	    ft = new_fieldtype(NULL, validate_pgn_result);
	    set_field_type(in->fields[0], ft);
	    break;
	case FIELD_TYPE_PGN_DATE:
	    ft = new_fieldtype(NULL, validate_pgn_date);
	    set_field_type(in->fields[0], ft);
	    break;
	case FIELD_TYPE_PGN_TAG_NAME:
	    ft = new_fieldtype(NULL, validate_pgn_tag_name);
	    set_field_type(in->fields[0], ft);
	    break;
	case FIELD_TYPE_ALNUM:
	    set_field_type(in->fields[0], TYPE_ALNUM, va_arg(ap, int));
	    break;
	case FIELD_TYPE_ALPHA:
	    set_field_type(in->fields[0], TYPE_ALPHA, va_arg(ap, int));
	    break;
	case FIELD_TYPE_ENUM:
	    set_field_type(in->fields[0], TYPE_ENUM, va_arg(ap, char **),
		    va_arg(ap, int), va_arg(ap, int));
	    break;
	case FIELD_TYPE_INTEGER:
	    set_field_type(in->fields[0], TYPE_INTEGER, va_arg(ap, int),
		    va_arg(ap, long), va_arg(ap, long));
	    break;
	case FIELD_TYPE_NUMERIC:
	    set_field_type(in->fields[0], TYPE_NUMERIC, va_arg(ap, int),
		    va_arg(ap, double), va_arg(ap, double));
	    break;
	case FIELD_TYPE_REGEXP:
	    set_field_type(in->fields[0], TYPE_REGEXP, va_arg(ap, char *));
	    break;
/* Solaris 5.9 */
#ifdef HAVE_TYPE_IPV4
	case FIELD_TYPE_IPV4:
	    set_field_type(in->fields[0], TYPE_IPV4);
	    break;
#endif
	default:
	    set_field_type(in->fields[0], NULL);
	    break;
    }

    va_end(ap);

    if (init) {
	strncpy(in->buf, init, sizeof(in->buf));
	set_field_buffer(in->fields[0], 0, init);
    }

    in->ft = ft;

    if (in->lines == 1)
	field_opts_off(in->fields[0], O_STATIC);

    if (in->buf)
	set_field_buffer(in->fields[0], 0, in->buf);

    field_opts_off(in->fields[0], O_BLANK|O_AUTOSKIP);
    in->fields[1] = NULL;
    in->f = new_form(in->fields);
    form_opts_off(in->f, O_BS_OVERLOAD);
    set_form_win(in->f, win->w);
    in->sw = derwin(win->w, in->lines, in->w - 2, 2, 1);
    set_form_sub(in->f, in->sw);
    post_form(in->f);
    form_driver(in->f, REQ_END_FIELD);
    keypad(win->w, TRUE);
    curs_set(1);
    wbkgd(win->w, CP_INPUT_WINDOW);
    (*win->func)(win);
    return win;
}
