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
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <panel.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <signal.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#include "common.h"
#include "colors.h"
#include "cboard.h"

char *random_agony(GAME g)
{
    static int n;
    FILE *fp;
    char line[LINE_MAX];

    if (n == -1 || !config.agony || !curses_initialized ||
	    (g.mode == MODE_HISTORY && !config.historyagony))
	return NULL;

    if (!agony) {
	if ((fp = fopen(config.agonyfile, "r")) == NULL) {
	    n = -1;
	    cmessage(ERROR, ANYKEY, "%s: %s", config.agonyfile, strerror(errno));
	    return NULL;
	}

	while (!feof(fp)) {
	    if (fscanf(fp, " %[^\n] ", line) == 1) {
		agony = Realloc(agony, (n + 2) * sizeof(char *));
		agony[n++] = strdup(trim(line));
	    }
	}

	agony[n] = NULL;
	fclose(fp);

	if (agony[0] == NULL || !n) {
	    n = -1;
	    return NULL;
	}
    }

    return agony[random() % n];
}

void draw_board(GAME g, int crow, int ccol)
{
    int row, col;
    int bcol = 0, brow = 0;
    int maxy = BOARD_HEIGHT, maxx = BOARD_WIDTH;
    int ncols = 0, offset = 1;
    unsigned coords_y = 8;

    for (row = 0; row < maxy; row++) {
	bcol = 0;

	for (col = 0; col < maxx; col++) {
	    int attrwhich = -1;
	    int attrs = 0;
	    chtype piece, movecount = 0;
	    int bold = 0;

	    if (row == 0 || row == maxy - 2) {
		if (col == 0)
		    mvwaddch(boardw, row, col, 
			    LINE_GRAPHIC((row) ? 
				ACS_LLCORNER | CP_BOARD_GRAPHICS : 
				ACS_ULCORNER | CP_BOARD_GRAPHICS));
		else if (col == maxx - 2)
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC((row) ?
				ACS_LRCORNER | CP_BOARD_GRAPHICS : 
				ACS_URCORNER | CP_BOARD_GRAPHICS));
		else if (!(col % 4))
		    mvwaddch(boardw, row, col, 
			    LINE_GRAPHIC((row) ? 
				ACS_BTEE | CP_BOARD_GRAPHICS : 
				ACS_TTEE | CP_BOARD_GRAPHICS));
		else {
		    if (col != maxx - 1)
			mvwaddch(boardw, row, col,
				LINE_GRAPHIC(ACS_HLINE | CP_BOARD_GRAPHICS));
		}

		continue;
	    }

	    if ((row % 2) && col == maxx - 1 && coords_y) {
		wattron(boardw, CP_BOARD_COORDS);
		mvwprintw(boardw, row, col, "%d", coords_y--);
		wattroff(boardw, CP_BOARD_COORDS);
		continue;
	    }

	    if ((col == 0 || col == maxx - 2) && row != maxy - 1) {
		if (!(row % 2))
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC((col) ?
				ACS_RTEE | CP_BOARD_GRAPHICS : 
				ACS_LTEE | CP_BOARD_GRAPHICS));
		else
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC(ACS_VLINE | CP_BOARD_GRAPHICS));

		continue;
	    }

	    if ((row % 2) && !(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col,
			LINE_GRAPHIC(ACS_VLINE | CP_BOARD_GRAPHICS));
		continue;
	    }

	    if (!(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col,
			LINE_GRAPHIC(ACS_PLUS | CP_BOARD_GRAPHICS));
		continue;
	    }

	    if ((row % 2)) {
		if ((col % 4)) {
		    if (ncols++ == 8) {
			offset++;
			ncols = 1;
		    }

		    if (((ncols % 2) && !(offset % 2)) || (!(ncols % 2) 
				&& (offset % 2)))
			attrwhich = BLACK;
		    else
			attrwhich = WHITE;

		    if (config.validmoves && g.b[brow][bcol].valid) {
			attrs = (attrwhich == WHITE) ? CP_BOARD_MOVES_WHITE :
			    CP_BOARD_MOVES_BLACK;

			if (g.b[brow][bcol].movecount) {
			    if (brow + 1 != crow && bcol + 1 != ccol)
				movecount = (g.b[brow][bcol].movecount + '0');
			}
		    }
		    else
			attrs = (attrwhich == WHITE) ? CP_BOARD_WHITE :
			    CP_BOARD_BLACK;

		    if (row == ROWTOMATRIX(crow) && col == COLTOMATRIX(ccol)) {
			attrs = CP_BOARD_CURSOR;
		    }

		    if (row == ROWTOMATRIX(g.sp.row) && 
			    col == COLTOMATRIX(g.sp.col)) {
			attrs = CP_BOARD_SELECTED;
		    }

		    if (row == maxy - 1)
			attrs = 0;

		    mvwaddch(boardw, row, col, ' ' | attrs);

		    if (row == maxy - 1)
			waddch(boardw, x_grid_chars[bcol] | CP_BOARD_COORDS);
		    else {
			piece = g.b[row / 2][bcol].icon;

			if (attrs & A_BOLD)
			    bold = 1;

			if (g.side == WHITE && isupper(piece))
			    attrs |= A_BOLD;
			else if (g.side == BLACK && islower(piece))
			    attrs |= A_BOLD;

			waddch(boardw, (piece && piece != int_to_piece(g, OPEN_SQUARE)) ? piece | attrs : ' ' | attrs);

			if (!bold)
			    attrs &= ~(A_BOLD);
		    }

		    if (movecount && row != maxy -1)
			waddch(boardw, movecount | CP_BOARD_COUNT);
		    else
			waddch(boardw, ' ' | attrs);

		    col += 2;
		    bcol++;
		}
	    }
	    else {
		if (col != maxx - 1)
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC(ACS_HLINE | CP_BOARD_GRAPHICS));
	    }
	}

	brow = row / 2;
    }
}

/* Convert the selected piece to SAN format and validate it. */
static char *board_to_san(GAME g)
{
    static char str[MAX_PGN_MOVE_LEN + 1], *p;
    int piece;
    int promo;
    BOARD t;

    snprintf(str, sizeof(str), "%c%i%c%i", x_grid_chars[g.sp.col - 1], g.sp.row,
	    x_grid_chars[g.sp.destcol - 1], g.sp.destrow);

    p = str;
    piece = piece_to_int(g.b[ROWTOBOARD(g.sp.row)][COLTOBOARD(g.sp.col)].icon);

    if (piece == PAWN && ((g.sp.destrow == 8 && g.turn == WHITE) ||
		    (g.sp.destrow == 1 && g.turn == BLACK))) {
	promo = cmessage(PROMOTION_TITLE, PROMOTION_PROMPT, PROMOTION_TEXT);
	
	if (piece_to_int(promo) == -1)
	    return NULL;

	p = str + strlen(str);
	*p++ = toupper(promo);
	*p = '\0';
    }

    memcpy(t, g.b, sizeof(BOARD));

    if ((p = a2a4tosan(g, t, str)) == NULL) {
	cmessage(p, ANYKEY, "%s", E_A2A4_PARSE);
	return NULL;
    }

    if (parse_move_text(g, t, p)) {
	invalid_move(g.n, p);
	return NULL;
    }

    memcpy(g.b, t, sizeof(BOARD));
    return p;
}

static int move_to_engine(GAME g)
{
    char *p;

    if ((p = board_to_san(g)) == NULL)
	return 0;

    g.sp.row = g.sp.col = g.sp.icon = 0;

    if (noengine) {
	add_to_history(&g.hp, &g.htotal, p);
	switch_turn(&g);
	SET_FLAG(g.flags, GF_MODIFIED);
	update_all(g);
	return 1;
    }

    SEND_TO_ENGINE("%s\n", p);
    return 1;
}

