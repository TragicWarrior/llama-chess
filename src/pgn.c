/* $Id: pgn.c,v 1.31 2002-12-20 00:42:14 bjk Exp $ */
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
#include <sys/wait.h>
#include <sys/stat.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MENU_H
#include <menu.h>
#endif

#include "common.h"
#include "pgn.h"

int end_of_game(const char *str)
{
    int i;

    for (i = 0; i < NARRAY(fancy_results); i++) {
	if (strstr(str, fancy_results[i].pgn) != NULL) {
	    strncpy(game[gindex].pgn[PGN_RESULT].value, fancy_results[i].fancy,
		    sizeof(game[gindex].pgn[PGN_RESULT].value));
	    return 1;
	}
    }

    return 0;
}

char *parse_piece(char *str)
{
    int len, i;
    char *tmp, *tmp2;

    if ((tmp = strsep(&str, "\n")) == NULL)
	tmp = trim(str);

    len = strlen(tmp);

    if (tmp[len - 1] == '#') {
	status.notify = "Game Over!";

	if ((tmp2 = strsep(&str, " ")) != NULL) {
	    for (i = 0; i < NARRAY(fancy_results); i++) {
		if (strcmp(tmp2, fancy_results[i].pgn) == 0) {
		    strncpy(game[gindex].pgn[PGN_RESULT].value, 
			    fancy_results[i].fancy,
			    sizeof(game[gindex].pgn[PGN_RESULT].value));
		    break;
		}
	    }
	}
    }
    else if (tmp[len - 1] == '+')
	status.notify = "Check!";
    else if (tmp[len - 2] == '=')
	status.notify = "Promotion!";
    else
	status.notify = NULL;

    return tmp;
}

/* Returns 1 if a duplicate tag was found. 0 otherwise. The index argument is
 * a pointer to int, and incremented automatically.
 */
int add_pgn_data(struct pgndata **dst, int *n, char *token, char *value)
{
    int i, index = *n;
    struct pgndata *tdata = *dst;

    token = trim(token);
    value = trim(value);

    /* If a duplicate was found, update the existing one to the new value. */
    for (i = 0; i < index; i++) {
	if (strcasecmp(tdata[i].token, token) == 0) {
	    if (value)
		strncpy(tdata[i].value, value, sizeof(tdata[i].value));
	    else
		tdata[i].value[0] = 0;

	    *dst = tdata;
	    return 1;
	}
    }

    tdata = Realloc(tdata, (index + 2) * sizeof(struct pgndata));

    strncpy(tdata[index].token, token, sizeof(tdata[index].token));

    if (value)
	strncpy(tdata[index].value, value, sizeof(tdata[index].value));

    memset(&tdata[++index], 0, sizeof(struct pgndata));
    *n = index;
    *dst = tdata;
    return 0;
}

static char *remove_pgn_tag_escapes(const char *str)
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

void init_board()
{
    int row, col;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++) {
	    int c = '.';

	    switch (row) {
		case 0:
		case 7:
		    switch (col) {
			case 0:
			case 7:
			    c = 'r';
			    break;
			case 1:
			case 6:
			    c = 'n';
			    break;
			case 2:
			case 5:
			    c = 'b';
			    break;
			case 3:
			    c = 'q';
			    break;
			case 4:
			    c = 'k';
			    break;
		    }
		    break;
		case 1:
		case 6:
		    c = 'p';
		    break;
	    }

	    board[row][col].icon = (row < 2) ? c : toupper(c);
	}
    }

    return;
}

void set_pgn_defaults()
{
    time_t now;
    char tbuf[MAX_TIME_LEN + 1] = {0};
    struct passwd *pwd;
    struct tm *tp;
    int n;

    if ((pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    game = Calloc(1, sizeof(struct games));

    time(&now);
    tp = localtime(&now);
    strftime(tbuf, sizeof(tbuf), TIME_FORMAT, tp);

    /* The standard seven tag roster (in order of appearance). */
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Event",
	    UNKNOWN);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Site",
	    UNKNOWN);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Date", tbuf);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Round", 
	    UNKNOWN);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "White", 
	    pwd->pw_gecos);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Black",
	    UNKNOWN);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Result", 
	    UNKNOWN);

    /* Add custom tags from the configuration file. */
    for (n = 0; n < config.pindex; n++)
	add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, 
		config.pgn[n].token, config.pgn[n].value);

    return;
}

