/* $Id: cboard.c,v 1.22 2002-12-12 19:21:14 bjk Exp $ */
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
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/time.h>
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
#include "cboard.h"

void draw_board()
{
    int row, col;
    int rcol = 0;
    int maxy = BOARD_HEIGHT, maxx = BOARD_WIDTH;
    int ncols = 0, offset = 1;
    unsigned coords_y = 8;

    for (row = 0; row < maxy; row++) {
	rcol = 0;

	for (col = 0; col < maxx; col++) {
	    int attrs = BOARD_WHITE;

	    if (row == 0 || row == maxy - 2) {
		if (col == 0)
		    mvwaddch(boardw, row, col, 
			    (row) ? ACS_LLCORNER : ACS_ULCORNER);
		else if (col == maxx - 2)
		    mvwaddch(boardw, row, col,
			    (row) ? ACS_LRCORNER : ACS_URCORNER);
		else if (!(col % 4))
		    mvwaddch(boardw, row, col, 
			    (row) ? ACS_BTEE : ACS_TTEE);
		else {
		    if (col != maxx - 1)
			mvwaddch(boardw, row, col, ACS_HLINE);
		}

		continue;
	    }

	    if ((row % 2) && col == maxx - 1 && coords_y) {
		mvwprintw(boardw, row, col, "%d", coords_y--);
		continue;
	    }

	    if ((col == 0 || col == maxx - 2) && row != maxy - 1) {
		if (!(row % 2))
		    mvwaddch(boardw, row, col,
			    (col) ? ACS_RTEE : ACS_LTEE);
		else
		    mvwaddch(boardw, row, col, ACS_VLINE);

		continue;
	    }

	    if ((row % 2) && !(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col, ACS_VLINE);
		continue;
	    }

	    if (!(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col, ACS_PLUS);
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
			attrs = BOARD_BLACK;

		    if (row == cursor_y && col == (cursor_x - 1)) {
			attrs = BOARD_CURSOR;
		    }

		    if (row == selected_y && col == (selected_x - 1)) {
			attrs = BOARD_SELECTED;
		    }

		    if (row == maxy - 1)
			attrs = 0;

		    wattron(boardw, attrs);
		    mvwaddch(boardw, row, col, ' ');

		    if (row == maxy - 1)
			waddch(boardw, x_grid_chars[rcol] | attrs);
		    else {
			if (status.bw == WHITE &&
				isupper(board[row / 2][rcol].icon))
			    attrs |= A_BOLD;
			else if (status.bw == BLACK &&
				islower(board[row / 2][rcol].icon))
			    attrs |= A_BOLD;

			waddch(boardw, (board[row / 2][rcol].icon) ?
				board[row / 2][rcol].icon | attrs :
				' ' | attrs);
		    }

		    waddch(boardw, ' ');
		    wattroff(boardw, attrs);
		    col += 2;
		    rcol++;
		}
	    }
	    else {
		if (col != maxx - 1)
		    mvwaddch(boardw, row, col, ACS_HLINE);
	    }
	}
    }

    return;
}

void parse_piece_command(int dest_y, int dest_x)
{
    char str[MAX_MOVE_LEN] = {0};
    char buf[MAX_MOVE_LEN] = {0};

    switch (selected_piece.icon) {
	case 'p':
	case 'P':
	    snprintf(str, sizeof(str), "%c%i",
		    x_grid_chars[selected_piece.col - 1], selected_piece.row);
	    break;
	default:
	    snprintf(str, sizeof(str), "%c%i", toupper(selected_piece.icon),
		    selected_piece.row);
	    break;
    }

    snprintf(buf, sizeof(buf), "%c%i", x_grid_chars[dest_x - 1], dest_y);
    strncat(str, buf, sizeof(str));
    send_to_engine("%s\n", str);
    selected_piece.icon = selected_x = selected_y = 0;
    return;
}

