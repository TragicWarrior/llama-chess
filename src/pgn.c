/* $Id: pgn.c,v 1.9 2002-12-09 18:54:24 bjk Exp $ */
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

/* This tries to match the PGN specification as much as possible. The only
 * real exception are the 'Result' and 'Date' tags which are converted for
 * easier reading. It'll be converted back to the standard when saving to a
 * file.
 *
 * User defined tags are supported and can be displayed in-game (see help).
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <pwd.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MENU_H
#include <menu.h>
#endif

#include "common.h"
#include "pgn.h"

/* Returns 1 if a duplicate was found. 0 otherwise. The index argument is a
 * pointer to int, and incremented automatically.
 */
int add_pgn_data(struct pgndata **dst, int *n, char *token, char *value)
{
    int i, index = *n;
    struct pgndata *tdata = *dst;

    token = trim(token);
    value = trim(value);

    for (i = 0; i < index; i++) {
	if (strcasecmp(tdata[i].token, token) == 0)
	    return 1;
    }

    tdata = Realloc(tdata, (index + 2) * sizeof(struct pgndata));

    strncpy(tdata[index].token, token, sizeof(tdata[index].token));

    if (value)
	strncpy(tdata[index].value, value, sizeof(tdata[index].value));

    memset(&tdata[index + 1], 0, sizeof(struct pgndata));
    *n = ++index;
    *dst = tdata;
    return 0;
}

static char *remove_escapes(const char *str)
{
    int i, n;
    int len = strlen(str);
    static char buf[MAX_PGN_LINE_LEN] = {0};

    for (i = n = 0; i < len; i++, n++) {
	switch (str[i]) {
	    case '\\':
		i++;
	    default:
		break;
	}

	buf[n] = str[i];
    }

    buf[n] = 0;
    return buf;
}