/* FIXME need a way to 'append' a game or round. */
static void init_data()
{
    set_pgn_defaults();
    gtotal = gindex + 1;
    init_board();
    return;
}

static void add_nag(FILE *fp, int index)
{
    int i, n = 0, count;
    int nag;
    char buf[2];

    while (fscanf(fp, " %1[$]%i %n", buf, &nag, &count) == 2) {
	/* Standard. */
	if (!nag || nag < 0 || nag > 255)
	    continue;

	for (i = 0; i < MAX_PGN_NAG; i++) {
	    if (game[gindex].history[index].nag[i])
		continue;

	    game[gindex].history[index].nag[i] = nag;
	    n++;
	    break;
	}

	if (n == MAX_PGN_NAG)
	    break;
    }

    return;
}

static void add_comment(FILE *fp, int index, int terminator)
{
    int i = 0;

    while (1) {
	int c;

	if ((c = fgetc(fp)) == terminator)
	    break;

	if (i + 1 > sizeof(game[gindex].history[index].comment) - 1)
	    continue;

	game[gindex].history[index].comment[i++] = c;
    }

    strncpy(game[gindex].history[index].comment,
	    trim(game[gindex].history[index].comment),
	    sizeof(game[gindex].history[index].comment));

    return;
}

static void add_move(FILE *fp, const char *move)
{
    int c;

    add_to_history(&game[gindex].history, &game[gindex].hindex,
	    &game[gindex].htotal, move);

    add_nag(fp, game[gindex].hindex - 1);

    c = fgetc(fp);

    while (isspace(c))
	c = fgetc(fp);

    if (c == '{' || c == ';') 
	add_comment(fp, game[gindex].hindex - 1, (c == '{') ? '}' : '\n');
    else
	ungetc(c, fp);

    return;
}

int parse_pgn_file(const char *filename)
{
    FILE *fp;
    char buf[LINE_MAX], *tmp;
    int tag_section = 0;

    if (gtotal)
	free_game_data();

    gtotal = gindex = 0;

    if (!*filename) {
	init_data();
	return 0;
    }

    game = Calloc(1, sizeof(struct games));

    if ((fp = fopen(filename, "r")) == NULL)
	return 1;

    while ((tmp = fgets(buf, sizeof(buf), fp)) != NULL) {
	char token[MAX_PGN_LINE_LEN + 1], value[MAX_PGN_LINE_LEN + 1];
	int i;
	char tbuf[MAX_TIME_LEN + 1] = {0};
	int offset;
	struct tm tp;

	/* Standard file comment. This has nothing to do with annotations. */
	if (*tmp == '%')
	    continue;

	if (tag_section && *tmp == '\n') {
	    tag_section = 0;
	    continue;
	}

	offset = strlen(tmp);
	tmp = trim(tmp);

	/* Must be a roster tag... */
	if (*tmp == '[') {
	    if (!tag_section) {
		tag_section = 1;
		game = Realloc(game, (gindex + 2) * sizeof(struct games));
		game[gindex + 1].pindex = game[gindex + 1].hindex = 0;
		memset(&game[gindex + 1].pgn, 0, sizeof(struct pgndata));
		memset(&game[gindex + 1].history, 0, sizeof(struct history));
	    }

	    tmp++;

	    if (sscanf(tmp, "%s %[^]]", token, value) == 2) {
		tmp = value;

		if (*tmp == '\"')
		    tmp++;

		if (tmp[strlen(tmp) - 1] == '\"')
		    tmp[strlen(tmp) - 1] = 0;

		strncpy(value, tmp, sizeof(value));

		if (strcasecmp(token, "Date") == 0) {
		    if (strptime(value, PGN_TIME_FORMAT, &tp) != NULL) {
			strftime(tbuf, sizeof(tbuf), TIME_FORMAT, &tp);
			strncpy(value, (char *)tbuf, sizeof(value));
		    }
		}
		else if (strcasecmp(token, "Result") == 0) {
		    for (i = 0; i < NARRAY(fancy_results); i++) {
			if (strcmp(value, fancy_results[i].pgn) == 0)
			    strncpy(value, fancy_results[i].fancy, 
				    sizeof(value));
		    }
		}
		else if (strcasecmp(token, "Round") == 0) {
		    if (*value == '-' || *value == '?')
			*value = 0;
		}

		if (!*value)
		    strncpy(value, UNKNOWN, sizeof(value));

		add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, 
			token, remove_pgn_tag_escapes(value));
	    }

	    continue;
	}
    
	if (!*tmp)
	    continue;

	/* Must be move text... */
	fseek(fp, -(offset), SEEK_CUR);
	game[gindex].hindex = game[gindex].htotal = 0;

	while (!feof(fp)) {
	    char move[MAX_PGN_MOVE_LEN + 1];
	    int moven, count;
	    int c = 0;

	    if (fscanf(fp, "%d. %7[a-zA-Z0-9+=#-] %n", &moven, move, &count)
		    == 2) {
		add_move(fp, move);

		/* Black. */
		if (fscanf(fp, " %7[a-zA-Z0-9+=#-] %n", move, &offset) == 1)
		    add_move(fp, move);

		continue;
	    }

	    /* No move text, NAG or comments. Must be a game terminating
	     * marker...
	     */
	    offset += count;
	    fseek(fp, -(offset), SEEK_CUR);
	    fread(buf, sizeof(char), offset, fp); 

	    while (!feof(fp)) {
		if ((c = fgetc(fp)) == '\n')
		    break;

		buf[offset++] = c;
	    }

	    ungetc(c, fp);
	    buf[offset] = 0;

	    /* Normally this would match the "Result" tag, but not always.
	     * So just leave it be (it's already defined from the tag parsing
	     * section above).
	     */
	    for (i = 0; i < NARRAY(fancy_results); i++) {
		if (strstr(buf, fancy_results[i].pgn) == 0)
		    goto done;
	    }

	    fseek(fp, offset, SEEK_CUR);
	    /*
	       printf("parse error?\n");
	       */
done:
	    break;
	}

	gindex++;
    }

    gtotal = gindex;

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
	