void update_status()
{
    int w = STATUS_WIDTH - 10;
    int i;
    char *book, *engine;
    char buf[w];

    switch (status.engine) {
	case ENGINE_THINKING:
	    engine = "thinking ...";
	    break;
	case ENGINE_READY:
	    engine = "ready";
	    break;
	case HISTORY_MODE:
	    engine = "ready (move history)";
	    break;
	default:
	    engine = UNKNOWN;
	    break;
    }

    mvwprintw(statusw, 2, 1, "Engine: %-*s", w, " ");
    wattron(statusw, ENGINE_STATUS);
    mvwaddstr(statusw, 2, 9, engine);
    wattroff(statusw, ENGINE_STATUS);

    switch (status.book_method) {
	case -1:
	    book = UNKNOWN;
	    break;
	case BOOK_BEST:
	    book = "best";
	    break;
	case BOOK_WORST:
	    book = "worst";
	    break;
	case BOOK_PREFER:
	    book = "prefer";
	    break;
	case BOOK_RANDOM:
	    book = "random";
	    break;
	default:
	    book = "disabled";
	    break;
    }

    mvwprintw(statusw, 3, 1, "  Book: %-*s", w, book);

    mvwprintw(statusw, 4, 1, "  Turn: %-*s", w, 
	    (status.turn == WHITE) ? "white" : "black");

    snprintf(buf, sizeof(buf), "%i of %i", gindex + 1, gtotal);
    mvwprintw(statusw, 5, 1, "  Game: %-*s", w, buf);

    for (i = 1; i < STATUS_WIDTH - 4; i++)
	mvwprintw(statusw, STATUS_HEIGHT - 2, i, " ");

    if (status.notify) {
	wattron(statusw, NOTIFY_STATUS);
	mvwprintw(statusw, STATUS_HEIGHT - 2,
		CENTERX(STATUS_WIDTH, status.notify), "%s", status.notify);
	wattroff(statusw, NOTIFY_STATUS);
    }
	
    return;
}

void update_history()
{
    char buf[16];

    if (game[gindex].htotal)
	snprintf(buf, sizeof(buf), "%u%s of %u", game[gindex].hindex,
		(game[gindex].history[game[gindex].hindex].comment[0]) ?
		"*" : "", game[gindex].htotal);
    else
	strncpy(buf, UNKNOWN, sizeof(buf));

    mvwprintw(historyw, 2, 1, "     Move: %-*s", HISTORY_WIDTH - 13, buf);
    mvwprintw(historyw, 3, 1, "Next move: %-*s", HISTORY_WIDTH - 13, 
	    get_history_by_index(game[gindex].hindex));
    mvwprintw(historyw, 4, 1, "Last move: %-*s", HISTORY_WIDTH - 13,
	    get_history_by_index(game[gindex].hindex - 1));
    return;
}

void update_data()
{
    int w;
    char *tmp;
    int i, tlen = 0;
    int n;

    /* Get the longest tag length and clear the initial lines. */
    for (i = 0; (i < DATA_HEIGHT - 6 && game[gindex].pgn[i].token[0]); i++) {
	int ttlen = strlen(game[gindex].pgn[i].token);

	if (tlen < ttlen)
	    tlen = ttlen;

	for (n = 1; n < DATA_WIDTH - 1; n++)
	    mvwprintw(dataw, i + 3, n, " ");
    }

    w = DATA_WIDTH - tlen - 4;

    if ((tmp = real_filename(pgnfile)) == NULL)
	tmp = "none";

    mvwprintw(dataw, 2, 1, "%*s: %-*s", tlen, "File", w, tmp);

    for (i = 0; (i < DATA_HEIGHT - 6 && game[gindex].pgn[i].token[0]); i++) {
	char buf[w + 1];

	if (strlen(game[gindex].pgn[i].value) > w)
	    snprintf(buf, sizeof(buf), "%-.*s...", w - 3, 
		    game[gindex].pgn[i].value);
	else
	    snprintf(buf, sizeof(buf), "%-.*s", w, game[gindex].pgn[i].value);

	mvwprintw(dataw, i + 3, 1, "%*s: %-.*s", tlen,
		game[gindex].pgn[i].token, w, buf);
    }

    for (; i < DATA_HEIGHT - 4; i++)
	mvwprintw(dataw, i + 3, 1, "%*s", DATA_WIDTH - 4, " ");

    mvwprintw(dataw, DATA_HEIGHT - 2, CENTERX(DATA_WIDTH, MAIN_HELP_PROMPT),
	    "%s", MAIN_HELP_PROMPT);

    return;
}