char *book_method(int method)
{
    char *book;

    switch (method) {
	case BOOK_BEST:
	    book = BOOK_BEST_STR;
	    break;
	case BOOK_WORST:
	    book = BOOK_WORST_STR;
	    break;
	case BOOK_PREFER:
	    book = BOOK_PREFER_STR;
	    break;
	case BOOK_RANDOM:
	    book = BOOK_RANDOM_STR;
	    break;
	case BOOK_OFF:
	    book = BOOK_OFF_STR;
	    break;
	default:
	    book = UNKNOWN;
	    break;
    }

    return book;
}

static void update_clock(int n, int *h, int *m, int *s)
{
    *h = n / 3600;
    *m = (n % 3600) / 60;
    *s = (n % 3600) % 60;

    return;
}

void update_status_window(GAME g)
{
    int i = 0;
    char buf[STATUS_WIDTH - 7];
    char tmp[15], *engine, *mode;
    int w = STATUS_WIDTH - 10;
    int h, m, s;
    char *p;

    *tmp = '\0';
    p = tmp;

    if (TEST_FLAG(g.flags, GF_DELETE)) {
	*p++ = '(';
	*p++ = 'x';
	i++;
    }

    if (TEST_FLAG(g.flags, GF_PERROR)) {
	if (!i)
	    *p++ = '(';
	else
	    *p++ = '/';

	*p++ = '!';
	i++;
    }

    if (TEST_FLAG(g.flags, GF_MODIFIED)) {
	if (!i)
	    *p++ = '(';
	else
	    *p++ = '/';

	*p++ = '*';
	i++;
    }

    if (*tmp != '\0')
	*p++ = ')';

    *p = '\0';

    mvwprintw(statusw, 2, 1, "%*s %-*s", 7, STATUS_FILE_STR, w,
	    (loadfile[0]) ? str_etc(loadfile, w, 1) : UNAVAILABLE);
    snprintf(buf, sizeof(buf), "%i %s %i %s", gindex + 1, N_OF_N_STR, gtotal, 
	    (*tmp) ? tmp : "");
    mvwprintw(statusw, 3, 1, "%*s %-*s", 7, STATUS_GAME_STR, w, buf);

    switch (g.mode) {
	case MODE_HISTORY:
	    mode = MODE_HISTORY_STR;
	    break;
	case MODE_EDIT:
	    mode = MODE_EDIT_STR;
	    break;
	case MODE_PLAY:
	    mode = MODE_PLAY_STR;
	    break;
	default:
	    mode = UNKNOWN;
	    break;
    }

    mvwprintw(statusw, 4, 1, "%*s %-*s", 7, STATUS_MODE_STR, w, mode);

    switch (status.engine) {
	case ENGINE_THINKING:
	    engine = ENGINE_THINKING_STR;
	    break;
	case ENGINE_READY:
	    engine = ENGINE_READY_STR;
	    break;
	case ENGINE_INITIALIZING:
	    engine = ENGINE_INITIALIZING_STR;
	    break;
	case ENGINE_OFFLINE:
	    engine = ENGINE_OFFLINE_STR;
	    break;
	default:
	    engine = UNKNOWN;
	    break;
    }

    mvwprintw(statusw, 5, 1, "%*s %-*s", 7, STATUS_ENGINE_STR, w, " ");
    wattron(statusw, CP_STATUS_ENGINE);
    mvwaddstr(statusw, 5, 9, engine);
    wattroff(statusw, CP_STATUS_ENGINE);

    mvwprintw(statusw, 6, 1, "%*s %-*i", 7, STATUS_DEPTH_STR, w,
	    config.engine_depth);

    mvwprintw(statusw, 7, 1, "%*s %-*s", 7, STATUS_BOOK_STR, w,
	    book_method(config.book_method));

    mvwprintw(statusw, 8, 1, "%*s %-*s", 7, STATUS_TURN_STR, w,
	    (g.turn == WHITE) ? WHITE_STR : BLACK_STR);

    strncpy(tmp, WHITE_STR, sizeof(tmp));
    tmp[0] = toupper(tmp[0]);
    update_clock(g.moveclock, &h, &m, &s);
    snprintf(buf, sizeof(buf), "c/%-2i %.2i:%.2i:%.2i", g.wcaptures, h, m, s);
    mvwprintw(statusw, 9, 1, "%*s: %-*s", 6, tmp, w, buf);

    strncpy(tmp, BLACK_STR, sizeof(tmp));
    tmp[0] = toupper(tmp[0]);
    update_clock(g.moveclock, &h, &m, &s);
    snprintf(buf, sizeof(buf), "c/%-2i %.2i:%.2i:%.2i", g.bcaptures, h, m, s);
    mvwprintw(statusw, 10, 1, "%*s: %-*s", 6, tmp, w, buf);

    for (i = 1; i < STATUS_WIDTH - 4; i++)
	mvwprintw(statusw, STATUS_HEIGHT - 2, i, " ");

    if (!status.notify)
	status.notify = strdup(GAME_HELP_PROMPT);

    wattron(statusw, CP_STATUS_NOTIFY);
    mvwprintw(statusw, STATUS_HEIGHT - 2,
	    CENTERX(STATUS_WIDTH, status.notify), "%s", status.notify);
    wattroff(statusw, CP_STATUS_NOTIFY);
}

void update_history_window(GAME g)
{
    char buf[HISTORY_WIDTH];
    HISTORY h;
    int n, total;

    memset(&h, 0, sizeof(HISTORY));
    n = (g.hindex + 1) / 2;

    if ((g.htotal % 2))
	total = (g.htotal + 1) / 2;
    else
	total = g.htotal / 2;

    if (g.htotal)
	snprintf(buf, sizeof(buf), "%u %s %u%s",n, N_OF_N_STR, total,
		(movestep == 1) ? HISTORY_MOVE_STEP : "");
    else
	strncpy(buf, UNAVAILABLE, sizeof(buf));

    mvwprintw(historyw, 2, 1, "%*s %-*s", 10, HISTORY_MOVE_STR,
	    HISTORY_WIDTH - 13, buf);

    if (history_by_index(g, g.hindex, &h))
	memset(&h, 0, sizeof(HISTORY));

    snprintf(buf, sizeof(buf), "%s %s", (h.move[0]) ? h.move : UNAVAILABLE,
	    ((h.comment && h.comment[0]) || h.nag[0]) ? HISTORY_ANNO_NEXT : "");
    mvwprintw(historyw, 3, 1, "%s %-*s", HISTORY_MOVE_NEXT_STR,
	    HISTORY_WIDTH - 13, buf);

    if (history_by_index(g, game[gindex].hindex - 1, &h))
	memset(&h, 0, sizeof(HISTORY));

    snprintf(buf, sizeof(buf), "%s %s", (h.move[0]) ? h.move : UNAVAILABLE,
	    ((h.comment && h.comment[0]) || h.nag[0]) ? HISTORY_ANNO_PREV : "");
    mvwprintw(historyw, 4, 1, "%s %-*s", HISTORY_MOVE_PREV_STR,
	    HISTORY_WIDTH - 13, buf);
}

void update_tag_window(TAG *t)
{
    int i;
    int w = TAG_WIDTH - 10;

    for (i = 0; i < 7; i++) {
	char *value = t[i].value;
	int n;

	if ((*value == '?' || *value == '-') && value[1] == '\0')
	    value = UNAVAILABLE;
	else if (strcmp(t[i].name, "Result") == 0) {
	    for (n = 0; n < NARRAY(fancy_results); n++) {
		if (strcmp(value, fancy_results[n].pgn) == 0) {
		    value = fancy_results[n].fancy;
		    break;
		}
	    }
	}

	value = str_etc(value, w, 0);
	mvwprintw(tagw, (i + 2), 1, "%*s: %-*s", 6, t[i].name, w, value);
    }
}