static void init_data()
{
    time_t now;
    char tbuf[MAX_TIME_LEN + 1] = {0};
    struct passwd *pwd;
    struct tm *tp;

    if ((pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    pgn_index = 0;
    time(&now);
    tp = localtime(&now);
    strftime(tbuf, sizeof(tbuf), TIME_FORMAT, tp);

    /* The standard seven tag roster (in order of appearance). */
    add_pgn_data(&pgn, &pgn_index, "Event", UNKNOWN);
    add_pgn_data(&pgn, &pgn_index, "Site", UNKNOWN);
    add_pgn_data(&pgn, &pgn_index, "Date", tbuf);
    add_pgn_data(&pgn, &pgn_index, "Round", UNKNOWN);
    add_pgn_data(&pgn, &pgn_index, "White", pwd->pw_gecos);
    add_pgn_data(&pgn, &pgn_index, "Black", UNKNOWN);
    add_pgn_data(&pgn, &pgn_index, "Result", UNKNOWN);

    return;
}

/* We can count the number of games in a file, but only one can be loaded. The
 * last one, since games are more than likely appended to the file.
 */
int parse_pgn_file(const char *filename)
{
    FILE *fp;
    char buf[MAX_PGN_LINE_LEN], *tmp;
    int tag_section = 0;

    if (!filename[0]) {
	init_data();
	return 0;
    }

    if ((fp = fopen(filename, "r")) == NULL)
	return 1;

    pgn_index = status.rounds = 0;

    while ((tmp = fgets(buf, sizeof(buf), fp)) != NULL) {
	char *token, *value;
	int len = strlen(tmp);
	int i;
	char tbuf[MAX_TIME_LEN + 1] = {0};
	struct tm tp;

	/* Need more comment handling. */
	if (tmp[0] == '%')
	    continue;

	if (tag_section && tmp[0] == '\n') {
	    tag_section = 0;
	    status.rounds++;
	    continue;
	}

	if (tmp[len - 1] == '\n')
	    tmp[len-- - 1] = 0;

	/* Must be a roster tag... */
	if (tmp[0] == '[') {
	    if (!tag_section) {
		tag_section = 1;
		pgn_index = 0; /* Reset everytime a new tag section is
				  detected. */
	    }

	    tmp++;

	    if ((token = strsep(&tmp, " ")) != NULL) {
		tmp++; /* Skip the initial value quote. */
		tmp[strlen(tmp) - 2] = 0; /* Remove trailing '"]' from value. */
		value = tmp;

		if (strcmp(token, "Date") == 0) {
		    if (strptime(value, PGN_TIME_FORMAT, &tp) != NULL) {
			strftime(tbuf, sizeof(tbuf), TIME_FORMAT, &tp);
			value = (char *)tbuf;
		    }
		}
		else if (strcmp(token, "Result") == 0) {
		    for (i = 0; i < NARRAY(fancy_results); i++) {
			if (strcmp(value, fancy_results[i].pgn) == 0)
			    value = fancy_results[i].fancy;
		    }
		}

		if (!value[0])
		    value = UNKNOWN;

		add_pgn_data(&pgn, &pgn_index, token, remove_escapes(value));
	    }

	    continue;
	}
    
	/* Must be move text... */
    }

    fclose(fp);
    return 0;
}

static char *pgn_escapes(const char *str)
{
    int i, n;
    int len = strlen(str);
    static char buf[MAX_PGN_LINE_LEN] = {0};

    for (i = n = 0; i < len; i++, n++) {
	switch (str[i]) {
	    case '\\':
	    case '\"':
		buf[n++] = '\\';
		break;
	    default:
		break;
	}

	buf[n] = str[i];
    }

    buf[n] = 0;
    return buf;
}
	
int save_pgn(const char *filename)
{
    int i, n;
    int len = 0;
    FILE *fp;

    if ((fp = fopen(filename, "a")) == NULL)
	return 1;

    for (i = 0; i < pgn_index; i++) {
	struct tm tp;

	if (strcmp(pgn[i].token, "Date") == 0) {
	    if (strptime(pgn[i].value, TIME_FORMAT, &tp) != NULL)
		strftime(pgn[i].value, sizeof(pgn[i].value), PGN_TIME_FORMAT,
			&tp);
	}
	else if (strcmp(pgn[i].value, UNKNOWN) == 0)
	    pgn[i].value[0] = 0;

	fprintf(fp, "[%s \"%s\"]\n", pgn[i].token, 
		(pgn[i].value[0]) ? pgn_escapes(pgn[i].value) : "");
    }

    fprintf(fp, "\n");

    for (i = 0, n = 1; i < history_index; i += 2, n++) {
	int wlen = strlen(history[i].move);
	int blen = strlen(history[i + 1].move);

	if (wlen + blen + 6 + len > 80) {
	    fprintf(fp, "\n");
	    len = 0;
	}

	fprintf(fp, "%u. %s %s", n, history[i].move, history[i + 1].move);

	if (i + 2 < history_index)
	    fprintf(fp, " ");

	len += wlen + blen + 6;
    }

    fprintf(fp, "\n\n");
    fclose(fp);

    return 0;
}

static void cleanup(WINDOW *win, PANEL *panel, MENU *menu, ITEM **items)
{
    int i;

    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; items[i]; i++)
	free_item(items[i]);

    del_panel(panel);
    delwin(win);
}

void edit_pgn_data(int edit)
{
    while (1) {
	WINDOW *win;
	PANEL *panel;
	ITEM **mitems = NULL;
	MENU *menu;
	int i;
	char editprompt[76] = {0};
	char *tmp = NULL;
	int rows, cols;
	int selected = -1;

	if (!edit)
	    set_item_opts(NULL, ~O_SELECTABLE);

	for (i = 0; i < pgn_index; i++) {
	    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	    mitems[i] = new_item(pgn[i].token, pgn[i].value);
	}

	mitems[i] = NULL;
	menu = new_menu(mitems);
	scale_menu(menu, &rows, &cols);

	if (edit) {
	    if (cols < strlen(PGN_EDIT_TITLE))
		cols = strlen(PGN_EDIT_TITLE) + 2;
	}
	else {
	    if (cols < strlen(PGN_INFO_TITLE))
		cols = strlen(PGN_INFO_TITLE) + 2;
	}

	win = newwin(rows + 3, cols + 2, CALCPOSY(rows), CALCPOSX(cols));
	/* FIXME */
	set_menu_format(menu, 12, 0);
	set_menu_win(menu, win);
	set_menu_sub(menu, derwin(win, rows, cols, 2, 1));
	set_menu_fore(menu, A_REVERSE);
	set_menu_grey(menu, A_NORMAL);
	set_menu_mark(menu, NULL);
	set_menu_spacing(menu, 2, 0, 0);
	menu_opts_off(menu, O_NONCYCLIC);
	post_menu(menu);
	panel = new_panel(win);
	draw_window_title(win, (edit) ? PGN_EDIT_TITLE : PGN_INFO_TITLE, 
		cols + 2);
	cbreak();
	noecho();
	nonl();
	keypad(win, TRUE);

	while (1) {
	    int c;
	    struct pgndata *tmppgn;
	    char *newtag = NULL;
	    int tpgn_index = 0;

	    update_panels();
	    doupdate();

	    c = wgetch(win);

	    switch (c) {
		case 'r':
		    if (!edit)
			break;

		    selected = item_index(current_item(menu));

		    if (selected <= 6) {
			message(NULL, ANYKEY, PGN_REMOVE_STR);
			goto cleanup;
		    }

		    for (i = 0; i < pgn_index; i++) {
			if (i == selected)
			    continue;

			add_pgn_data(&tmppgn, &tpgn_index, pgn[i].token,
				pgn[i].value);
		    }

		    for (i = pgn_index = 0; i < tpgn_index; i++) {
			add_pgn_data(&pgn, &pgn_index, tmppgn[i].token,
				tmppgn[i].value);
		    }

		    free(tmppgn);
		    goto cleanup;
		    break;
		case 'a':
		    if (!edit)
			break;

		    if ((newtag = get_input_str(PGN_NEW_TAG, NULL)) == NULL)
			break;

		    if (add_pgn_data(&pgn, &pgn_index, newtag, NULL)) {
			message(ERROR, ANYKEY, "%s \"%s\"", PGN_DUPLICATE,
				newtag);
			goto cleanup;
		    }

		    selected = pgn_index - 1;
		    goto gotitem;
		    break;
		case 'j':
		case KEY_UP:
		    menu_driver(menu, REQ_UP_ITEM);
		    break;
		case 'k':
		case KEY_DOWN:
		    menu_driver(menu, REQ_DOWN_ITEM);
		    break;
		case KEY_RETURN:
		    if (!edit)
			break;

		    selected = item_index(current_item(menu));
		    goto gotitem;
		    break;
		case 'q':
		case KEY_ESCAPE:
		    cleanup(win, panel, menu, mitems);
		    goto done;
		    break;
		default:
		    break;
	    }
	}

gotitem:
	if (strcmp(pgn[selected].token, "Date") == 0) {
	    message(NULL, ANYKEY, "%s \"Date\"", PGN_EDIT_REFUSE);
	    goto cleanup;
	}

	snprintf(editprompt, sizeof(editprompt),
		"%s \"%s\"", PGN_EDIT_TAG, pgn[selected].token);

	tmp = get_input_str(editprompt, pgn[selected].value);

	if (tmp) {
	    if (strcmp(tmp, UNKNOWN) == 0)
		pgn[selected].value[0] = 0;
	}

	strncpy(pgn[selected].value, (tmp) ? tmp : "",
		sizeof(pgn[selected].value));

cleanup:
	cleanup(win, panel, menu, mitems);
    }

done:
    return;
}