void draw_window_title(WINDOW *win, const char *title, int width)
{
    int i;

    wattron(win, WINDOW_TITLE);

    for (i = 1; i < width - 1; i++)
	mvwprintw(win, 1, i, "%c", ' ');

    if (title)
	mvwprintw(win, 1, CENTERX(width, title), "%s", title);

    wattroff(win, WINDOW_TITLE);
    wattron(win, WINDOW_BORDER);
    box(win, ACS_VLINE, ACS_HLINE);
    wattroff(win, WINDOW_BORDER);

    return;
}

void update_all()
{
    update_status();
    update_data();
    update_history();
    return;
}

void refresh_all()
{
    werase(statusw);
    werase(historyw);
    werase(dataw);
    werase(boardw);
    update_all();
    draw_window_title(statusw, STATUS_TITLE, STATUS_WIDTH);
    draw_window_title(dataw, DATA_TITLE, DATA_WIDTH);
    draw_window_title(historyw, HISTORY_TITLE, HISTORY_WIDTH);
    update_panels();
    doupdate();
    return;
}

void game_loop()
{
    int rrow = 8, rcol = 1;

    cursor_x = 2, cursor_y = 1;
    gindex = gtotal - 1;

    wtimeout(boardw, 500);
    send_to_engine("nopost\n");

    if (pgnfile[0])
	send_to_engine("pgnload %s\n", pgnfile);
    else
	send_to_engine("show board\n");

    flushinp();

    while (!quit) {
	int c = 0;
	fd_set fds;
	int n, len;
	char enginebuf[8192] = {0};
	struct timeval tv;
	char *tmp;
	char buf[78];
	struct pgndata *tmppgn = NULL;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(from_engine, &fds);

	if ((n = select(from_engine + 1, &fds, NULL, NULL, &tv)) > 0) {
	    if (FD_ISSET(from_engine, &fds)) {
		if ((len = read(from_engine, enginebuf, sizeof(enginebuf))) 
			> 0) {
		    parse_engine_output(enginebuf);
		    update_all();
		}
		else
		    message(ERROR, ANYKEY, "read() error from engine");
	    }

	}
	else {
	    /* timeout */
	}

	draw_board();
	wmove(boardw, cursor_y, cursor_x);
	update_panels();
	doupdate();

	if ((c = wgetch(boardw)) == ERR)
	    continue;

	switch (c) {
	    int annotate;

	    case '>':
		if (gindex + 1 == gtotal)
		    gindex = 0;
		else
		    gindex++;

		update_all();
		break;
	    case '<':
		if (gindex - 1 < 0)
		    gindex = gtotal - 1;
		else
		    gindex--;

		update_all();
		break;
	    case 'a':
	        annotate = game[gindex].hindex;

		if (annotate && game[gindex].history[annotate - 1].move[0])
		    annotate--;
		else
		    break;

		snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATE_HISTORY,
			game[gindex].history[annotate].move);

		if ((tmp = get_input(buf, 
				game[gindex].history[annotate].comment, 0, 0, 
				-1)) != NULL)
		    strncpy(game[gindex].history[annotate].comment, tmp,
			    sizeof(game[gindex].history[annotate].comment));
		update_history();
		break;
	    case 'i':
		if (!pgnfile[0])
		    break;

		edit_pgn_data(0);
		break;
	    case 'g':
		if (browse_history || status.engine == ENGINE_THINKING)
		    break;

		send_to_engine("go\n");
		break;
	    case 'b':
		if (status.book_method == -1 || status.engine ==
			ENGINE_THINKING)
		    break;

		if (status.book_method + 1 >= BOOK_MAX)
		    status.book_method = 0;
		else
		    status.book_method++;

		send_to_engine("book %s\n", book_methods[status.book_method]);
		break;
	    case 'h':
		if (browse_history) {
		    if (game[gindex].hindex != game[gindex].htotal) {
			if ((c = message(NULL, YESNO, 
					"Resume game from history?")) != 'y')
			    break;

			game[gindex].htotal = game[gindex].hindex;
			update_history();
		    }

		    browse_history = 0;

		    if (status.bw != status.turn) {
			send_to_engine("go\n");
			break;
		    }
		    
		    status.engine = ENGINE_READY;
		    update_status();
		    cancel_manual_mode = 1;
		    break;
		}

		send_to_engine("manual\n");
		browse_history = 1;
		status.engine = HISTORY_MODE;
		update_status();
		break;
	    case 'u':
		if (browse_history)
		    break;

		/* FIXME history */
		send_to_engine("remove\n");
		break;
	    case 'r':
		if ((tmp = get_input_str("Load saved game filename", NULL)) 
			== NULL)
		    break;

		if ((c = parse_pgn_file(tmp)) != 0) {
		    if (c > 0)
			message(NULL, ANYKEY, "%s: %s", tmp, strerror(errno));
		    else
			message(NULL, ANYKEY, "%s: parse error", tmp);

		    break;
		}

		gindex = gtotal - 1;
		strncpy(pgnfile, tmp, sizeof(pgnfile));
		send_to_engine("pgnload %s\n", pgnfile);
		break;
	    case 's':
		if (!game[gindex].htotal) {
		    message(NULL, ANYKEY, "No moves to save");
		    break;
		}

		if (message(NULL, YESNO, "Edit save game data?") == 'y') {
		    if ((tmppgn = edit_pgn_data(1)) == NULL)
			break;
		}

		if ((tmp = get_input_str_clear("Save game filename", 
				pgnfile)) == NULL) {
		    if (tmppgn)
			free(tmppgn);
		    break;
		}

		if (save_pgn(tmp, (tmppgn) ? tmppgn : game[gindex].pgn)) {
		    if (tmppgn)
			free(tmppgn);

		    message(NULL, ANYKEY, "%s: %s", tmp, strerror(errno));
		    break;
		}

		free(tmppgn);
		strncpy(pgnfile, tmp, sizeof(pgnfile));
		parse_pgn_file(pgnfile);
		gindex = gtotal - 1;
		update_data();
		update_status();
		break;
	    case CTRL('G'):
		help(MAIN_HELP, mainhelp);
		break;
	    case 'N':
		if (message(NULL, YESNO, "Really start a new game?") != 'y')
		    break;

		reset_history();
		browse_history = 0;

		if (pgnfile[0]) {
		    pgnfile[0] = 0;
		    parse_pgn_file(pgnfile);
		}

		update_all();

		status.bw = WHITE;
		send_to_engine("new\n");
		send_to_engine("show board\n");
		break;
	    case 'R':
		refresh_all();
		break;
	    case 'c':
		if (status.engine == ENGINE_THINKING)
		    break;

		if ((tmp = get_input_str_clear(ENGINE_COMMAND_PROMPT, NULL)) 
			!= NULL) {
		    send_to_engine("%s\n", tmp);
		}
		break;
	    case KEY_ESCAPE:
		selected_piece.icon = selected_y = selected_x = 0;
		break;
	    case 'j':
	    case KEY_UP:
		if (browse_history)
		    break;

		if (cursor_y - 2 < 1)
		    cursor_y = BOARD_HEIGHT - 3, rrow = 1;
		else
		    cursor_y -= 2, rrow++;
		break;
	    case 'k':
	    case KEY_DOWN:
		if (browse_history)
		    break;

		if (cursor_y + 2 > BOARD_HEIGHT - 2)
		    cursor_y = 1, rrow = 8;
		else
		    cursor_y += 2, rrow--;
		break;
	    case 'l':
	    case KEY_LEFT:
		if (browse_history) {
		    if (game[gindex].hindex - 1 < 0)
			break;

		    send_to_engine("undo\n");
		    game[gindex].hindex--;
		    break;
		}

		if (cursor_x - 4 < 2)
		    cursor_x = BOARD_WIDTH - 4, rcol = 8;
		else
		    cursor_x -= 4, rcol--;
		break;
	    case ';':
	    case KEY_RIGHT:
		if (browse_history) {
		    if (game[gindex].hindex + 1 > game[gindex].htotal)
			break;

		    send_to_engine("%s\n", 
			    game[gindex].history[game[gindex].hindex++].move);
		    break;
		}

		if (cursor_x + 4 > BOARD_WIDTH - 4)
		    cursor_x = 2, rcol = 1;
		else
		    cursor_x += 4, rcol++;
		break;
	    case 'w':
		if (browse_history)
		    break;

		send_to_engine("switch\n");
		break;
	    case ' ':
		if (browse_history)
		    break;

		if (selected_piece.icon) {
		    beep();
		    break;
		}

		if ((selected_piece.icon = winch(boardw) & A_CHARTEXT) == ' ') {
		    selected_piece.icon = 0;
		    break;
		}

		if ((islower(selected_piece.icon) && status.turn != BLACK) ||
			(isupper(selected_piece.icon) && status.turn != WHITE)) {
		    selected_piece.icon = 0;
		    break;
		}

		selected_piece.row = rrow;
		selected_piece.col = rcol;
		selected_x = cursor_x;
		selected_y = cursor_y;
		break;
	    case '\n':
		if (browse_history)
		    break;

		if (status.engine == ENGINE_THINKING) {
		    beep();
		    break;
		}

		if (!selected_piece.icon)
		    break;

		parse_piece_command(rrow, rcol);
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
    }

    return;
}