void draw_prompt(WINDOW *win, int y, int width, const char *str, chtype attr)
{
    int i;

    wattron(win, attr);

    for (i = 1; i < width - 1; i++)
	mvwaddch(win, y, i, ' ');

    mvwprintw(win, y, CENTERX(width, str), "%s", str);
    wattroff(win, attr);
}

void draw_window_title(WINDOW *win, const char *title, int width, chtype attr,
	chtype battr)
{
    int i;

    if (title) {
	wattron(win, attr);

	for (i = 1; i < width - 1; i++)
	    mvwaddch(win, 1, i, ' ');

	mvwprintw(win, 1, CENTERX(width, title), "%s", title);
	wattroff(win, attr);
    }

    wattron(win, battr);
    box(win, ACS_VLINE, ACS_HLINE);
    wattroff(win, battr);
}

void update_all(GAME g)
{
    update_status_window(g);
    update_history_window(g);
}

static void game_next_prev(GAME g, int n, int count)
{
    if (gtotal < 2)
	return;

    if (n == 1) {
	if (gindex + count > gtotal - 1) {
	    if (count != 1)
		gindex = gtotal - 1;
	    else
		gindex = 0;
	}
	else
	    gindex += count;
    }
    else {
	if (gindex - count < 0) {
	    if (count != 1)
		gindex = 0;
	    else
		gindex = gtotal - 1;
	}
	else
	    gindex -= count;
    }

    init_history(g);
    update_all(g);
    update_tag_window(g.tag);
}

void free_game_data(GAME g)
{
    int i;

    for (i = 0; i < gtotal; i++) {
	free_history_data(g.history, 0);
	free_tag_data(g.tag, g.tindex);
    }

    memset(&g, 0, sizeof(GAME));
}

void free_all_games()
{
    int i;

    for (i = 0; i < gtotal; i++)
	free_game_data(game[i]);

    if (game)
	free(game);
    game = NULL;
}

static void delete_game(int which)
{
    GAME *g = NULL;
    int gi = 0;
    int i;

    for (i = 0; i < gtotal; i++) {
	if (i == which || TEST_FLAG(game[i].flags, GF_DELETE)) {
	    free_game_data(game[i]);
	    continue;
	}

	g = Realloc(g, (gi + 1) * sizeof(GAME));
	memcpy(&g[gi], &game[i], sizeof(GAME));
	g[gi].tag = game[i].tag;
	g[gi].history = game[i].history;
	g[gi].hp = game[i].hp;
	gi++;
    }

    game = g;
    gtotal = gi;

    if (which != -1) {
	if (which + 1 >= gtotal)
	    gindex = gtotal - 1;
	else
	    gindex = which;
    }
    else
	gindex = gtotal - 1;

    game[gindex].hp = game[gindex].history;
}

/* FIXME dont show out of reach counts. Diagonals. */
/*
static void number_valid_moves(BOARD b, int srow, int scol)
{
    int row, col;
    int count;

    for (row = srow + 1, col = scol, count = 1; VALIDFILE(row); row++) {
	if (!b[ROWTOBOARD(row)][COLTOBOARD(col)].valid)
	    continue;

	b[ROWTOBOARD(row)][COLTOBOARD(col)].movecount = count++;
    }

    for (row = srow - 1, col = scol, count = 1; VALIDFILE(row); row--) {
	if (!b[ROWTOBOARD(row)][COLTOBOARD(col)].valid)
	    continue;

	b[ROWTOBOARD(row)][COLTOBOARD(col)].movecount = count++;
    }

    for (col = scol + 1, row = srow, count = 1; VALIDFILE(col); col++) {
	if (!b[ROWTOBOARD(row)][COLTOBOARD(col)].valid)
	    continue;

	b[ROWTOBOARD(row)][COLTOBOARD(col)].movecount = count++;
    }

    for (col = scol - 1, row = srow, count = 1; VALIDFILE(col); col--) {
	if (!b[ROWTOBOARD(row)][COLTOBOARD(col)].valid)
	    continue;

	b[ROWTOBOARD(row)][COLTOBOARD(col)].movecount = count++;
    }

    return;
}
*/

/*
static void get_valid_cursor(BOARD b, int which, int count, int *crow,
	int *ccol, int minr, int maxr, int minc, int maxc)
{
    int row, col;
    int incr, cincr;

    if (which == UP || which == RIGHT)
	incr = 1;
    else
	incr = -(1);

    switch (which) {
	case UP:
	case DOWN:
	    if (count > 1) {
		row = *crow;

		if (which == UP)
		    *crow += count;
		else
		    *crow -= count;

		if (!VALIDFILE(*crow))
		    *crow = row;
	    }

	    for (row = *crow + incr, col = *ccol; VALIDFILE(row); row += incr) {
		if (!b[ROWTOBOARD(row)][COLTOBOARD(col)].valid)
		    continue;

		*crow = row;
		goto done;
	    }

	    break;
	case RIGHT:
	case LEFT:
	    if (count > 1) {
		col = *ccol;

		if (which == RIGHT)
		    *ccol += count;
		else
		    *ccol -= count;

		if (!VALIDFILE(*ccol))
		    *ccol = col;
	    }

	    for (col = *ccol + incr, row = *crow; VALIDFILE(col); col += incr) {
		if (!b[ROWTOBOARD(row)][COLTOBOARD(col)].valid)
		    continue;

		*ccol = col;
		goto done;
	    }

	    break;
	default:
	    break;
    }

    if (*ccol < sp.col || (which == DOWN || which == LEFT))
	cincr = -(1);
    else
	cincr = 1;

    if (*ccol < sp.col && (which == DOWN || which == LEFT))
	cincr = 1;

    for (row = *crow + incr; VALIDFILE(row); row += incr) {
	for (col = *ccol + cincr; VALIDFILE(col); col += cincr) {
	    if (!b[ROWTOBOARD(row)][COLTOBOARD(col)].valid)
		continue;

	    *crow = row;
	    *ccol = col;
	    goto done;
	}
    }

done:
    number_valid_moves(board, *crow, *ccol);
    return;
}
*/

static int find_move_exp(GAME g, const char *str, int init, int which,
	int count)
{
    int i;
    int ret;
    static regex_t r;
    static int firstrun = 1;
    char errbuf[255];
    int incr;
    int found;

    if (init) {
	if (!firstrun)
	    regfree(&r);

	if ((ret = regcomp(&r, str, REG_EXTENDED|REG_NOSUB)) != 0) {
	    regerror(ret, &r, errbuf, sizeof(errbuf));
	    cmessage(E_REGCOMP_TITLE, ANYKEY, "%s", errbuf);
	    return -1;
	}

	firstrun = 1;
    }

    incr = (which == 0) ? -(1) : 1;

    for (i = g.hindex + incr - 1, found = 0; ; i += incr) {
	if (i == g.hindex - 1)
	    break;

	if (i > g.htotal)
	    i = 0;
	else if (i < 0)
	    i = g.htotal;

	// FIXME RAV
	ret = regexec(&r, g.hp[i].move, 0, 0, 0);

	if (ret == 0) {
	    if (count == ++found) {
		return i + 1;
	    }
	}
	else {
	    if (ret != REG_NOMATCH) {
		regerror(ret, &r, errbuf, sizeof(errbuf));
		cmessage(E_REGEXEC_TITLE, ANYKEY, "%s", errbuf);
		return -1;
	    }
	}
    }

    return -1;
}

static int toggle_delete_flag(int n)
{
    int i, x;

    TOGGLE_FLAG(game[n].flags, GF_DELETE);

    for (i = x = 0; i < gtotal; i++) {
	if (TEST_FLAG(game[i].flags, GF_DELETE))
	    x++;
    }

    if (x == gtotal) {
	cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
	CLEAR_FLAG(game[n].flags, GF_DELETE);
	return 1;
    }

    return 0;
}