static void dump_comments_and_nag(FILE *fp, int index)
{
    int i;

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (game[gindex].history[index].nag[i])
	    fprintf(fp, "$%i ", game[gindex].history[index].nag[i]);
    }

    if (game[gindex].history[index].comment[0])
	fprintf(fp, "\n{%s}\n", game[gindex].history[index].comment);

    return;
}

int save_pgn(char *filename, struct pgndata *pgn, int isfifo)
{
    FILE *fp;
    int i, n;
    int len = 0;
    int data_index = 0;
    struct pgndata *data = NULL;

    /* This is a hack to resume an existing game when more than one game is
     * available. Also resuming a saved game and a game from history.
     */
    if (isfifo) {
	if ((fp = fopen(filename, "w")) == NULL)
	    return 1;
    }
    else
	if ((fp = fopen(filename, "a")) == NULL)
	    return 1;

    /* Modify a backup of the data so all the fancy tag names are kept while
     * PGN data is saved (history).
     */
    for (i = 0; pgn[i].token[0]; i++)
	add_pgn_data(&data, &data_index, pgn[i].token, pgn[i].value);

    for (i = 0; data[i].token[0]; i++) {
	struct tm tp;

	if (strcmp(data[i].token, "Date") == 0) {
	    if (strptime(data[i].value, TIME_FORMAT, &tp) != NULL)
		strftime(data[i].value, sizeof(data[i].value), PGN_TIME_FORMAT,
			&tp);
	}
	else if (strcmp(data[i].token, "Round") == 0) {
	    if (strcmp(data[i].value, UNKNOWN) == 0) {
		data[i].value[0] = '-';
		data[i].value[1] = 0;
	    }
	}
	else if (strcmp(data[i].token, "Result") == 0) {
	    if (strcmp(data[i].value, UNKNOWN) == 0) {
		data[i].value[0] = '*';
		data[i].value[1] = 0;
	    }
	    else {
		for (n = 0; n < NARRAY(fancy_results); n++) {
		    if (strcmp(data[i].value, fancy_results[n].fancy) == 0) {
			strncpy(data[i].value, fancy_results[n].pgn, 
				sizeof(data[i].value));
			break;
		    }
		}
	    }
	}
	else if (strcmp(data[i].value, UNKNOWN) == 0)
	    data[i].value[0] = 0;

	fprintf(fp, "[%s \"%s\"]\n", data[i].token, 
		(data[i].value[0]) ? pgn_escapes(data[i].value) : "");
    }

    fprintf(fp, "\n");

    /* Move text section. If it's dumping to the FIFO, dont dump comments and
     * NAG data.
     */
    for (i = 0, n = 1; i < game[gindex].hindex; i += 2, n++) {
	int wlen = strlen(game[gindex].history[i].move);
	int blen = strlen(game[gindex].history[i + 1].move);

	if (wlen + blen + 6 + len + 1 > 80) {
	    fprintf(fp, "\n");
	    len = 0;
	}

	fprintf(fp, "%u. %s ", n, game[gindex].history[i].move);

	if (!isfifo)
	    dump_comments_and_nag(fp, i);

	fprintf(fp, "%s ", game[gindex].history[i + 1].move);

	if (!isfifo)
	    dump_comments_and_nag(fp, i + 1);

	len += wlen + blen + 6;
    }

    if (strlen(data[PGN_RESULT].value) + len + 1 > 80)
	fprintf(fp, "\n");

    fprintf(fp, "%s\n\n", pgn_escapes(data[PGN_RESULT].value));

    fclose(fp);
    free(data);
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

struct pgndata *edit_pgn_data(int edit)
{
    struct pgndata *data = NULL;
    int data_index = 0;
    int i, lastindex = 0;

    /* Edit the backup copy, not the original in case the save fails. */
    for (i = 0; i < game[gindex].pindex; i++)
	add_pgn_data(&data, &data_index, game[gindex].pgn[i].token,
		game[gindex].pgn[i].value);

    while (1) {
	WINDOW *win;
	PANEL *panel;
	ITEM **mitems = NULL;
	MENU *menu;
	int i;
	char buf[76] = {0};
	char *tmp = NULL;
	int rows, cols;
	int selected = -1;
	char tmptime[MAX_TIME_LEN];
	struct tm tp;
	char *mbuf = NULL;

	for (i = 0; i < data_index; i++) {
	    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	    mitems[i] = new_item(data[i].token,
		    (strlen(data[i].value) > MAX_VALUE_WIDTH)
		     ? "Press ENTER..." : data[i].value);
	}

	mitems[i] = NULL;
	menu = new_menu(mitems);
	scale_menu(menu, &rows, &cols);

	if (cols < strlen(PGN_PROMPT))
	    cols = strlen(PGN_PROMPT);

	cols += 2;

	win = newwin(rows + 5, cols, CALCPOSY(rows + 5), CALCPOSX(cols));
	set_menu_format(menu, 12, 0);
	set_menu_win(menu, win);
	set_menu_sub(menu, derwin(win, rows, cols - 2, 2, 1));
	set_menu_fore(menu, A_REVERSE);
	set_menu_grey(menu, A_NORMAL);
	set_menu_mark(menu, NULL);
	set_menu_spacing(menu, 2, 0, 0);
	menu_opts_off(menu, O_NONCYCLIC);
	post_menu(menu);
	panel = new_panel(win);
	draw_window_title(win, (edit) ? PGN_EDIT_TITLE : PGN_INFO_TITLE, 
		cols);

	mvwprintw(win, rows + 3, CENTERX(cols, PGN_PROMPT), "%s", PGN_PROMPT);

	cbreak();
	noecho();
	keypad(win, TRUE);
	set_menu_pattern(menu, mbuf);

	while (1) {
	    int c;
	    struct pgndata *tmppgn = NULL;
	    char *newtag = NULL;
	    int tpgn_index = 0;
	    char *tmp;

	    if (set_current_item(menu, mitems[lastindex]) != E_OK) {
		lastindex = item_count(menu) - 1;
		continue;
	    }

	    /* This nl() statement needs to be here because NL is recognized
	     * for some reason after the first selection.
	     */
	    nl();
	    update_panels();
	    doupdate();

	    c = wgetch(win);

	    switch (c) {
		case CTRL('G'):
		    if (edit)
			help(PGN_EDIT_HELP, pgn_edit_help);
		    else
			help(PGN_INFO_HELP, pgn_info_help);
		    break;
		case CTRL('R'):
		    if (!edit)
			break;

		    selected = item_index(current_item(menu));

		    if (selected <= 6) {
			message(NULL, ANYKEY, PGN_REMOVE_STR);
			goto cleanup;
		    }

		    for (i = 0; i < data_index; i++) {
			if (i == selected)
			    continue;

			add_pgn_data(&tmppgn, &tpgn_index, data[i].token,
				data[i].value);
		    }

		    for (i = data_index = 0; i < tpgn_index; i++) {
			add_pgn_data(&data, &data_index, tmppgn[i].token,
				tmppgn[i].value);
		    }

		    free(tmppgn);
		    goto cleanup;
		    break;
		case CTRL('A'):
		    if (!edit)
			break;

		    if ((newtag = get_input(PGN_NEW_TAG, NULL, 1, 0, NULL, NULL,
				    NULL, FIELD_TYPE_PGN_TAG_NAME)) == NULL)
			break;

		    newtag[0] = toupper(newtag[0]);

		    for (i = 0; i < data_index; i++) {
			if (strcasecmp(data[i].token, newtag) == 0) {
			    selected = i;
			    goto gotitem;
			}
		    }

		    add_pgn_data(&data, &data_index, newtag, NULL);

		    selected = data_index - 1;
		    goto gotitem;
		    break;
		case KEY_UP:
		    menu_driver(menu, REQ_UP_ITEM);
		    break;
		case KEY_DOWN:
		    menu_driver(menu, REQ_DOWN_ITEM);
		    break;
		case '\n':
		    selected = item_index(current_item(menu));
		    goto gotitem;
		    break;
		case KEY_ESCAPE:
		    cleanup(win, panel, menu, mitems);
		    goto done;
		    break;
		default:
		    tmp = menu_pattern(menu);

		    if (tmp && tmp[strlen(tmp) - 1] != c) {
			menu_driver(menu, REQ_CLEAR_PATTERN);
			menu_driver(menu, c);
		    }
		    else {
			if (menu_driver(menu, REQ_NEXT_MATCH) == E_NO_MATCH)
			    menu_driver(menu, c);
		    }

		    break;
	    }

	    lastindex = item_index(current_item(menu));
	}

gotitem:
	lastindex = selected;

	if (!edit) {
	    snprintf(buf, sizeof(buf), "Tag Information for \"%s\"", 
		    data[selected].token);
	    message(buf, ANYKEY, "%s", data[selected].value);
	    goto cleanup;
	}

	snprintf(buf, sizeof(buf), "%s \"%s\"", PGN_EDIT_TAG,
		data[selected].token);

	if (strcmp(data[selected].token, "Date") == 0) {
	    tmp = strptime(data[selected].value, TIME_FORMAT, &tp);
	    strftime(tmptime, MAX_TIME_LEN, PGN_TIME_FORMAT, &tp);

	    tmp = get_input(buf, tmptime, 0, 0, 0, NULL, NULL, NULL,
		    FIELD_TYPE_PGN_DATE);

	    if (tmp) {
		if (strptime(tmp, PGN_TIME_FORMAT, &tp) == NULL) {
		    message(ERROR, ANYKEY, "The \"Date\" tag must be in "
			    "YYYY.MM.DD format");
		    goto cleanup;
		}
	    }
	    else
		goto cleanup;
	}
	else if (strcmp(data[selected].token, "Round") == 0)
	    tmp = get_input(buf, NULL, 1, 1, NULL, NULL, NULL,
		    FIELD_TYPE_PGN_ROUND);
	else
	    tmp = get_input(buf, data[selected].value, 0, 0, NULL, NULL, NULL,
		    -1);

	if (tmp) {
	    if (strcmp(tmp, UNKNOWN) == 0)
		data[selected].value[0] = 0;
	}

	strncpy(data[selected].value, (tmp) ? tmp : "",
		sizeof(data[selected].value));

cleanup:
	cleanup(win, panel, menu, mitems);
    }

done:
    if (!edit) {
	free(data);
	return NULL;
    }

    return data;
}
