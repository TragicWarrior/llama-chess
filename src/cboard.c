/* $Id: cboard.c,v 1.63 2003-01-24 20:21:50 bjk Exp $ */
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

#include "common.h"
#include "colors.h"
#include "cboard.h"

char *random_agony()
{
    static int index;
    FILE *fp;
    char line[LINE_MAX];

    if (index == -1 || !config.agony || 
	    (browse_history && !config.historyagony))
	return NULL;

    if (!agony) {
	if ((fp = fopen(config.agonyfile, "r")) == NULL) {
	    index = -1;
	    message(ERROR, ANYKEY, "%s", E_AGONY);
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

	if (agony[0] == NULL || !index) {
	    index = -1;
	    return NULL;
	}
    }

    return agony[random() % index];
}

void draw_board(BOARD b, int crow, int ccol)
{
    int row, col;
    int bcol = 0, brow = 0;
    int maxy = BOARD_HEIGHT, maxx = BOARD_WIDTH;
    int ncols = 0, offset = 1;
    unsigned coords_y = 8;

    for (row = 0; row < maxy; row++) {
	bcol = 0;

	for (col = 0; col < maxx; col++) {
	    int attrs = CP_BOARD_WHITE;
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
			attrs = CP_BOARD_BLACK;

		    if (config.validmoves && b[brow][bcol].valid) {
			attrs = CP_BOARD_MOVES;

			if (b[brow][bcol].movecount) {
			    if (brow + 1 != crow && bcol + 1 != ccol)
				movecount = (b[brow][bcol].movecount + '0');
			}
		    }

		    if (row == ROWTOMATRIX(crow) && col == COLTOMATRIX(ccol)) {
			attrs = CP_BOARD_CURSOR;
		    }

		    if (row == ROWTOMATRIX(sp.row) && 
			    col == COLTOMATRIX(sp.col)) {
			attrs = CP_BOARD_SELECTED;
		    }

		    if (row == maxy - 1)
			attrs = 0;

		    mvwaddch(boardw, row, col, ' ' | attrs);

		    if (row == maxy - 1)
			waddch(boardw, x_grid_chars[bcol] | CP_BOARD_COORDS);
		    else {
			piece = b[row / 2][bcol].icon;

			if (attrs & A_BOLD)
			    bold = 1;

			if (status.side == WHITE && isupper(piece))
			    attrs |= A_BOLD;
			else if (status.side == BLACK && islower(piece))
			    attrs |= A_BOLD;

			waddch(boardw,
				(piece && piece != int_to_piece(OPEN_SQUARE)) ?
				piece | attrs : ' ' | attrs);

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

    return;
}

void copy_board(BOARD s, BOARD d)
{
    int row, col;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++)
	    d[row][col].icon = s[row][col].icon;
    }

    return;
}

/* Convert the selected piece to SAN format and validate it. */
static char *board_to_san(BOARD b)
{
    static char str[MAX_PGN_MOVE_LEN + 1], *p;
    int piece;
    int promo;
    BOARD t;

    snprintf(str, sizeof(str), "%c%i%c%i", x_grid_chars[sp.col - 1], sp.row,
	    x_grid_chars[sp.destcol - 1], sp.destrow);

    p = str;
    piece = piece_to_int(b[ROWTOBOARD(sp.row)][COLTOBOARD(sp.col)].icon);

    if (piece == PAWN && ((sp.destrow == 8 && status.turn == WHITE) ||
		    (sp.destrow == 1 && status.turn == BLACK))) {
	promo = message(PROMOTION_TITLE, PROMOTION_PROMPT, PROMOTION_TEXT);
	
	if (piece_to_int(promo) == -1)
	    return NULL;

	p = str + strlen(str);
	*p++ = toupper(promo);
	*p = '\0';
    }

    copy_board(b, t);

    if ((p = a2a4tosan(t, str)) == NULL) {
	message(p, ANYKEY, "%s", E_A2A4_PARSE);
	return NULL;
    }

    validate_move = 1;

    if (parse_move_text(t, p, 0)) {
	message(ERROR, ANYKEY, "%s: %s", E_INVALID_MOVE, p);
	validate_move = 0;
	return NULL;
    }

    validate_move = 0;
    return p;
}

static int move_to_engine(BOARD b)
{
    char *p;

    if ((p = board_to_san(b)) == NULL)
	return 0;

    SEND_TO_ENGINE("%s\n", p);
    sp.row = sp.col = 0;
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

void update_status()
{
    int i;
    char buf[STATUS_WIDTH - 7];

    snprintf(buf, sizeof(buf), "%i %s %i %s", gindex + 1, N_OF_N_STR, gtotal, 
	    (game[gindex].delete) ? GAME_NOTSAVED : "");
    mvwprintw(statusw, 2, 1, "%s %-*s", STATUS_GAME_STR, STATUS_WIDTH - 8, buf);

    switch (status.engine) {
	case ENGINE_THINKING:
	    strcpy(buf, ENGINE_THINKING_STR);
	    break;
	case ENGINE_READY:
	    strcpy(buf, ENGINE_READY_STR);

	    if (browse_history)
		strcat(buf, ENGINE_MOVE_HISTORY_STR);

	    break;
	case ENGINE_INITIALIZING:
	    strcpy(buf, ENGINE_INITIALIZING_STR);
	    break;
	case ENGINE_OFFLINE:
	    strcpy(buf, ENGINE_OFFLINE_STR);

	    if (browse_history)
		strcat(buf, ENGINE_MOVE_HISTORY_STR);

	    break;
	default:
	    strcpy(buf, UNKNOWN);
	    break;
    }

    mvwprintw(statusw, 3, 1, "%s %-*s", STATUS_ENGINE_STR, STATUS_WIDTH - 10, 
	    " ");
    wattron(statusw, CP_STATUS_ENGINE);
    mvwaddstr(statusw, 3, 9, buf);
    wattroff(statusw, CP_STATUS_ENGINE);

    mvwprintw(statusw, 4, 1, "%s %-*i", STATUS_DEPTH_STR, STATUS_WIDTH - 9,
	    config.engine_depth);

    mvwprintw(statusw, 5, 1, "%s %-*s", STATUS_BOOK_STR, STATUS_WIDTH - 8,
	    book_method(config.book_method));

    mvwprintw(statusw, 6, 1, "%s %-*s", STATUS_TURN_STR, STATUS_WIDTH - 8,
	    (status.turn == WHITE) ? WHITE_STR : BLACK_STR);

    for (i = 1; i < STATUS_WIDTH - 4; i++)
	mvwprintw(statusw, STATUS_HEIGHT - 2, i, " ");

    if (status.notify) {
	wattron(statusw, CP_STATUS_NOTIFY);
	mvwprintw(statusw, STATUS_HEIGHT - 2,
		CENTERX(STATUS_WIDTH, status.notify), "%s", status.notify);
	wattroff(statusw, CP_STATUS_NOTIFY);
    }
	
    return;
}

void update_history()
{
    char buf[HISTORY_WIDTH];
    struct history h = {{0},NULL,{0}};

    if (game[gindex].htotal)
	snprintf(buf, sizeof(buf), "%u %s %u", game[gindex].hindex, N_OF_N_STR,
		game[gindex].htotal);
    else
	strncpy(buf, UNAVAILABLE, sizeof(buf));

    mvwprintw(historyw, 2, 1, "%s %-*s", HISTORY_MOVE_STR, HISTORY_WIDTH - 13,
	    buf);

    get_history_by_index(game[gindex].hindex, &h);

    snprintf(buf, sizeof(buf), "%s %s", (h.move[0]) ? h.move : UNAVAILABLE,
	    ((h.comment && h.comment[0]) || h.nag[0]) ? HISTORY_ANNO_NEXT : "");
    mvwprintw(historyw, 3, 1, "%s %-*s", HISTORY_MOVE_NEXT_STR,
	    HISTORY_WIDTH - 13, buf);

    if (get_history_by_index(game[gindex].hindex - 1, &h))
	h.move[0] = 0;

    snprintf(buf, sizeof(buf), "%s %s", (h.move[0]) ? h.move : UNAVAILABLE,
	    ((h.comment && h.comment[0]) || h.nag[0]) ? HISTORY_ANNO_PREV : "");
    mvwprintw(historyw, 4, 1, "%s %-*s", HISTORY_MOVE_PREV_STR,
	    HISTORY_WIDTH - 13, buf);
    return;
}

void update_white_black()
{
    char *white, *black;

    if (game[gindex].tag[TAG_WHITE].value[0] == '?' || 
	    game[gindex].tag[TAG_WHITE].value[0] == '\0')
	white = UNAVAILABLE;
    else 
	white = game[gindex].tag[TAG_WHITE].value;

    if (game[gindex].tag[TAG_BLACK].value[0] == '?' || 
	    game[gindex].tag[TAG_BLACK].value[0] == '\0')
	black = UNAVAILABLE;
    else 
	black = game[gindex].tag[TAG_BLACK].value;

    mvwprintw(whitew, 2, 1, "%s %-*s", BW_NAME_STR, BW_WIDTH - 8, white);
    mvwprintw(blackw, 2, 1, "%s %-*s", BW_NAME_STR, BW_WIDTH - 8, black);

    mvwprintw(whitew, 3, 1, "%s %-2i", BW_CAPTURE_STR, game[gindex].wcaptures);
    mvwprintw(blackw, 3, 1, "%s %-2i", BW_CAPTURE_STR, game[gindex].bcaptures);
    return;
}

void draw_prompt(WINDOW *win, int y, int width, const char *str, chtype attr)
{
    int i;

    wattron(win, attr);

    for (i = 1; i < width - 1; i++)
	mvwaddch(win, y, i, ' ');

    mvwprintw(win, y, CENTERX(width, str), "%s", str);

    wattroff(win, attr);
    return;
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

    return;
}

void update_all()
{
    update_status();
    update_white_black();
    update_history();
    return;
}

static void init_active_game()
{
    init_history(board);

    if (gindex == gactive) {
	browse_history = 0;
	status.engine = ENGINE_READY;
    }

    update_all();
    return;
}

static void game_next_prev(int n, int count)
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

    init_active_game();
    return;
}

void free_game_data()
{
    int i;

    if (!gtotal)
	return;

    for (i = 0; i < gtotal; i++) {
	free_historydata(game[i].history, game[i].htotal);
	free(game[i].history);
	free_tag_data(game[i].tag, game[i].tindex);
	free(game[i].tag);
    }

    return;
}

static void delete_game(int which)
{
    struct games *g = NULL;
    int gi = 0;
    int i;
    
    for (i = 0; i < gtotal; i++) {
	if (i == which || game[i].delete) {
	    free_historydata(game[i].history, game[i].htotal);
	    free(game[i].history);
	    free_tag_data(game[i].tag, game[i].tindex);
	    free(game[i].tag);
	    continue;
	}

	g = Realloc(g, (gi + 2) * sizeof(struct games));

	memcpy(&g[gi], &game[i], sizeof(struct games));

	g[gi].tag = game[i].tag;
	g[gi].history = game[i].history;
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

    gactive = gindex;
    return;
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

void game_loop()
{
    int error_recover = 0;
    int pushkey = 0;
    int count = 0;
    int crow = 8, ccol = 1;

    gactive = gindex;
    gindex = gtotal - 1;

    if (pgnfile[0])
	init_history(board);

    flushinp();
    update_all();
    wtimeout(boardw, 70);

    while (!quit) {
	int c = 0;
	fd_set fds;
	int i, n = 0, len = 0;
	char fdbuf[8192] = {0};
	struct timeval tv;
	char *tmp;
	char buf[78];
	char tfile[FILENAME_MAX];
	struct tags *tmptag = NULL;
	int minr, maxr, minc, maxc;

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
			    message(ERROR, ANYKEY, "Attempt #%i. read(): %s",
				    ++error_recover, strerror(errno));
			    continue;
			}
		    }
		    else {
			if (len)
			    parse_engine_output(board, fdbuf);

			update_all();
		    }
		}

		for (i = 0; i < gtotal; i++) {
		    if (game[i].sockfd <= 0)
			continue;

		    if (FD_ISSET(game[i].sockfd, &fds)) {
			len = recv(game[i].sockfd, fdbuf, sizeof(fdbuf), 0);

			if (len == -1) {
			    if (errno != EAGAIN) {
				message(ERROR, ANYKEY, 
					"Attempt #%i. recv(): %s", 
					++error_recover, strerror(errno));
				continue;
			    }
			}
			else {
			    if (len)
				parse_ics_output(fdbuf);

			    update_all();
			}
		    }
		}
	    }
	    else {
		if (n == -1)
		    message(ERROR, ANYKEY, "select(): %s", strerror(errno));
		else {
		    /* timeout */
		}
	    }
	}

	error_recover = 0;
	draw_board(board, crow, ccol);
	wmove(boardw, ROWTOMATRIX(crow), COLTOMATRIX(ccol));
	update_panels();
	doupdate();

	if (pushkey)
	    c = pushkey;
	else {
	    if ((c = wgetch(boardw)) == ERR)
		continue;
	}

	switch (c) {
	    int annotate;

	    case ']':
	        view_annotation(game[gindex].hindex);
		break;
	    case '[':
	        view_annotation(game[gindex].hindex - 1);
		break;
	    case '>':
		game_next_prev(1, (count) ? count : 1);
		break;
	    case '<':
		game_next_prev(0, (count) ? count : 1);
		break;
	    case 'j':
		if (!browse_history || game[gindex].htotal < 2)
		    break;

		/*
		if ((tmp = get_input(GAME_HISTORY_JUMP_TITLE, NULL, 1, 1, 
				NULL, NULL, NULL, 0, FIELD_TYPE_INTEGER, 1, 0, 
				game[gindex].htotal)) == NULL)
		    break;
		*/

		if ((tmp = get_input(GAME_HISTORY_JUMP_TITLE, NULL, 1, 1, 
				NULL, NULL, NULL, 0, -1)) == NULL)
		    break;

		if (!isinteger(tmp))
		    break;

		if ((i = atoi(tmp)) > game[gindex].htotal || i < 0)
		    break;

		game[gindex].hindex = i;
		init_history(board);
		update_all();
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

		if ((tmp = get_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL, NULL, 
				NULL, 0, -1)) == NULL)
		    break;

		if (!isinteger(tmp))
		    break;

		if ((i = atoi(tmp) - 1) > gtotal - 1 || i < 0)
		    break;

		gindex = i;
		init_history(board);
		update_all();
		break;
	    case 'd':
		if (gtotal < 2)
		    break;

		if (game[gindex].delete == 1)
		    game[gindex].delete = 0;
		else
		    game[gindex].delete = 1;

		for (i = n = 0; i < gtotal; i++) {
		    if (game[i].delete)
			n++;
		}

		if (n == gtotal) {
		    message(NULL, ANYKEY, "%s", E_DELETE_GAME);
		    game[gindex].delete = 0;
		    break;
		}

		update_status();
		break;
	    case 'D':
		if (gtotal < 2) {
		    message(NULL, ANYKEY, "%s", E_DELETE_GAME);
		    break;
		}

		tmp = NULL;

		for (i = n = 0; i < gtotal; i++) {
		    if (game[i].delete)
			n++;
		}

		if (!n)
		    tmp = GAME_DELETE_GAME_TEXT;
		else {
		    if (n == gtotal) {
			message(NULL, ANYKEY, "%s", E_DELETE_GAME);
			break;
		    }

		    tmp = GAME_DELETE_ALL_TEXT;
		}

		if (config.deleteprompt) {
		    if ((c = message(NULL, YESNO, "%s", tmp)) != 'y')
			break;
		}

		delete_game((!n) ? gindex : -1);
		init_history(board);
		update_all();
		break;
	    case 'a':
	        annotate = game[gindex].hindex;

		if (annotate && game[gindex].history[annotate - 1].move[0])
		    annotate--;
		else
		    break;

		snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_EDIT_TITLE,
			game[gindex].history[annotate].move);

		tmp = get_input(buf, game[gindex].history[annotate].comment, 
			0, 0, NAG_PROMPT, history_edit_nag, (void *)annotate,
			CTRL('T'), -1);

		len = (tmp) ? strlen(tmp) + 1 : 1;

		game[gindex].history[annotate].comment = 
		    Realloc(game[gindex].history[annotate].comment, len);

		strncpy(game[gindex].history[annotate].comment,
			(tmp) ? tmp : "", len);

		update_history();
		break;
	    case 'i':
		edit_tags(0);
		break;
	    case 'g':
		if (browse_history || status.engine == ENGINE_THINKING)
		    break;

		SEND_TO_ENGINE("go\n");
		break;
	    case 'b':
		if (config.book_method == -1 || status.engine ==
			ENGINE_THINKING)
		    break;

		if (config.book_method + 1 >= BOOK_MAX)
		    config.book_method = 0;
		else
		    config.book_method++;

		SEND_TO_ENGINE("book %s\n", book_methods[config.book_method]);
		break;
	    case 'h':
		if (browse_history) {
		    if (game[gindex].openingside == BLACK) {
			message(NULL, ANYKEY, "%s", E_RESUME_BLACK);
			break;
		    }

		    if (!pushkey && 
			    game[gindex].hindex != game[gindex].htotal) {
			if ((c = message_uncentered(NULL, YESNO, "%s",
					GAME_RESUME_HISTORY_TEXT)) != 'y')
			    break;
		    }

		    if (!engine_initialized) {
			if (start_chess_engine() < 0)
			    break;

			pushkey = 'h';
			break;
		    }

		    pushkey = 0;
		    oldhistorytotal = game[gindex].htotal;
		    game[gindex].htotal = game[gindex].hindex;
		    gactive = gindex;
		    browse_history = 0;
		    status.engine = ENGINE_READY;

		    SEND_TO_ENGINE("\npgnload %s\n", config.fifo);
		    update_all();
		    break;
		}

		if (!game[gindex].htotal)
		    break;

		init_history(board);
		break;
	    case 'u':
		if (browse_history || !game[gindex].htotal)
		    break;

		history_previous(board, 2);
		game[gindex].htotal = game[gindex].hindex;

		SEND_TO_ENGINE("remove\n");
		update_history();
		break;
	    case 'r':
		if ((tmp = get_input(GAME_LOAD_TITLE, NULL, 1, 1,
				BROWSER_PROMPT, browse_directory, NULL, 
				'\t', -1)) == NULL)
		    break;

		if ((c = parse_pgn_file(board, tmp)) != 0) {
		    if (c > 0)
			message(NULL, ANYKEY, "%s: %s", tmp, strerror(errno));
		    else
			message(NULL, ANYKEY, "%s: %s", tmp, E_PGN_PARSE);

		    break;
		}

		gindex = gtotal - 1;
		gactive = gindex;
		strncpy(pgnfile, tmp, sizeof(pgnfile));
		init_history(board);
		update_all();
		break;
	    case 'S':
	    case 's':
		for (i = n = 0; i < gtotal; i++) {
		    if (!game[i].htotal)
			continue;

		    n = 1;
		}

		if (!n) {
		    message(NULL, ANYKEY, "%s", E_SAVE_NOGMOVES);
		    break;
		}

		oldhistorytotal = game[gindex].htotal;

		if (browse_history && game[gindex].hindex != 
			game[gindex].htotal && (c == 'S' || config.saveprompt))
		{
		    c = message_uncentered(NULL, GAME_SAVE_HISTORY_PROMPT,
			    "%s", GAME_SAVE_HISTORY_TEXT);

		    if (c == 'c')
			game[gindex].htotal = game[gindex].hindex;
		    else if (c == 'a');
		    else
			break;
		}

		if ((c == 'S' || config.saveprompt) && 
			message(NULL, YESNO, "%s", GAME_EDIT_TAG_PROMPT) 
			== 'y') {
		    if ((tmptag = edit_tags(1)) == NULL) {
			game[gindex].htotal = oldhistorytotal;
			break;
		    }

		    game[gindex].tindex = 0;

		    for (i = 0; tmptag[i].name; i++)
			add_tag(&game[gindex].tag, &game[gindex].tindex,
				tmptag[i].name, tmptag[i].value);

		    free_tag_data(tmptag, i);
		    free(tmptag);
		}

		if ((tmp = get_input(GAME_SAVE_TITLE, pgnfile, 1, 1,
				BROWSER_PROMPT, browse_directory, NULL, 
				'\t', -1)) == NULL) {
		    game[gindex].htotal = oldhistorytotal;
		    break;
		}

		if (strstr(tmp, ".") == NULL && compression_cmd(tmp, 0)
			== NULL) {
		    snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
		    tmp = tfile;
		}

		if (save_pgn(tmp, 0)) {
		    game[gindex].htotal = oldhistorytotal;
		    break;
		}

		game[gindex].htotal = oldhistorytotal;
		parse_pgn_file(board, pgnfile);
		gindex = gtotal - 1;
		gactive = gindex;
		status.notify = NOTIFY_SAVED;
		update_all();
		break;
	    case CTRL('G'):
		help(GAME_HELP, mainhelp);
		break;
	    case 'n':
	    case 'N':
		if (c == 'N') {
		    if (message(NULL, YESNO, "%s", GAME_NEW_PROMPT) != 'y')
			break;
		}

		browse_history = sp.icon = 0;

		if (c == 'n') {
		    newgameinit = 1;
		    new_game(board);
		    gactive = gindex;
		}
		else {
		    reset_history();
		    pgnfile[0] = '\0';
		    parse_pgn_file(board, pgnfile);
		}

		game[gindex].wcaptures = game[gindex].bcaptures = 0;

		if (status.engine == ENGINE_OFFLINE ||
			engine_initialized == 0) {
		    if (start_chess_engine() < 0)
			break;
		}

		SEND_TO_ENGINE("\nnew\n");
		set_engine_defaults();
		status.engine = ENGINE_READY;
		status.notify = NULL;
		update_all();
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
		sp.icon = sp.row = sp.col = 0;

		if (config.validmoves)
		    reset_valid_moves(board);
		break;
	    case '0' ... '9':
		if (c == '0')
		    count = 10;
		else
		    count = c - '0';

		continue;
	    case KEY_UP:
		if (browse_history) {
		    history_next(board, (count > 0) ?
			    config.jumpcount * count : config.jumpcount);
		    update_all();
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
		if (browse_history) {
		    history_previous(board, (count) ?
			    config.jumpcount * count : config.jumpcount);
		    update_all();
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
		}
		else
		    crow--;

		if (crow < 1)
		    crow = 8;

		break;
	    case KEY_LEFT:
		if (browse_history) {
		    history_previous(board, (count) ? count : 1);
		    update_all();
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
		if (browse_history) {
		    history_next(board, (count) ? count : 1);
		    update_all();
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
		if (browse_history)
		    break;

		if (status.turn == BLACK && status.side == WHITE) {
		    status.side = BLACK;
		    break;
		}
		else if (status.turn == WHITE && status.side == BLACK) {
		    status.side = WHITE;
		    break;
		}

		SEND_TO_ENGINE("\nswitch\n");
		break;
	    case ' ':
		if (browse_history)
		    break;

		if (status.engine != ENGINE_READY || !engine_initialized) {
		    if (start_chess_engine() < 0) {
			sp.icon = 0;
			break;
		    }
		}

		if (sp.icon || status.engine == ENGINE_THINKING) {
		    beep();
		    break;
		}

		sp.icon = mvwinch(boardw, ROWTOMATRIX(crow), 
			COLTOMATRIX(ccol)+1) & A_CHARTEXT;

		if (sp.icon == ' ') {
		    sp.icon = 0;
		    break;
		}

		if ((islower(sp.icon) && status.turn != BLACK) ||
			(isupper(sp.icon) && status.turn != WHITE)) {
		    message_uncentered(NULL, ANYKEY, "%s", E_SELECT_TURN);
		    sp.icon = 0;
		    break;
		}

		sp.row = crow;
		sp.col = ccol;

		if (config.validmoves) {
		    get_valid_moves(board, piece_to_int(sp.icon), sp.row, 
			    sp.col, &minr, &maxr, &minc, &maxc);
		    /*
		    number_valid_moves(board, sp.row, sp.col);
		    */
		}

		break;
	    case '\015':
	    case '\n':
		pushkey = 0;

		if (browse_history)
		    break;

		if (status.engine == ENGINE_THINKING) {
		    beep();
		    break;
		}

		if (!sp.icon)
		    break;

		sp.destrow = crow;
		sp.destcol = ccol;

		if (move_to_engine(board)) {
		    if (config.validmoves)
			reset_valid_moves(board);
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

    return;
}

void usage(const char *pn)
{
    printf("Usage: %s [-hv] [-p <pgnfile>] [-i hostname[:port]] "
	    "[-u username[:passwd]]\n", pn);
    printf("  -p  Load PGN file.\n");
    printf("  -i  ICS hostname and optional port.\n");
    printf("  -u  ICS username and optional password.\n");
    printf("  -v  Version information.\n");
    printf("  -h  This help text.\n");

    exit(EXIT_FAILURE);
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

	    message(NULL, ANYKEY, "%s", E_BROKEN_PIPE);
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

    return;
}

static void copydatafile(const char *dst, const char *src)
{
    FILE *fp, *ofp;
    char buf[LINE_MAX], *s;

    snprintf(buf, sizeof(buf), "%s/%s", DATA_PATH, src);

    fprintf(stderr, "%s %s...\n", COPY_DATAFILE, buf);

    if ((fp = fopen(buf, "r")) == NULL) {
	warn("%s", buf);
	return;
    }

    if ((ofp = fopen(dst, "w+")) == NULL) {
	fclose(fp);
	warn("%s", dst);
	return;
    }

    while ((s = fgets(buf, sizeof(buf), fp)) != NULL)
	fprintf(ofp, "%s", s);

    fclose(fp);
    fclose(ofp);
    return;
}

static void set_defaults()
{
    struct stat st;

    status.engine = ENGINE_OFFLINE;

    config.jumpcount = 5;
    config.clevel = 6;
    config.book_method = BOOK_RANDOM;
    config.engine_depth = 0;
    config.historyagony = 0;
    config.agony = 1;
    config.linegraphics = 0;
    config.saveprompt = 1;
    config.deleteprompt = 1;
    config.validmoves = 1;
    strncpy(config.ics_server, DEFAULT_ICS_SERVER, sizeof(config.ics_server));
    config.ics_port = DEFAULT_ICS_PORT;
    config.ics_user = strdup(DEFAULT_ICS_USER);

    set_default_colors();

    if (stat(config.nagfile, &st) == -1 && errno == ENOENT)
	copydatafile(config.nagfile, "nag.data");

    if (stat(config.agonyfile, &st) == -1 && errno == ENOENT)
	copydatafile(config.agonyfile, "agony.data");

    if (stat(config.ccfile, &st) == -1 && errno == ENOENT)
	copydatafile(config.ccfile, "cc.data");

    return;
}

int main(int argc, char *argv[])
{
    int opt;
    struct passwd *pwd;
    struct stat st;
    char buf[FILENAME_MAX];
    char datadir[FILENAME_MAX];

    while ((opt = getopt(argc, argv, "hp:vu:i:")) != -1) {
	char *tmp;
	int i;

	switch (opt) {
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
			    usage(argv[0]);
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
				usage(argv[0]);

			    config.ics_port = atoi(tmp);
			    break;
			default:
			    usage(argv[0]);
		    }
		}
		break;
	    case 'v':
		printf("%s\n%s\n", PACKAGE_STRING, COPYRIGHT);
		exit(EXIT_SUCCESS);
	    case 'p':
		strncpy(pgnfile, optarg, sizeof(pgnfile));
		break;
	    case 'h':
	    default:
		usage(argv[0]);
	}
    }

    if ((pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    snprintf(datadir, sizeof(datadir), "%s/.cboard", pwd->pw_dir);
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

    if (access(config.fifo, R_OK) == -1) {
	if (mkfifo(config.fifo, 0600) == -1)
	    err(EXIT_FAILURE, "%s", config.fifo);
    }

    set_defaults();

    if (access(config.configfile, R_OK) == 0)
	parse_rcfile(config.configfile);

    signal(SIGPIPE, catch_signal);
    signal(SIGCONT, catch_signal);
    signal(SIGSTOP, catch_signal);
    signal(SIGINT, catch_signal);

    if ((opt = parse_pgn_file(board, pgnfile)) != 0) {
	if (opt > 0)
	    err(EXIT_FAILURE, "%s", pgnfile);
	else
	    errx(EXIT_FAILURE, "%s: %s", pgnfile, E_PGN_PARSE);
    }

    srandom(getpid());

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
    whitew = newwin(BW_HEIGHT, BW_WIDTH, 0, 0);
    whitep = new_panel(whitew);
    blackw = newwin(BW_HEIGHT, BW_WIDTH, BW_HEIGHT, 0);
    blackp = new_panel(blackw);
    keypad(boardw, TRUE);
    leaveok(boardw, TRUE);
    leaveok(whitew, TRUE);
    leaveok(blackw, TRUE);
    leaveok(statusw, TRUE);
    leaveok(historyw, TRUE);
    curs_set(0);
    cbreak();
    noecho();

    wbkgd(statusw, CP_STATUS_WINDOW);
    draw_window_title(statusw, STATUS_WINDOW_TITLE, STATUS_WIDTH,
	    CP_STATUS_TITLE, CP_STATUS_BORDER);
    wbkgd(whitew, CP_WHITE_WINDOW);
    draw_window_title(whitew, WHITE_WINDOW_TITLE, BW_WIDTH, CP_WHITE_TITLE, 
	    CP_WHITE_BORDER);
    wbkgd(blackw, CP_BLACK_WINDOW);
    draw_window_title(blackw, BLACK_WINDOW_TITLE, BW_WIDTH, CP_BLACK_TITLE, 
	    CP_BLACK_BORDER);
    wbkgd(historyw, CP_HISTORY_WINDOW);
    draw_window_title(historyw, HISTORY_WINDOW_TITLE, HISTORY_WIDTH,
	    CP_HISTORY_TITLE, CP_HISTORY_BORDER);

    game_loop();
    stop_engine();

    endwin();
    free_game_data();
    free(game);
    del_panel(boardp);
    del_panel(historyp);
    del_panel(statusp);
    del_panel(whitep);
    del_panel(blackp);
    delwin(boardw);
    delwin(historyw);
    delwin(statusw);
    delwin(whitew);
    delwin(blackw);
    exit(EXIT_SUCCESS);
}