static void edit_save_tags(GAME g)
{
    int i;
    TAG *t;

    if ((t = edit_tags(g, 1)) == NULL)
	return;

    g.tindex = 0;

    for (i = 0; t[i].name; i++)
	add_tag(&g.tag, &g.tindex, t[i].name, t[i].value);

    free_tag_data(t, i);
    free(t);
    SET_FLAG(g.flags, GF_MODIFIED);
}

static int find_game_exp(char *str, int which, int count)
{
    char *nstr = NULL, *exp = NULL;
    regex_t nexp, vexp;
    int ret = -1;
    int g = 0;
    char buf[255], *tmp;
    char errbuf[255];
    int found = 0;
    int incr = (which == 0) ? -(1) : 1;

    strncpy(buf, str, sizeof(buf));
    tmp = buf;

    if (strstr(tmp, ":") != NULL) {
	nstr = strsep(&tmp, ":");

	if ((ret = regcomp(&nexp, nstr,
			REG_ICASE|REG_EXTENDED|REG_NOSUB)) != 0) {
	    regerror(ret, &nexp, errbuf, sizeof(errbuf));
	    cmessage(E_REGCOMP_TITLE, ANYKEY, "%s", errbuf);
	    ret = g = -1;
	    goto cleanup;
	}
    }

    exp = tmp;

    if (exp == NULL)
	goto cleanup;

    if ((ret = regcomp(&vexp, exp, REG_EXTENDED|REG_NOSUB)) != 0) {
	regerror(ret, &vexp, errbuf, sizeof(errbuf));
	cmessage(E_REGCOMP_TITLE, ANYKEY, "%s", errbuf);
	ret = -1;
	goto cleanup;
    }

    ret = -1;

    for (g = gindex + incr, found = 0; ; g += incr) {
	int t;

	if (g == gindex)
	    break;

	if (g == gtotal)
	    g = 0;
	else if (g < 0)
	    g = gtotal - 1;

	for (t = 0; t < game[g].tindex; t++) {
	    if (nstr) {
		if (regexec(&nexp, game[g].tag[t].name, 0, 0, 0) == 0) {
		    if (regexec(&vexp, game[g].tag[t].value, 0, 0, 0) == 0) {
			if (count == ++found) {
			    ret = g;
			    goto cleanup;
			}
		    }
		}
	    }
	    else {
		if (regexec(&vexp, game[g].tag[t].value, 0, 0, 0) == 0) {
		    if (count == ++found) {
			ret = g;
			goto cleanup;
		    }
		}
	    }
	}

	ret = -1;
    }

cleanup:
    if (nstr)
	regfree(&nexp);

    if (g != -1)
	regfree(&vexp);

    return ret;
}

void edit_board(GAME g)
{
    chtype p;

    p = g.b[ROWTOBOARD(g.sp.row)][COLTOBOARD(g.sp.col)].icon;
    g.b[ROWTOBOARD(g.sp.destrow)][COLTOBOARD(g.sp.destcol)].icon = p;
    g.b[ROWTOBOARD(g.sp.row)][COLTOBOARD(g.sp.col)].icon = 
	int_to_piece(g, OPEN_SQUARE);
}

// Updates the notification line in the status window then refreshes the
// status window.
void update_status_notify(GAME g, char *fmt, ...)
{
    va_list ap;
#ifdef HAVE_VASPRINTF
    char *line;
#else
    char line[COLS];
#endif

    if (!fmt) {
	if (status.notify) {
	    free(status.notify);
	    status.notify = NULL;

	    if (curses_initialized)
		update_status_window(g);
	}

	return;
    }

    va_start(ap, fmt);
#ifdef HAVE_VASPRINTF
    vasprintf(&line, fmt, ap);
#else
    vsnprintf(line, sizeof(line), fmt, ap);
#endif
    va_end(ap);

    if (status.notify)
	free(status.notify);

    status.notify = strdup(line);

#ifdef HAVE_VASPRINTF
    free(line);
#endif
    if (curses_initialized)
	update_status_window(g);
}

static void switch_side(GAME *g)
{
    if ((*g).side == WHITE)
	(*g).side = BLACK;
    else
	(*g).side = WHITE;
}

static struct annotation_edit_s *init_annotation_edit(int g, int n, HISTORY h)
{
    struct annotation_edit_s *a;

    a = Malloc(sizeof(struct annotation_edit_s));
    a->game = g;
    a->n = n;
    memcpy(&a->h, &h, sizeof(HISTORY));
    a->h.comment = h.comment;
    return a;
}