void usage(const char *pn)
{
    printf("Usage: %s [-hv] [-p <pgnfile>]\n", pn);
    printf("  -p  Load PGN file.\n");
    printf("  -v  Version information.\n");
    printf("  -h  This help text.\n");

    exit(EXIT_FAILURE);
}

void catch_signal(int which)
{
    switch (which) {
	case SIGPIPE:
	    if (quit)
		break;

	    message(NULL, ANYKEY, "Broken pipe. Quitting.");
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

int main(int argc, char *argv[])
{
    int opt;
    int i;

    /* FIX THIS STUPID THING */
    printf("GNUChess is modified in main() to setlinebuf(). This fixes the "
	    "PIPE_BUF problem.\n");

    while ((opt = getopt(argc, argv, "hp:v")) != -1) {
	switch (opt) {
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

    signal(SIGPIPE, catch_signal);
    signal(SIGCONT, catch_signal);
    signal(SIGSTOP, catch_signal);

    if ((opt = parse_pgn_file(pgnfile)) != 0) {
	if (opt > 0)
	    err(EXIT_FAILURE, "%s", pgnfile);
	else
	    errx(EXIT_FAILURE, "%s: parse error", pgnfile);
    }

    initscr();
    init_chess_engine();

    if (has_colors() == TRUE && start_color() == OK) {
	init_pair(1, COLOR_WHITE, COLOR_RED);
	init_pair(2, COLOR_WHITE, COLOR_BLACK);
	init_pair(3, COLOR_WHITE, COLOR_YELLOW);
	init_pair(4, COLOR_WHITE, COLOR_GREEN);
	init_pair(5, COLOR_WHITE, COLOR_BLUE);
	init_pair(6, COLOR_YELLOW, COLOR_BLACK);
	init_pair(7, COLOR_RED, COLOR_BLACK);
	init_pair(8, COLOR_WHITE, COLOR_GREEN);
	init_pair(9, COLOR_CYAN, COLOR_BLACK);
    }

    boardw = newwin(BOARD_HEIGHT, BOARD_WIDTH, 0, COLS - BOARD_WIDTH);
    boardp = new_panel(boardw);
    historyw = newwin(HISTORY_HEIGHT, HISTORY_WIDTH, LINES - HISTORY_HEIGHT,
	    COLS - HISTORY_WIDTH);
    historyp = new_panel(historyw);
    statusw = newwin(STATUS_HEIGHT, STATUS_WIDTH, LINES - STATUS_HEIGHT, 0);
    statusp = new_panel(statusw);
    dataw = newwin(DATA_HEIGHT, DATA_WIDTH, 0, 0);
    datap = new_panel(dataw);
    keypad(boardw, TRUE);
    curs_set(0);
    cbreak();
    noecho();

    draw_window_title(dataw, DATA_TITLE, DATA_WIDTH);
    draw_window_title(statusw, STATUS_TITLE, STATUS_WIDTH);
    draw_window_title(historyw, HISTORY_TITLE, HISTORY_WIDTH);

    game_loop();
    send_to_engine("quit\n");
    endwin();
    del_panel(datap);
    del_panel(boardp);
    del_panel(statusp);
    del_panel(historyp);
    delwin(dataw);
    delwin(boardw);
    delwin(statusw);
    delwin(historyw);

    for (i = 0; i < gtotal; i++) {
	free(game[i].pgn);
	free(game[i].history);
    }

    free(game);
    exit(EXIT_SUCCESS);
}
