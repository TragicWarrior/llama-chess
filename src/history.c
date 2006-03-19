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
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MENU_H
#include <menu.h>
#endif

#include "common.h"
#include "colors.h"
#include "history.h"

int history_total(HISTORY *h)
{
    int i;

    if (!h)
	return 0;

    for (i = 0; h[i].n != -1; i++);
    return i;
}

// 'start' is for truncating the move history from some move.
void free_history_data(HISTORY *h, int start)
{
    int t = history_total(h);
    int i;

    for (i = start; i < t; i++) {
	if (h[i].comment)
	    free(h[i].comment);

	if (h[i].rav) {
	    free_history_data(h[i].rav, 0);
	    free(h[i].rav);
	}
    }

    if (h)
	free(h);
}

static int init_nag()
{
    FILE *fp;
    char line[LINE_MAX];
    int i = 0;

    if ((fp = fopen(config.nagfile, "r")) == NULL) {
	cmessage(ERROR, ANYKEY, "%s: %s", config.nagfile, strerror(errno));
	return 1;
    }

    while (!feof(fp)) {
	if (fscanf(fp, " %[^\n] ", line) == 1) {
	    nags = Realloc(nags, (i + 2) * sizeof(struct nag_s));
	    nags[i].line = strdup(line);
	    i++;
	}
    }

    if (nags)
	nags[i].line = NULL;
    return 0;
}

static void view_nag(void *arg)
{
    HISTORY *h = (HISTORY *)arg;
    char buf[80];
    char line[LINE_MAX] = {0};
    int i = 0;

    snprintf(buf, sizeof(buf), "Viewing NAG for \"%s\"", h->move);

    if (!nags) {
	if (init_nag())
	    return;
    }

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (!h->nag[i])
	    break;

	strncat(line, nags[h->nag[i] - 1].line, sizeof(line));
	strncat(line, "\n", sizeof(line));
    }

    line[strlen(line) - 1] = 0;
    message(buf, ANYKEY, "%s", line);
}

void view_annotation(HISTORY h)
{
    char buf[MAX_PGN_MOVE_LEN + strlen(ANNOTATION_VIEW_TITLE) + 4];
    int nag = 0, comment = 0;

    if (h.comment && h.comment[0])
        comment++;
 
    if (h.nag[0])
 	nag++;

    if (!nag && !comment)
	return;

    snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_VIEW_TITLE, h.move);

    if (comment)
	show_message(buf, (nag) ? "Any other key to continue" : ANYKEY,
		(nag) ? "Press 'n' to view NAG" : NULL, 
		(nag) ? view_nag : NULL, (nag) ? (void *)&h : NULL,
		(nag) ? 'n' : 0, "%s", h.comment);
    else
	show_message(buf, "Any other key to continue", "Press 'n' to view NAG",
		view_nag, (void *)&h, 'n', "%s", "No annotations for this move");
}

int history_by_index(GAME g, int n, HISTORY *h)
{
    if (n < 0 || n > g.htotal - 1)
	return 1;

    *h = g.hp[n];
    return 0;
}

