/* $Id: history.c,v 1.34 2003-01-24 20:27:50 bjk Exp $ */
/*
    Copyright (C) 2002-2003 Ben Kibbey <bjk@arbornet.org>

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
#include <limits.h>
#include <string.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MENU_H
#include <menu.h>
#endif

#include "common.h"
#include "colors.h"
#include "history.h"

void free_historydata(struct history *h, int total)
{
    int i;

    if (total) {
	for (i = 0; i < total; i++) {
	    if (h[i].comment)
		free(h[i].comment);
	}
    }

    return;
}

static int init_nag()
{
    FILE *fp;
    char line[LINE_MAX];
    int i = 0;

    if ((fp = fopen(config.nagfile, "r")) == NULL) {
	message(ERROR, ANYKEY, "Could not open NAG file.");
	return 1;
    }

    while (!feof(fp)) {
	if (fscanf(fp, " %[^\n] ", line) == 1) {
	    nag = Realloc(nag, (i + 2) * sizeof(struct nags));
	    strncpy(nag[i].line, line, sizeof(nag[i].line));
	    i++;
	}
    }

    memset(&nag[i], 0, sizeof(struct nags));
    return 0;
}

static void view_nag(void *arg)
{
    int index = (int)arg;
    char buf[80];
    char line[LINE_MAX] = {0};
    int i = 0;

    snprintf(buf, sizeof(buf), "Viewing NAG for \"%s\"", 
	    game[gindex].history[index].move);

    if (!nag) {
	if (init_nag())
	    return;
    }

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (!game[gindex].history[index].nag[i])
	    break;

	strncat(line, nag[game[gindex].history[index].nag[i] - 1].line,
		sizeof(line));
	strncat(line, "\n", sizeof(line));
    }

    line[strlen(line) - 1] = 0;
    message_uncentered(buf, ANYKEY, "%s", line);
    return;
}

void view_annotation(int index)
{
    char buf[MAX_PGN_MOVE_LEN + strlen(ANNOTATION_VIEW_TITLE) + 4];
    int nag = 0, comment = 0;

    if (index < 0 || index > game[gindex].htotal)
	return;

    if (game[gindex].history[index].comment &&
	    game[gindex].history[index].comment[0])
        comment++;
 
    if (game[gindex].history[index].nag[0])
 	nag++;

    if (!nag && !comment)
	return;

    snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_VIEW_TITLE,
	    game[gindex].history[index].move);

    if (comment)
	show_message(buf, (nag) ? "Any other key to continue" : ANYKEY,
		(nag) ? "Press 'n' to view NAG" : NULL, 
		(nag) ? view_nag : NULL, (nag) ? (void *)index : NULL,
		(nag) ? 'n' : 0, "%s", game[gindex].history[index].comment);
    else
	show_message(buf, "Any other key to continue", "Press 'n' to view NAG",
		view_nag, (void *)index, 'n', "%s", 
		"No annotations for this move");

    return;
}

int get_history_by_index(int index, struct history *h)
{
    if (index < 0 || index > game[gindex].htotal - 1)
	return 1;

    *h = game[gindex].history[index];
    return 0;
}

void reset_history()
{
    game[gindex].hindex = game[gindex].htotal = 0;
    return;
}

char *history_edit_nag(void *arg)
{
    WINDOW *win, *subw;
    PANEL *panel;
    ITEM **mitems = NULL;
    MENU *menu;
    int i = 0, n;
    int itemcount = 0;
    int rows, cols;
    char *mbuf = NULL;
    int index = (int)arg;

    if (!nag) {
	if (init_nag())
	    return NULL;
    }

    i = 0;
    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
    mitems[i++] = new_item(NONE, NULL);

    for (n = 0; nag[n].line[0]; n++, i++) {
	mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	mitems[i] = new_item(nag[n].line, NULL);
    }

    mitems[i] = NULL;
    menu = new_menu(mitems);
    scale_menu(menu, &rows, &cols);

    win = newwin(rows + 4, cols + 2, CALCPOSY(rows) - 2, CALCPOSX(cols));
    set_menu_win(menu, win);
    subw = derwin(win, rows, cols, 2, 1);
    set_menu_sub(menu, subw);
    set_menu_fore(menu, A_REVERSE);
    set_menu_grey(menu, A_NORMAL);
    set_menu_mark(menu, NULL);
    set_menu_spacing(menu, 0, 0, 0);
    menu_opts_off(menu, O_NONCYCLIC|O_SHOWDESC|O_ONEVALUE);
    post_menu(menu);
    panel = new_panel(win);
    cbreak();
    noecho();
    keypad(win, TRUE);
    set_menu_pattern(menu, mbuf);
    wbkgd(win, CP_MESSAGE_WINDOW);
    draw_window_title(win, NAG_EDIT_TITLE, cols + 2, CP_HISTORY_TITLE,
	    CP_HISTORY_BORDER);

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (game[gindex].history[index].nag[i] && 
		game[gindex].history[index].nag[i] <= item_count(menu)) {
	    set_item_value(mitems[game[gindex].history[index].nag[i]], TRUE);
	    set_current_item(menu, mitems[game[gindex].history[index].nag[i]]);
	    itemcount++;
	}
    }

    while (1) {
	int c;
	char *tmp;
	char buf[cols - 4];

	wattron(win, A_REVERSE);

	for (c = 1; c < (cols + 2) - 1; c++)
	    mvwprintw(win, rows + 2, c, " ");

	c = item_index(current_item(menu)) + 1;

	snprintf(buf, sizeof(buf), "Item %i of %i (%i of %i selected) %s", c, 
		item_count(menu), itemcount, MAX_PGN_NAG, NAG_EDIT_PROMPT);
	draw_prompt(win, rows + 2, cols + 2, buf, CP_MESSAGE_PROMPT);

	wattroff(win, A_REVERSE);

	if (!itemcount) {
	    for (i = 0; mitems[i]; i++)
		set_item_value(mitems[i], FALSE);

	    set_item_value(mitems[0], TRUE);
	}
	else
	    set_item_value(mitems[0], FALSE);

	/* This nl() statement needs to be here because NL is recognized
	 * for some reason after the first selection.
	 */
	nl();
	update_panels();
	doupdate();

	c = wgetch(win);

	switch (c) {
	    int found;

	    case CTRL('G'):
		help(NAG_EDIT_HELP, naghelp);
		break;
	    case KEY_RIGHT:
		if (!itemcount)
		    break;

		found = 0;

		for (i = item_index(current_item(menu)) + 1; mitems[i]; i++) {
		    if (item_value(mitems[i]) == TRUE) {
			found = i;
			break;
		    }
		}

		if (!found) {
		    for (i = 0; mitems[i]; i++) {
			if (item_value(mitems[i]) == TRUE) {
			    found = i;
			    break;
			}
		    }
		}

		set_current_item(menu, mitems[found]);
		break;
	    case KEY_LEFT:
		if (!itemcount)
		    break;

		found = 0;

		for (i = item_index(current_item(menu)) - 1; i > 0; i--) {
		    if (item_value(mitems[i]) == TRUE) {
			found = i;
			break;
		    }
		}

		if (!found) {
		    for (i = item_count(menu) - 1; i > 0; i--) {
			if (item_value(mitems[i]) == TRUE) {
			    found = i;
			    break;
			}
		    }
		}

		set_current_item(menu, mitems[found]);
		break;
	    case KEY_HOME:
		menu_driver(menu, REQ_FIRST_ITEM);
		break;
	    case KEY_END:
		menu_driver(menu, REQ_LAST_ITEM);
		break;
	    case KEY_UP:
		menu_driver(menu, REQ_UP_ITEM);
		break;
	    case KEY_DOWN:
		menu_driver(menu, REQ_DOWN_ITEM);
		break;
	    case KEY_PPAGE:
	    case CTRL('P'):
		if (menu_driver(menu, REQ_SCR_UPAGE) == E_REQUEST_DENIED)
		    menu_driver(menu, REQ_FIRST_ITEM);
		break;
	    case KEY_NPAGE:
	    case CTRL('N'):
		if (menu_driver(menu, REQ_SCR_DPAGE) == E_REQUEST_DENIED)
		    menu_driver(menu, REQ_LAST_ITEM);
		break;
	    case ' ':
		if (item_index(current_item(menu)) == 0 && 
			item_value(current_item(menu)) == FALSE) {
		    itemcount = 0;
		    break;
		}

		if (item_value(current_item(menu)) == TRUE) {
		    set_item_value(current_item(menu), FALSE);
		    itemcount--;
		}
		else {
		    if (itemcount + 1 > MAX_PGN_NAG)
			break;

		    set_item_value(current_item(menu), TRUE);
		    itemcount++;
		}
		break;
	    case '\n':
		goto gotitem;
		break;
	    case KEY_ESCAPE:
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
    }

