/* $Id: history.c,v 1.18 2002-12-19 18:19:47 bjk Exp $ */
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
#include "history.h"

void view_annotation(int index)
{
    char buf[MAX_PGN_MOVE_LEN + strlen(VIEW_ANNOTATION) + 4];

    if (!game[gindex].history[index].comment[0])
	return;

    snprintf(buf, sizeof(buf), "%s \"%s\"", VIEW_ANNOTATION,
	    game[gindex].history[index].move);

    message(buf, ANYKEY, "%s", 
	    game[gindex].history[index].comment);
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

void history_edit_nag()
{
    WINDOW *win, *subw;
    PANEL *panel;
    static ITEM **mitems = NULL;
    MENU *menu;
    int i = 0, n;
    int itemcount = 0;
    int rows, cols;
    char *mbuf = NULL;
    FILE *fp;
    char line[LINE_MAX];

    if (!mitems) {
	if ((fp = fopen(config.nagfile, "r")) == NULL) {
	    message(ERROR, ANYKEY, "Could not open NAG file.");
	    return;
	}

	mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	mitems[i++] = new_item(NONE, NULL);

	while (!feof(fp)) {
	    if (fscanf(fp, " %[^\n] ", line) == 1) {
		mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
		mitems[i++] = new_item(strdup(line), NULL);
	    }

	    mitems[i] = NULL;
	}
    }

    menu = new_menu(mitems);
    scale_menu(menu, &rows, &cols);

    win = newwin(rows + 4, cols + 2, CALCPOSY(rows), CALCPOSX(cols));
    set_menu_win(menu, win);
    /* FIXME test. may not need to free subw. */
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
    draw_window_title(win, NAG_TITLE, cols + 2);

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (game[gindex].history[game[gindex].hindex].nag[i]) {
	    set_item_value(mitems[game[gindex].history[game[gindex].hindex].nag[i]], TRUE);
	    set_current_item(menu, mitems[game[gindex].history[game[gindex].hindex].nag[i]]);
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
		item_count(menu), itemcount, MAX_PGN_NAG, NAG_PROMPT);
	mvwprintw(win, rows + 2, CENTERX(cols, buf), "%s", buf);

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
		help(NAG_HELP, naghelp);
		break;
	    case KEY_LEFT:
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
	    case KEY_RIGHT:
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
	    case KEY_UP:
		menu_driver(menu, REQ_UP_ITEM);
		break;
	    case KEY_DOWN:
		menu_driver(menu, REQ_DOWN_ITEM);
		break;
	    case KEY_PPAGE:
	    case CTRL('P'):
		menu_driver(menu, REQ_SCR_UPAGE);
		break;
	    case KEY_NPAGE:
	    case CTRL('N'):
		menu_driver(menu, REQ_SCR_DPAGE);
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
	game[gindex].history[game[gindex].hindex].nag[i] = 0;

    for (i = 0, n = 0; mitems[i] && n < MAX_PGN_NAG; i++) {
	if (item_value(mitems[i]) == TRUE)
	    game[gindex].history[game[gindex].hindex].nag[n++] = i;
    }

done:
    unpost_menu(menu);
    free_menu(menu);

    /*
    for (i = 0; mitems[i]; i++)
	free_item(mitems[i]);
	*/

    del_panel(panel);
    delwin(win);
    delwin(subw);
    return;
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

static char *random_agony()
{
    static int index;
    FILE *fp;
    char line[LINE_MAX];

    if (index == -1 || (browse_history && !config.historyagony))
	return NULL;

    if (!agony) {
	if ((fp = fopen(config.agonyfile, "r")) == NULL) {
	    index = -1;
	    message(ERROR, ANYKEY, "Could not open agony file.");
	    return NULL;
	}

	while (!feof(fp)) {
	    if (fscanf(fp, " %[^\n] ", line) == 1) {
		agony = Realloc(agony, (index + 2) * sizeof(char *));
		agony[index++] = strdup(trim(line));
	    }
	}

	agony[index] = NULL;
	fclose(fp);
    }

    if (agony[0][0] == 0 || !index) {
	index = -1;
	return NULL;
    }

    return agony[random() % index];
}

void move_piece(char *move)
{
    int row, srow;
    int col = 0, scol = 0;
    int n;
    char dst[MAX_PGN_MOVE_LEN + 1], *d = dst;
    char src[MAX_PGN_MOVE_LEN + 1], *s = src;
    char tsrc[2], *t = tsrc;
    char tdst[2], *tt = tdst;
    char *p = move;
    int piece, upper;

    *s++ = *p++;
    *s++ = *t++ = *p++;
    *s = *t = 0;
    *d++ = *p++;
    *d++ = *tt++ = *p++;
    *d = *tt = 0;
    d -= 2;
    s -= 2;
    t--;
    tt--;

    srow = 8 - (int)strtol(t, NULL, 10);
    row = 8 - (int)strtol(tt, NULL, 10);

    for (n = 0; n < strlen(x_grid_chars); n++) {
	if (s[0] == x_grid_chars[n])
	    scol = n;

	if (d[0] == x_grid_chars[n])
	    col = n;
    }

    if (board[row][col].icon != '.') {
	if (isupper(board[row][col].icon))
	    game[gindex].bcaptures++;
	else
	    game[gindex].wcaptures++;

	status.notify = random_agony();
    }
    else {
	/* FIXME */
	/* En Passant. */
	if (row == 2 && board[srow][scol].icon == 'P') {
	    board[row + 1][col].icon = '.';
	    game[gindex].wcaptures++;
	}

	if (row == 5 && board[srow][scol].icon == 'p') {
	    board[row - 1][col].icon = '.';
	    game[gindex].bcaptures++;
	}
    }

    board[row][col].icon = board[srow][scol].icon;
    board[srow][scol].icon = '.';

    upper = isupper(board[row][col].icon);
    piece = tolower(board[row][col].icon);

    if (upper)
	status.turn = BLACK;
    else
	status.turn = WHITE;

    if (piece == 'p' && (row == 0 || row == 7)) {
	board[row][col].icon = (upper) ? 'Q' : 'q';
	status.notify = (upper) ? "White promotion to queen!" :
	    "Black promotion to queen!";
	return;
    }

    if (piece == 'k') {
	n = scol - col;

	if (abs(n) > 1) {
	    if (n > 0) {
		board[row][col + 1].icon = board[row][0].icon;
		board[row][0].icon = '.';
		status.notify = (upper) ? "White castles queen side!" :
		    "Black castles queen side!";
	    }
	    else {
		board[row][col - 1].icon = board[row][7].icon;
		board[row][7].icon = '.';
		status.notify = (upper) ? "White castles king side!" :
		    "Black castles king side!";
	    }

	    return;
	}
    }

    return;
}

static void parse_history_move(int index)
{
    int i;

    init_board();
    game[gindex].bcaptures = game[gindex].wcaptures = 0;

    for (i = 0; i < index; i++) {
	struct history h;

	if (get_history_by_index(i, &h))
	    break;

	move_piece(h.move);
    }

    return;
}

void history_previous(int n)
{
    if (game[gindex].hindex - n < 0) {
	if (n == config.history_jump) {
	    if (game[gindex].hindex == 0)
		return;
	    else
	        game[gindex].hindex = 0;
	}
	else
	    game[gindex].hindex = game[gindex].htotal;
    }
    else
	game[gindex].hindex -= n;

    parse_history_move(game[gindex].hindex);
    return;
}

void history_next(int n)
{
    if (game[gindex].hindex + n > game[gindex].htotal) {
	if (n == config.history_jump) {
	    if (game[gindex].hindex == game[gindex].htotal)
		return;
	    else
	        game[gindex].hindex = game[gindex].htotal;
	}
	else
	    game[gindex].hindex = 0;
    }
    else
	game[gindex].hindex += n;

    parse_history_move(game[gindex].hindex);
    return;
}

void init_history()
{
    parse_history_move(game[gindex].hindex);
    status.engine = HISTORY_MODE;
    browse_history = 1;
    update_status();
    return;
}