void reset_history(GAME g)
{
    g.hindex = g.htotal = 0;
    g.hp = g.history;
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
    struct annotation_edit_s *anno = (struct annotation_edit_s *)arg;

    if (!nags) {
	if (init_nag()) {
	    free(anno);
	    return NULL;
	}
    }

    i = 0;
    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
    mitems[i++] = new_item(NONE, NULL);

    for (n = 0; nags[n].line; n++, i++) {
	mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	mitems[i] = new_item(nags[n].line, NULL);
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
	if (anno->h.nag[i] && anno->h.nag[i] <= item_count(menu)) {
	    set_item_value(mitems[anno->h.nag[i]], TRUE);
	    set_current_item(menu, mitems[anno->h.nag[i]]);
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
		help(NAG_EDIT_HELP, ANYKEY, naghelp);
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

		SET_FLAG(game[gindex].flags, GF_MODIFIED);
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
	anno->h.nag[i] = 0;

    for (i = 0, n = 0; mitems[i] && n < MAX_PGN_NAG; i++) {
	if (item_value(mitems[i]) == TRUE)
	    anno->h.nag[n++] = i;
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
    memcpy(&game[anno->game].hp[anno->n], &anno->h, sizeof(HISTORY));
    game[anno->game].hp[anno->n].comment = anno->h.comment;
    free(anno);
    return NULL;
}

void add_to_history(HISTORY **h, int *t, const char *str)
{
    HISTORY *history = *h;

    history = Realloc(history, (*t + 2) * sizeof(HISTORY));
    memset(&history[*t], 0, sizeof(HISTORY));
    strncpy(history[*t].move, str, sizeof(history[*t].move)); //FIXME dymanic
    history[*t].n = *t;
    *t++;
    memset(&history[*t], 0, sizeof(HISTORY));
    history[*t].n = -1;
    *h = history;
}

void parse_history_move(GAME g, int idx)
{
    int i = 0;
    int flags = 0;

    g.bcaptures = g.wcaptures = 0;
    g.turn = g.openingside;

    if (TEST_FLAG(g.flags, GF_PERROR))
	SET_FLAG(flags, GF_PERROR);

    if (TEST_FLAG(g.flags, GF_MODIFIED))
	SET_FLAG(flags, GF_MODIFIED);

    if (TEST_FLAG(g.flags, GF_DELETE))
	SET_FLAG(flags, GF_DELETE);

    if (TEST_FLAG(g.flags, GF_GAMEOVER))
	SET_FLAG(flags, GF_GAMEOVER);
    
    g.flags = flags;
    g.ply = 0;

    init_board(g.b);

    /* FIXME Move numbers and turns. */
    if (g.fentag)
	parse_fen_line(g, g.b, g.tag[g.fentag].value);

    for (i = 0; i < idx; i++) {
	HISTORY h;

	if (history_by_index(g, i, &h))
	    break;
	
	if (parse_move_text(g, g.b, h.move)) {
	    invalid_move(g.n, h.move);
	    break;
	}

	switch_turn(&g);
    }

    if (!status.notify && !g.mode == MODE_HISTORY)
	update_status_notify(g, "%s", GAME_HELP_PROMPT);
}

/* FIXME castling */
static void cursor_from_history(GAME g, int idx, int *r, int *c)
{
    char *p;
    int len;

    if (idx > g.htotal || idx < 0)
	return;

    p = g.hp[idx].move;
    len = strlen(p);

    if (*p == 'O') {
	if (len <= 4)
	    *c = 7;
	else
	    *c = 3;

	*r = (g.turn == WHITE) ? 8 : 1;
	return;
    }

    while (!isdigit(*p))
	p--;

    *r = ROWTOINT(*p--);
    *c = COLTOINT(*p);
}

void history_previous(GAME g, int n, int *r, int *c)
{
    if (g.hindex - n < 0) {
	if ((n == 2 && movestep == 2) || (n == 1 && movestep == 1))
	    g.hindex = g.htotal;
	else
	    g.hindex = 0;
    }
    else
	g.hindex -= n;

    cursor_from_history(g, g.hindex, r, c);
    parse_history_move(g, g.hindex);
}

void history_next(GAME g, int n, int *r, int *c)
{
    if (g.hindex + n > g.htotal) {
	if ((n == 2 && movestep == 2) || (n == 1 && movestep == 1))
	    g.hindex = 0;
	else
	    g.hindex = g.htotal;
    }
    else
	g.hindex += n;

    cursor_from_history(g, game[gindex].hindex, r, c);
    parse_history_move(g, game[gindex].hindex);
}

void init_history(GAME g)
{
    g.mode = MODE_HISTORY;
    parse_history_move(g, game[gindex].hindex);
    //update_status_window();
}