void game_loop()
{  
    int error_recover = 0;
    int pushkey = 0;
    int count = 0;
    int crow = 2, ccol = 5;
    char moveexp[255] = {0};
    char gameexp[255] = {0};
    int delete_count = 0;
    int markstart = -1, markend = -1;
    int editmode = 0;

    gindex = gtotal - 1;
    markstart = -1, markend = -1;

    if (loadfile[0])
	init_history(game[gindex]);

    update_status_notify(game[gindex], "%s", GAME_HELP_PROMPT);
    movestep = 2;
    paused = 1; //FIXME clock
    flushinp();
    update_all(game[gindex]);
    update_tag_window(game[gindex].tag);

    while (!quit) {
	int c = 0;
	fd_set fds;
	int i, x, n = 0, len = 0;
	char fdbuf[8192] = {0};
	struct timeval tv;
	char *tmp = NULL;
	char buf[78];
	char tfile[FILENAME_MAX];
	int minr, maxr, minc, maxc;
	struct annotation_edit_s *anno = NULL;

	// FIXME game.fds
#if 0
	if (engine_initialized) {
	    tv.tv_sec = 0;
	    tv.tv_usec = 0;

	    FD_ZERO(&fds);
	    FD_SET(enginefd[0], &fds);

	    for (i = 0; i < gtotal; i++) {
		if (game[i].sockfd > 0) {
		    if (game[i].sockfd > n)
			n = game[i].sockfd;

		    FD_SET(game[i].sockfd, &fds);
		}
	    }

	    n = (n > enginefd[0]) ? n : enginefd[0];

	    if ((n = select(n + 1, &fds, NULL, NULL, &tv)) > 0) {
		if (FD_ISSET(enginefd[0], &fds)) {
		    len = read(enginefd[0], fdbuf, sizeof(fdbuf));

		    if (len == -1) {
			if (errno != EAGAIN) {
			    cmessage(ERROR, ANYKEY, "Attempt #%i. read(): %s",
				    ++error_recover, strerror(errno));
			    continue;
			}
		    }
		    else {
			if (len) {
			    // FIXME engine may be associated with another
			    // selected game.
			    parse_engine_output(game[gindex].b, fdbuf);
			    update_all(game[gindex]);
			}
		    }
		}

		for (i = 0; i < gtotal; i++) {
		    if (game[i].sockfd <= 0)
			continue;

		    if (FD_ISSET(game[i].sockfd, &fds)) {
			len = recv(game[i].sockfd, fdbuf, sizeof(fdbuf), 0);

			if (len == -1) {
			    if (errno != EAGAIN) {
				cmessage(ERROR, ANYKEY, 
					"Attempt #%i. recv(): %s", 
					++error_recover, strerror(errno));
				continue;
			    }
			}
			else {
			    if (len)
				parse_ics_output(fdbuf);

			    update_all(game[gindex]);
			}
		    }
		}
	    }
	    else {
		if (n == -1)
		    cmessage(ERROR, ANYKEY, "select(): %s", strerror(errno));
		else {
		    /* timeout */
		}
	    }
	}
#endif

	error_recover = 0;
	draw_board(game[gindex], crow, ccol);

	wmove(boardw, ROWTOMATRIX(crow), COLTOMATRIX(ccol));

	if (!paused) {
	}

	update_panels();
	doupdate();

	if (pushkey)
	    c = pushkey;
	else {
	    if ((c = wgetch(boardw)) == ERR)
		continue;
	}

	if (!count && status.notify)
	    update_status_notify(game[gindex], NULL);

	switch (c) {
	    int annotate;

	    case 'p':
		if (paused)
		    paused = 0;
		else
		    paused = 1;

		break;
	    case 'e':
		if (game[gindex].htotal)
		    break;

	        if (editmode) {
		    editmode = 0;
		    add_tag(&game[gindex].tag, &game[gindex].tindex,
			    "FEN", board_to_fen(game[gindex]));
		    add_tag(&game[gindex].tag, &game[gindex].tindex,
			    "SetUp", "1");
		    game[gindex].mode = MODE_PLAY;
		    game[gindex].fentag = find_tag(game[gindex], "FEN");
		}
		else {
		    game[gindex].mode = MODE_EDIT;
		    editmode = 1;
		}

		update_all(game[gindex]);
		break;
	    case '}':
	    case '{':
	    case '?':
		if (gtotal < 2)
		    break;

		if (!*gameexp || c == '?') {
		    if ((tmp = get_input(GAME_FIND_EXPRESSION_TITLE, gameexp, 
				    1, 1, GAME_FIND_EXPRESSION_PROMPT, NULL, 
				    NULL, 0, -1)) == NULL)
			break;

		    strncpy(gameexp, tmp, sizeof(gameexp));
		}

	        if ((n = find_game_exp(gameexp, (c == '{') ? 0 : 1,
				(count) ? count : 1)) == -1)
		    break;

		gindex = n;
		init_history(game[gindex]);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case '!':
	        crow = 1;
		break;
	    case '@':
	        crow = 2;
		break;
	    case '#':
	        crow = 3;
		break;
	    case '$':
	        crow = 4;
		break;
	    case '%':
	        crow = 5;
		break;
	    case '^':
	        crow = 6;
		break;
	    case '&':
	        crow = 7;
		break;
	    case '*':
	        crow = 8;
		break;
	    case 'A':
	        ccol = 1;
		break;
	    case 'B':
	        ccol = 2;
		break;
	    case 'C':
	        ccol = 3;
		break;
	    case 'D':
	        ccol = 4;
		break;
	    case 'E':
	        ccol = 5;
		break;
	    case 'F':
	        ccol = 6;
		break;
	    case 'G':
	        ccol = 7;
		break;
	    case 'H':
	        ccol = 8;
		break;
	    case '_':
	    case '+':
		if (status.engine != ENGINE_READY)
		    break;

		n = (count) ? count : 1;

		if (c == '_') {
		    if (config.engine_depth - n < 0)
			n = 0;
		    else
			n -= config.engine_depth;
		}
		else
		    n += config.engine_depth;

		SEND_TO_ENGINE("depth %i\n", abs(n));
		break;
	    case ']':
	    case '[':
	    case '/':
		if (game[gindex].htotal < 2)
		    break;

		n = 0;

	        if (!*moveexp || c == '/') {
		    if ((tmp = get_input(FIND_REGEXP, moveexp, 1, 1, NULL, 
				    NULL, NULL, 0, -1)) == NULL)
			break;

		    strncpy(moveexp, tmp, sizeof(moveexp));
		    n = 1;
		}

		if ((n = find_move_exp(game[gindex], moveexp, n, 
				(c == '[') ? 0 : 1, (count) ? count : 1)) == -1)
		    break;

		game[gindex].hindex = n;
		parse_history_move(game[gindex], game[gindex].hindex);
		update_all(game[gindex]);
		break;
	    case 'v':
	        view_annotation(game[gindex].hp[game[gindex].hindex]);
		break;
	    case 'V':
		if (game[gindex].hindex - 1 >= 0)
		    view_annotation(game[gindex].hp[game[gindex].hindex - 1]);
		break;
	    case '>':
		game_next_prev(game[gindex], 1, (count) ? count : 1);

		if (delete_count) {
		    markend = gindex;
		    pushkey = 'x';
		    delete_count = 0;
		}

		game[gindex].mode = MODE_HISTORY;
		editmode = 0;
		break;
	    case '<':
		game_next_prev(game[gindex], 0, (count) ? count : 1);

		if (delete_count) {
		    markend = gindex;
		    pushkey = 'x';
		    delete_count = 0;
		}

		game[gindex].mode = MODE_HISTORY;
		editmode = 0;
		break;
	    case 'j':
		if (game[gindex].mode != MODE_HISTORY || 
			game[gindex].htotal < 2)
		    break;

		/*
		if ((tmp = get_input(GAME_HISTORY_JUMP_TITLE, NULL, 1, 1, 
				NULL, NULL, NULL, 0, FIELD_TYPE_INTEGER, 1, 0, 
				game[gindex].htotal)) == NULL)
		    break;
		*/

		if (!count) {
		    if ((tmp = get_input(GAME_HISTORY_JUMP_TITLE, NULL, 1, 1, 
				    NULL, NULL, NULL, 0, -1)) == NULL)
			break;

		    if (!isinteger(tmp))
			break;

		    i = atoi(tmp);
		}
		else
		    i = count;

		if (i > (game[gindex].htotal / 2) || i < 0)
		    break;

		game[gindex].hindex = i * 2;
		init_history(game[gindex]);
		update_all(game[gindex]);
		break;
	    case 'J':
		if (gtotal < 2)
		    break;

		/*
		if ((tmp = get_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL, NULL,
				NULL, 0, FIELD_TYPE_INTEGER, 1, 1, gtotal))
			== NULL)
		    break;
		*/

		if (!count) {
		    if ((tmp = get_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL,
				    NULL, NULL, 0, -1)) == NULL)
			break;

		    if (!isinteger(tmp))
			break;

		    i = atoi(tmp);
		}
		else
		    i = count;

		if (--i > gtotal - 1 || i < 0)
		    break;

		gindex = i;
		init_history(game[gindex]);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'x':
		pushkey = 0;

		if (editmode) {
		    if (game[gindex].sp.icon)
			game[gindex].b[ROWTOBOARD(game[gindex].sp.row)][COLTOBOARD(game[gindex].sp.col)].icon = int_to_piece(game[gindex], OPEN_SQUARE);
		    else
			game[gindex].b[ROWTOBOARD(crow)][COLTOBOARD(ccol)].icon = int_to_piece(game[gindex], OPEN_SQUARE);

		    game[gindex].sp.icon = game[gindex].sp.row = game[gindex].sp.col = 0;
		    break;
		}

		if (gtotal < 2)
		    break;

		if (count && !delete_count) {
		    markstart = gindex;
		    delete_count = 1;
		    update_status_notify(game[gindex], "%s (delete)",
			    status.notify);
		    continue;
		}

		if (markstart >= 0 && markend >= 0) {
		    if (markstart > markend) {
			i = markstart;
			markstart = markend;
			markend = i;
		    }

		    for (i = markstart; i <= markend; i++) {
			if (toggle_delete_flag(i))
			    break;
		    }
		}
		else {
		    if (toggle_delete_flag(gindex))
			break;
		}

		markstart = markend = -1;
		update_status_window(game[gindex]);
		break;
	    case 'X':
		if (gtotal < 2) {
		    cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
		    break;
		}

		tmp = NULL;

		for (i = n = 0; i < gtotal; i++) {
		    if (TEST_FLAG(game[i].flags, GF_DELETE))
			n++;
		}

		if (!n)
		    tmp = GAME_DELETE_GAME_TEXT;
		else {
		    if (n == gtotal) {
			cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
			break;
		    }

		    tmp = GAME_DELETE_ALL_TEXT;
		}

		if (config.deleteprompt) {
		    if ((c = cmessage(NULL, YESNO, "%s", tmp)) != 'y')
			break;
		}

		delete_game((!n) ? gindex : -1);
		init_history(game[gindex]);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'a':
	        annotate = game[gindex].hindex;

		if (annotate && game[gindex].hp[annotate - 1].move[0])
		    annotate--;
		else
		    break;

		snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_EDIT_TITLE,
			game[gindex].hp[annotate].move);

		anno = init_annotation_edit(gindex, annotate, 
			game[gindex].hp[annotate]);
		tmp = get_input(buf, game[gindex].hp[annotate].comment, 
			0, 0, NAG_PROMPT, history_edit_nag, (void *)anno, 
			CTRL('T'), -1);

		if (!tmp && (!game[gindex].hp[annotate].comment ||
			    !*game[gindex].hp[annotate].comment))
		    break;
		else if (tmp && game[gindex].hp[annotate].comment) {
		    if (strcmp(tmp, game[gindex].hp[annotate].comment) == 0)
			break;
		}
		    
		len = (tmp) ? strlen(tmp) + 1 : 1;

		game[gindex].hp[annotate].comment = 
		    Realloc(game[gindex].hp[annotate].comment, len);

		strncpy(game[gindex].hp[annotate].comment,
			(tmp) ? tmp : "", len);

		SET_FLAG(game[gindex].flags, GF_MODIFIED);
		update_all(game[gindex]);
		break;
	    case 't':
		edit_save_tags(game[gindex]);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'I':
		if (!editmode)
		    break;

		c = message(GAME_EDIT_TITLE, GAME_EDIT_PROMPT, "%s",
			GAME_EDIT_TEXT);

		if (piece_to_int(c) == -1 && tolower(c) != 'x')
		    break;

		if (tolower(c) == 'x')
		    c = tolower(c);

		if (c == 'x' && (crow != 6 && crow != 3))
		    break;

		if (c == 'x') {
		    for (i = 0; i < 8; i++) {
			if (game[gindex].b[ROWTOBOARD(3)][COLTOBOARD(i)].icon == 'x')
			    game[gindex].b[ROWTOBOARD(3)][COLTOBOARD(i)].icon = OPEN_SQUARE;
			if (game[gindex].b[ROWTOBOARD(6)][COLTOBOARD(i)].icon == 'x')
			    game[gindex].b[ROWTOBOARD(6)][COLTOBOARD(i)].icon = OPEN_SQUARE;
		    }
		}

		game[gindex].b[ROWTOBOARD(crow)][COLTOBOARD(ccol)].icon = c;
		break;
	    case 'i':
		edit_tags(game[gindex], 0);
		break;
	    case 'g':
		if (game[gindex].mode == MODE_HISTORY || 
			status.engine == ENGINE_THINKING)
		    break;

		status.engine = ENGINE_THINKING;
		update_status_window(game[gindex]);
		SEND_TO_ENGINE("go\n");
		break;
	    case 'b':
		if (config.book_method == -1 || status.engine ==
			ENGINE_THINKING || config.engine != GNUCHESS)
		    break;

		if (config.book_method + 1 >= BOOK_MAX)
		    n = 0;
		else
		    n = config.book_method + 1;

		SEND_TO_ENGINE("book %s\n", book_methods[n]);
		break;
	    case 'h':
		if (game[gindex].mode == MODE_HISTORY) {
		    if (game[gindex].openingside == BLACK) {
			cmessage(NULL, ANYKEY, "%s", E_RESUME_BLACK);
			break;
		    }

		    if (game[gindex].hindex != game[gindex].htotal) {
			if (!pushkey) {
			    if ((c = message(NULL, YESNO, "%s",
					    GAME_RESUME_HISTORY_TEXT)) != 'y')
				break;
			}
		    }
		    else {
			if (TEST_FLAG(game[gindex].flags, GF_GAMEOVER))
			    break;
		    }

		    if (!noengine)
			wtimeout(boardw, 70);

		    if (!noengine && !engine_initialized) {
			if (start_chess_engine() < 0)
			    break;

			pushkey = 'h';
			break;
		    }

		    pushkey = 0;
		    oldhistorytotal = game[gindex].htotal;
		    game[gindex].htotal = game[gindex].hindex;
		    game[gindex].mode = MODE_PLAY;
		    status.engine = ENGINE_READY;

		    /* FIXME crafty */
		    if (config.engine != GNUCHESS)
			SEND_TO_ENGINE("read %s\n", config.fifo);
		    else
			SEND_TO_ENGINE("\npgnload %s\n", config.fifo);

		    update_all(game[gindex]);
		    break;
		}

		if (!game[gindex].htotal || status.engine == ENGINE_THINKING)
		    break;

		wtimeout(boardw, -1);
		init_history(game[gindex]);
		break;
	    case 'u':
		/* FIXME dies reading FIFO sometimes. */
		if (game[gindex].mode != MODE_PLAY || !game[gindex].htotal)
		    break;

		history_previous(game[gindex], (count) ? count * 2 : 2, &crow, &ccol);
		oldhistorytotal = game[gindex].htotal;
		game[gindex].htotal = game[gindex].hindex;

		if (status.engine == CRAFTY)
		    SEND_TO_ENGINE("read %s\n", config.fifo);
		else
		    SEND_TO_ENGINE("\npgnload %s\n", config.fifo);

		update_history_window(game[gindex]);
		break;
	    case 'r':
		if ((tmp = get_input(GAME_LOAD_TITLE, NULL, 1, 1,
				BROWSER_PROMPT, browse_directory, NULL, 
				'\t', -1)) == NULL)
		    break;

		tmp = tilde_expand(tmp);

		if (parse_pgn_file(tmp))
		    break;

		gindex = gtotal - 1;
		strncpy(loadfile, tmp, sizeof(loadfile));
		init_history(game[gindex]);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'S':
	    case 's':
		x = -1;

		if (gtotal > 1) {
		    n = message(NULL, GAME_SAVE_MULTI_PROMPT, "%s", 
			    GAME_SAVE_MULTI_TEXT);

		    if (n == 'c')
			x = gindex;
		    else if (n == 'a')
			x = -1;
		    else {
			update_status_notify(game[gindex], "%s", NOTIFY_SAVE_ABORTED);
			break;
		    }
		}

		if ((tmp = get_input(GAME_SAVE_TITLE, loadfile, 1, 1,
				BROWSER_PROMPT, browse_directory, NULL, 
				'\t', -1)) == NULL) {
		    update_status_notify(game[gindex], "%s", NOTIFY_SAVE_ABORTED);
		    break;
		}

		tmp = tilde_expand(tmp);

		if (strstr(tmp, ".") == NULL && compression_cmd(tmp, 0)
			== NULL) {
		    snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
		    tmp = tfile;
		}

		if (save_pgn(tmp, 0, x)) {
		    update_status_notify(game[gindex], "%s", NOTIFY_SAVE_FAILED);
		    break;
		}

		update_status_notify(game[gindex], "%s", NOTIFY_SAVED);
		update_all(game[gindex]);
		break;
	    case CTRL('G'):
		n = 0;

		while (n != 'q') {
		    n = help(GAME_HELP_INDEX_TITLE,
			    GAME_HELP_INDEX_PROMPT, mainhelp);

		    switch (n) {
			case 'h':
			    help(GAME_HELP_HISTORY_TITLE, ANYKEY, historyhelp);
			    break;
			case 'p':
			    help(GAME_HELP_PLAY_TITLE, ANYKEY, playhelp);
			    break;
			case 'e':
			    help(GAME_HELP_EDIT_TITLE, ANYKEY, edithelp);
			    break;
			case 'g':
			    help(GAME_HELP_GAME_TITLE, ANYKEY, gamehelp);
			    break;
			default:
			    n = 'q';
			    break;
		    }
		}

		break;
	    case 'n':
	    case 'N':
		if (c == 'N') {
		    if (cmessage(NULL, YESNO, "%s", GAME_NEW_PROMPT) != 'y')
			break;
		}

		game[gindex].mode = MODE_PLAY;
		editmode = 0;
		game[gindex].sp.icon = 0;

		if (c == 'n') {
		    newgameinit = 1;
		    new_game();
		}
		else {
		    reset_history(game[gindex]);
		    loadfile[0] = '\0';
		    parse_pgn_file(loadfile);
		}

		game[gindex].wcaptures = game[gindex].bcaptures = 0;
		crow = (game[gindex].side == WHITE) ? 2 : 7;
		ccol = 4;

		if (!noengine && (status.engine == ENGINE_OFFLINE ||
			engine_initialized == 0)) {
		    if (start_chess_engine() < 0)
			break;
		}

		SEND_TO_ENGINE("\nnew\n");
		set_engine_defaults();
		status.engine = ENGINE_READY;
		update_status_notify(game[gindex], NULL);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case CTRL('L'):
	    case 'R':
		endwin();
		keypad(boardw, TRUE);
		update_panels();
		doupdate();
		break;
	    case 'c':
		if (status.engine == ENGINE_THINKING)
		    break;

		if (status.engine == ENGINE_OFFLINE)
		    break;

		if ((tmp = get_input_str_clear(ENGINE_CMD_TITLE, NULL)) 
			!= NULL) {
		    SEND_TO_ENGINE("%s\n", tmp);
		}
		break;
	    case KEY_ESCAPE:
		game[gindex].sp.icon = game[gindex].sp.row = game[gindex].sp.col = 0;
		markend = markstart = 0;

		if (count) {
		    count = 0;
		    update_status_notify(game[gindex], NULL);
		}

		if (config.validmoves)
		    reset_valid_moves(game[gindex].b);

		break;
	    case '0' ... '9':
		n = c - '0';

		if (count)
		    count = count * 10 + n;
		else
		    count = n;

		update_status_notify(game[gindex], "Repeat %i", count);
		continue;
	    case KEY_UP:
		if (game[gindex].mode == MODE_HISTORY) {
		    history_next(game[gindex], (count > 0) ?
			    config.jumpcount * count * movestep : 
			    config.jumpcount * movestep, &crow, &ccol);
		    update_all(game[gindex]);
		    break;
		}

		/*
		if (sp.icon && config.validmoves) {
		    get_valid_cursor(board, UP, (count) ? count : 1, 
			    &crow, &ccol, minr, maxr, minc, maxc);
		    break;
		}
		*/

		if (count) {
		    crow += count;
		    pushkey = '\n';
		}
		else
		    crow++;

		if (crow > 8)
		    crow = 1;

		break;
	    case KEY_DOWN:
		if (game[gindex].mode == MODE_HISTORY) {
		    history_previous(game[gindex], (count) ?
			    config.jumpcount * count * movestep : 
			    config.jumpcount * movestep, &crow, &ccol);
		    update_all(game[gindex]);
		    break;
		}

		/*
		if (sp.icon && config.validmoves) {
		    get_valid_cursor(board, DOWN, (count) ? count : 1, 
			    &crow, &ccol, minr, maxr, minc, maxc);
		    break;
		}
		*/

		if (count) {
		    crow -= count;
		    pushkey = '\n';
		    update_status_notify(game[gindex], NULL);
		}
		else
		    crow--;

		if (crow < 1)
		    crow = 8;

		break;
	    case KEY_LEFT:
		if (game[gindex].mode == MODE_HISTORY) {
		    history_previous(game[gindex], (count) ?
			    count * movestep : movestep, &crow, &ccol);
		    update_all(game[gindex]);
		    break;
		}

		/*
		if (sp.icon && config.validmoves) {
		    get_valid_cursor(board, LEFT, (count) ? count : 1, 
			    &crow, &ccol, minr, maxr, minc, maxc);
		    break;
		}
		*/

		if (count) {
		    ccol -= count;
		    pushkey = '\n';
		}
		else
		    ccol--;

		if (ccol < 1)
		    ccol = 8;

		break;
	    case KEY_RIGHT:
		if (game[gindex].mode == MODE_HISTORY) {
		    history_next(game[gindex], (count) ? count * movestep 
			    : movestep, &crow, &ccol);
		    update_all(game[gindex]);
		    break;
		}

		/*
		if (sp.icon && config.validmoves) {
		    get_valid_cursor(board, RIGHT, (count) ? count : 1, 
			    &crow, &ccol, minr, maxr, minc, maxc);
		    break;
		}
		*/

		if (count) {
		    ccol += count;
		    pushkey = '\n';
		}
		else
		    ccol++;

		if (ccol > 8)
		    ccol = 1;

		break;
	    case 'w':
		if (game[gindex].mode == MODE_HISTORY)
		    break;

		if (game[gindex].mode == MODE_EDIT)
		    switch_turn(&game[gindex]);

		/* FIXME crafty. */
		SEND_TO_ENGINE("\nswitch\n");
		switch_side(&game[gindex]);
		update_status_window(game[gindex]);
		break;
	    case ' ':
		if (!editmode && game[gindex].mode == MODE_HISTORY) {
		    if (movestep == 1)
			movestep = 2;
		    else
			movestep = 1;

		    update_history_window(game[gindex]);
		    break;
		}

		if (!noengine && (status.engine == ENGINE_OFFLINE ||
			    !engine_initialized) && !editmode) {
		    if (start_chess_engine() < 0) {
			game[gindex].sp.icon = 0;
			break;
		    }

		}

		if (!editmode)
		    wtimeout(boardw, 70);

		if (game[gindex].sp.icon || (!editmode && status.engine == ENGINE_THINKING)) {
		    beep();
		    break;
		}

		game[gindex].sp.icon = mvwinch(boardw, ROWTOMATRIX(crow), 
			COLTOMATRIX(ccol)+1) & A_CHARTEXT;

		if (game[gindex].sp.icon == ' ') {
		    game[gindex].sp.icon = 0;
		    break;
		}

		if (!editmode && ((islower(game[gindex].sp.icon) &&
				game[gindex].turn != BLACK) ||
			    (isupper(game[gindex].sp.icon) &&
			     game[gindex].turn != WHITE))) {
		    message(NULL, ANYKEY, "%s", E_SELECT_TURN);
		    game[gindex].sp.icon = 0;
		    break;
		}

		game[gindex].sp.row = crow;
		game[gindex].sp.col = ccol;

		if (!editmode && config.validmoves) {
		    get_valid_moves(game[gindex], game[gindex].b, 
			    piece_to_int(game[gindex].sp.icon),
			    game[gindex].sp.row, game[gindex].sp.col, &minr,
			    &maxr, &minc, &maxc);
		    /*
		    number_valid_moves(board, sp.row, sp.col);
		    */
		}

		if (game[gindex].mode == MODE_PLAY)
		    paused = 0;
		break;
	    case '\015':
	    case '\n':
		pushkey = count = 0;
		update_status_notify(game[gindex], NULL);

		if (!editmode && game[gindex].mode == MODE_HISTORY)
		    break;

		if (status.engine == ENGINE_THINKING) {
		    beep();
		    break;
		}

		if (!game[gindex].sp.icon)
		    break;

		game[gindex].sp.destrow = crow;
		game[gindex].sp.destcol = ccol;

		if (editmode) {
		    edit_board(game[gindex]);
		    game[gindex].sp.icon = game[gindex].sp.row = game[gindex].sp.col = 0;
		    break;
		}

		if (move_to_engine(game[gindex])) {
		    if (config.validmoves)
			reset_valid_moves(game[gindex].b);

		    if (TEST_FLAG(game[gindex].flags, GF_GAMEOVER)) {
			CLEAR_FLAG(game[gindex].flags, GF_GAMEOVER);
			SET_FLAG(game[gindex].flags, GF_MODIFIED);
		    }
		}

		break;
	    case 'q':
		quit = 1;
		break;
	    case 0:
		break;
	    default:
		beep();
		break;
	}

	count = 0;
    }
}

