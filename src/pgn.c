/* $Id: pgn.c,v 1.8 2002-12-09 14:13:31 bjk Exp $ */
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

/* Get pgn_index number. */
static int get_index_number(WINDOW *win, int y, int x)
{
    int selected;
    char buf[4] = {0};

    mvwinnstr(win, y, x, buf, sizeof(buf) - 1);

    if(sscanf(buf, "%u", &selected) != 1)
	return -1;

    return selected;
}

/* FIXME segfault after 'q' (sometimes), scrolling */
void edit_pgn_data()
{
again:
    while (1) {
	WINDOW *win;
	PANEL *panel;
	int y = (pgn_index + 5 > LINES - 2) ? LINES - 2 : pgn_index + 5;
	int x = 0;
	int tlen = 0;
	int i;
	unsigned selected = 0;
	int cy = 2;
	char editprompt[76] = {0};
	char buf[4] = {0};
	char *tmp = NULL;

	for (i = 0; (i < pgn_index && i < LINES - 5); i++) {
	    int ttlen = strlen(pgn[i].token);
	    int vlen = strlen(pgn[i].value);
	    int llen = ttlen + vlen + 2;

	    if (tlen < ttlen)
		tlen = ttlen;

	    if (x < llen)
		x = llen;
	}

	x += 4;

	if (x < strlen(PGN_EDIT_PROMPT) + 4)
	    x = strlen(PGN_EDIT_PROMPT) + 4;

	win = newwin(y, x, LINES / 2 - y / 2, CALCPOSX(x));
	panel = new_panel(win);
	draw_window_title(win, PGN_EDIT_TITLE, x);
	curs_set(1);
	cbreak();
	noecho();
	nonl();
	keypad(win, TRUE);

	for (i = 0; i < pgn_index; i++)
	    mvwprintw(win, 2 + i, 1, "%u. %*s: %-*s", i + 1, tlen,
		    pgn[i].token, (x - tlen - (sizeof(buf) - 1 + 4)),
		    pgn[i].value);

	mvwprintw(win, y - 2, CENTERX(x, PGN_EDIT_PROMPT), "%s",
		PGN_EDIT_PROMPT);

	while (1) {
	    int c;
	    char *newtoken;
	    struct pgndata *tmppgn = NULL;
	    int tpgn_index = 0;

	    wmove(win, cy, 1);
	    update_panels();
	    doupdate();
	    c = wgetch(win);

	    switch (c) {
		case 'r':
		    if ((selected = get_index_number(win, cy, 1)) == -1) {
			message(ERROR, ANYKEY, PGN_BAD_INDEX);
			continue;
		    }

		    selected--;

		    if (selected <= 6) {
			message(NULL, ANYKEY, PGN_REMOVE_STR);
			continue;
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
		    del_panel(panel);
		    delwin(win);
		    goto again;
		    break;
		case 'a':
		    if ((newtoken = get_input(PGN_NEW_TAG, NULL)) == NULL)
			break;

		    if (add_pgn_data(&pgn, &pgn_index, newtoken, NULL)) {
			message(ERROR, ANYKEY, "%s \"%s\"", PGN_DUPLICATE,
				newtoken);
			    continue;
		    }

		    selected = pgn_index - 1;
		    goto gotkey;
		case 'j':
		case KEY_UP:
		    if (cy - 1 < 2)
			cy = y - 4;
		    else
			cy--;
		    break;
		case 'k':
		case KEY_DOWN:
		    if (cy + 1 > y - 4)
			cy = 2;
		    else
			cy++;
		    break;
		case KEY_RETURN:
		    if ((selected = get_index_number(win, cy, 1)) == -1) {
			message(ERROR, ANYKEY, PGN_BAD_INDEX);
			continue;
		    }

		    selected--;
		    goto gotkey;
		case 'q':
		    del_panel(panel);
		    delwin(win);
		    goto done;
		default:
		    beep();
		    break;
	    }
	}

gotkey:
	if (strcmp(pgn[selected].token, "Date") == 0) {
	    message(NULL, ANYKEY, "%s \"Date\"", PGN_EDIT_REFUSE);
	    continue;
	}

	snprintf(editprompt, sizeof(editprompt),
		"%s \"%s\"", PGN_EDIT_TAG, pgn[selected].token);

	tmp = get_input(editprompt, pgn[selected].value);

	if (tmp) {
	    if (strcmp(tmp, UNKNOWN) == 0)
		pgn[selected].value[0] = 0;
	}

	strncpy(pgn[selected].value, (tmp) ? tmp : "",
		sizeof(pgn[selected].value));

	del_panel(panel);
	delwin(win);
    }

done:
    curs_set(0);
    return;
}