gotitem:
    for (i = 0; i < MAX_PGN_NAG; i++)
	game[gindex].history[index].nag[i] = 0;

    for (i = 0, n = 0; mitems[i] && n < MAX_PGN_NAG; i++) {
	if (item_value(mitems[i]) == TRUE)
	    game[gindex].history[index].nag[n++] = i;
    }

done:
    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; mitems[i]; i++)
	free_item(mitems[i]);

    free(mitems);
    del_panel(panel);
    delwin(subw);
    delwin(win);
    return NULL;
}

void add_to_history(struct history **h, int *n, int *t, const char *str)
{
    struct history *history = *h;
    int index = *n;

    history = Realloc(history, (index + 2) * sizeof(struct history));
    memset(&history[index], 0, sizeof(struct history));
    strncpy(history[index].move, str, sizeof(history[index].move));
    memset(&history[++index], 0, sizeof(struct history));

    *n = *t = index;
    *h = history;
    return;
}

void parse_history_move(BOARD b, int index)
{
    int i;

    init_board(b);
    game[gindex].bcaptures = game[gindex].wcaptures = 0;
    status.turn = game[gindex].openingside;

    for (i = 0; i < index; i++) {
	struct history h;

	if (get_history_by_index(i, &h))
	    break;
	
	if (parse_move_text(b, h.move, 1)) {
	    message(NULL, ANYKEY, "Invalid move \"%s\"", h.move);
	    break;
	}

	switch_turn();
    }

    return;
}

void history_previous(BOARD b, int n)
{
    if (game[gindex].hindex - n < 0) {
	if (n != 1)
	    game[gindex].hindex = 0;
	else
	    game[gindex].hindex = game[gindex].htotal;
    }
    else
	game[gindex].hindex -= n;

    parse_history_move(b, game[gindex].hindex);
    return;
}

void history_next(BOARD b, int n)
{
    if (game[gindex].hindex + n > game[gindex].htotal) {
	if (n != 1)
	    game[gindex].hindex = game[gindex].htotal;
	else
	    game[gindex].hindex = 0;
    }
    else
	game[gindex].hindex += n;

    parse_history_move(b, game[gindex].hindex);
    return;
}

void init_history(BOARD b)
{
    browse_history = 1;
    parse_history_move(b, game[gindex].hindex);
    update_status();
    return;
}