void usage(const char *pn, int ret)
{
    int i;

    for (i = 0; cmdlinehelp[i]; i++)
	fputs(cmdlinehelp[i], stderr);

    exit(ret);
}

void catch_signal(int which)
{
    switch (which) {
	case SIGINT:
	    stop_engine();
	    endwin();
	    exit(EXIT_FAILURE);
	    break;
	case SIGPIPE:
	    if (quit)
		break;

	    cmessage(NULL, ANYKEY, "%s", E_BROKEN_PIPE);
	    endwin();
	    exit(EXIT_FAILURE);
	    break;
	case SIGSTOP:
	    savetty();
	    break;
	case SIGCONT:
	    resetty();
	    keypad(boardw, TRUE);
	    break;
	default:
	    break;
    }
}

int main(int argc, char *argv[])
{
    int opt;
    struct stat st;
    char buf[FILENAME_MAX];
    char datadir[FILENAME_MAX];
    int ret = EXIT_SUCCESS;
    int validate = 0, validate_and_save = 0;

    if ((config.pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    snprintf(datadir, sizeof(datadir), "%s/.cboard", config.pwd->pw_dir);
    snprintf(buf, sizeof(buf), "%s/cc.data", datadir);
    config.ccfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/nag.data", datadir);
    config.nagfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/agony.data", datadir);
    config.agonyfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/config", datadir);
    config.configfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/fifo", datadir);
    config.fifo = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/tmpfile", datadir);
    config.tmpfile = strdup(buf);

    if (stat(datadir, &st) == -1) {
	if (errno == ENOENT) {
	    if (mkdir(datadir, 0755) == -1)
		err(EXIT_FAILURE, "%s", datadir);
	}
	else
	    err(EXIT_FAILURE, "%s", datadir);

	stat(datadir, &st);
    }

    if (!S_ISDIR(st.st_mode))
	errx(EXIT_FAILURE, "%s: %s", datadir, E_NOTADIR);

    if (access(config.fifo, R_OK) == -1 && errno == ENOENT) {
	if (mkfifo(config.fifo, 0600) == -1)
	    err(EXIT_FAILURE, "%s", config.fifo);
    }

    set_defaults();

#ifdef DEBUG
    while ((opt = getopt(argc, argv, "EDNVShp:vu:e:f:i:")) != -1) {
#else
    while ((opt = getopt(argc, argv, "ENVShp:vu:e:f:i:")) != -1) {
#endif
	char *tmp;
	int i;

	switch (opt) {
	    case 'E':
		config.stoponerror = 1;
		break;
	    case 'N':
		noengine = 1;
		break;
	    case 'S':
		validate_and_save = 1;
	    case 'V':
		validate = 1;
		break;
#ifdef DEBUG
	    case 'D':
		debug = 1;
		break;
#endif
	    case 'u':
		i = 0;

		while ((tmp = strsep(&optarg, ":")) != NULL) {
		    switch (i++) {
			case 0:
			    config.ics_user = optarg;
			    break;
			case 1:
			    config.ics_passwd = optarg;
			    break;
			default:
			    usage(argv[0], EXIT_FAILURE);
		    }
		}
		break;
	    case 'i':
		i = 0;

		while ((tmp = strsep(&optarg, ":")) != NULL) {
		    switch (i++) {
			case 0:
			    strncpy(config.ics_server, tmp,
				    sizeof(config.ics_server));
			    break;
			case 1:
			    if (!isinteger(tmp))
				usage(argv[0], EXIT_FAILURE);

			    config.ics_port = atoi(tmp);
			    break;
			default:
			    usage(argv[0], EXIT_FAILURE);
		    }
		}
		break;
	    case 'v':
		printf("%s (%s)\n%s\n", PACKAGE_STRING, curses_version(), 
			COPYRIGHT);
		exit(EXIT_SUCCESS);
	    case 'p':
		filetype = PGN_FILE;
		strncpy(loadfile, optarg, sizeof(loadfile));
		break;
	    case 'f':
		filetype = FEN_FILE;
		strncpy(loadfile, optarg, sizeof(loadfile));
		break;
	    case 'e':
		filetype = EPD_FILE;
		strncpy(loadfile, optarg, sizeof(loadfile));
		break;
	    case 'h':
	    default:
		usage(argv[0], EXIT_SUCCESS);
	}
    }

    if ((validate || validate_and_save) && !*loadfile)
	usage(argv[0], EXIT_FAILURE);

    if (access(config.configfile, R_OK) == 0)
	parse_rcfile(config.configfile);

    signal(SIGPIPE, catch_signal);
    signal(SIGCONT, catch_signal);
    signal(SIGSTOP, catch_signal);
    signal(SIGINT, catch_signal);

    srandom(getpid());

    switch (filetype) {
	case PGN_FILE:
	    ret = parse_pgn_file(loadfile);
	    break;
	case FEN_FILE:
	    //ret = parse_fen_file(loadfile);
	    break;
	case EPD_FILE:
	case NO_FILE:
	default:
	    // No file specified. Empty game.
	    ret = parse_pgn_file(NULL);
	    break;
    }

    if (validate || validate_and_save) {
	if (validate_and_save) {
	    int i;

	    for (i = 0; i < gtotal; i++)
		pgn_dumpgame(stdout, &game[i], i, 0);
	}

	exit(ret);
    }

    if (initscr() == NULL)
	errx(EXIT_FAILURE, "%s", E_INITCURSES);
    else
	curses_initialized = 1;

    if (has_colors() == TRUE && start_color() == OK)
	init_color_pairs();

    boardw = newwin(BOARD_HEIGHT, BOARD_WIDTH, 0, COLS - BOARD_WIDTH);
    boardp = new_panel(boardw);
    historyw = newwin(HISTORY_HEIGHT, HISTORY_WIDTH, LINES - HISTORY_HEIGHT,
	    COLS - HISTORY_WIDTH);
    historyp = new_panel(historyw);
    statusw = newwin(STATUS_HEIGHT, STATUS_WIDTH, LINES - STATUS_HEIGHT, 0);
    statusp = new_panel(statusw);
    tagw = newwin(TAG_HEIGHT, TAG_WIDTH, 0, 0);
    tagp = new_panel(tagw);
    keypad(boardw, TRUE);
//  leaveok(boardw, TRUE);
    leaveok(tagw, TRUE);
    leaveok(statusw, TRUE);
    leaveok(historyw, TRUE);
    curs_set(0);
    cbreak();
    noecho();

    wbkgd(boardw, CP_BOARD_WINDOW);
    wbkgd(statusw, CP_STATUS_WINDOW);
    draw_window_title(statusw, STATUS_WINDOW_TITLE, STATUS_WIDTH,
	    CP_STATUS_TITLE, CP_STATUS_BORDER);
    wbkgd(tagw, CP_TAG_WINDOW);
    draw_window_title(tagw, TAG_WINDOW_TITLE, TAG_WIDTH, CP_TAG_TITLE, 
	    CP_TAG_BORDER);
    wbkgd(historyw, CP_HISTORY_WINDOW);
    draw_window_title(historyw, HISTORY_WINDOW_TITLE, HISTORY_WIDTH,
	    CP_HISTORY_TITLE, CP_HISTORY_BORDER);

    game_loop();
    stop_engine();

    endwin();
    free_all_games();
    del_panel(boardp);
    del_panel(historyp);
    del_panel(statusp);
    del_panel(tagp);
    delwin(boardw);
    delwin(historyw);
    delwin(statusw);
    delwin(tagw);
    exit(EXIT_SUCCESS);
}
