/* $Id: cboard.c,v 1.10 2002-12-06 20:38:12 bjk Exp $ */
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
    const char *coords_y = "87654321";
    const char *coords_x = x_grid_chars;
    int yindex = 0, xindex = 0;

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

	    if ((row % 2) && col == maxx - 1) {
		mvwprintw(boardw, row, col, "%c", coords_y[yindex++]);
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
			waddch(boardw, coords_x[xindex++] | attrs);
		    else {
			waddch(boardw, board[row / 2][rcol].icon | attrs);
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
	    snprintf(str, sizeof(str), "%c%i", selected_piece.icon,
		    selected_piece.row);
	    break;
    }

    snprintf(buf, sizeof(buf), "%c%i", x_grid_chars[dest_x - 1], dest_y);
    strncat(str, buf, sizeof(str));
    send_to_engine("%s\n", str);
    selected_piece.icon = selected_x = selected_y = 0;
    return;
}

char *last_history(int index)
{
    if (index < 0 || index > history_total - 1)
	return "none";

    return history[index].move;
}

void update_history()
{
    char buf[16];

    if (history_total)
	snprintf(buf, sizeof(buf), "%u of %u", history_index,
		history_total);
    else
	strncpy(buf, UNKNOWN, sizeof(buf));

    mvwprintw(historyw, 2, 1, "     Move: %-*s", HISTORY_WIDTH - 13, buf);
    mvwprintw(historyw, 3, 1, "Next move: %-*s", HISTORY_WIDTH - 13, 
	    last_history(history_index));
    mvwprintw(historyw, 4, 1, "Last move: %-*s", HISTORY_WIDTH - 13,
	    last_history(history_index - 1));
    return;
}

void update_status()
{
    int w = STATUS_WIDTH - 10;
    char *book, *engine;

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

    mvwprintw(statusw, 5, 1, " Round: %i", status.rounds);

    if (status.notify) {
	wattron(statusw, NOTIFY_STATUS);
	mvwprintw(statusw, STATUS_HEIGHT - 2,
		CENTERX(STATUS_WIDTH, status.notify), "%s", status.notify);
	wattroff(statusw, NOTIFY_STATUS);
    }
	
    return;
}

void update_data()
{
    int w;
    char *tmp;
    int i, tlen = 0;

    /* Get the longest token length. */
    for (i = 0; (i < DATA_HEIGHT - 2 && pgn[i].token[0]); i++) {
	int ttlen = strlen(pgn[i].token);

	if (tlen < ttlen)
	    tlen = ttlen;
    }

    w = DATA_WIDTH - tlen - 4;

    if ((tmp = real_filename(data.pgnfile)) == NULL)
	tmp = "none";

    mvwprintw(dataw, 2, 1, "%*s: %-*s", tlen, "File", w, tmp);

    for (i = 0; (i < DATA_HEIGHT - 2 && pgn[i].token[0]); i++)
	mvwprintw(dataw, i + 3, 1, "%*s: %-*s", tlen, pgn[i].token, w,
		pgn[i].value);

    /*
    mvwprintw(dataw, 6, 1, " White: %-*s", w,
	    (status.bw == WHITE) ? data.white : data.black);
    mvwprintw(dataw, 7, 1, " Black: %-*s", w,
	    (!status.bw == WHITE) ? data.white : data.black);
	    */
    return;
}

void draw_window_title(WINDOW *win, const char *title, int width)
{
    int i;

    wattron(win, WINDOW_TITLE);

    for (i = 1; i < width - 1; i++)
	mvwprintw(win, 1, i, "%c", ' ');

    mvwprintw(win, 1, CENTERX(width, title), "%s", title);

    wattroff(win, WINDOW_TITLE);
    wattron(win, WINDOW_BORDER);
    box(win, ACS_VLINE, ACS_HLINE);
    wattroff(win, WINDOW_BORDER);

    return;
}

void refresh_all()
{
    werase(statusw);
    werase(historyw);
    werase(dataw);
    werase(boardw);
    update_status();
    update_data();
    update_history();
    draw_window_title(statusw, STATUS_TITLE, STATUS_WIDTH);
    draw_window_title(dataw, DATA_TITLE, DATA_WIDTH);
    draw_window_title(historyw, HISTORY_TITLE, HISTORY_WIDTH);
    update_panels();
    doupdate();
    return;
}

/* FIXME segfault after 'q' (sometimes), scrolling */
void edit_pgn_data()
{
    const char *prompt = "UP/DOWN/ENTER selects, 'a' adds and 'q' quits";

    while (1) {
	WINDOW *win;
	PANEL *panel;
	int y = (pgn_index + 5 > LINES - 2) ? LINES - 2 : pgn_index + 5;
	int x = 0;
	int tlen = 0;
	int i;
	unsigned selected = 0;
	int cy = 2;
	char buf[3] = {0};
	char editprompt[76] = {0};
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

	if (x < strlen(prompt) + 4)
	    x = strlen(prompt) + 4;

	win = newwin(y, x, LINES / 2 - y / 2, CALCPOSX(x));
	panel = new_panel(win);
	draw_window_title(win, "Editing PGN Save Data", x);
	curs_set(1);
	cbreak();
	noecho();
	nonl();
	keypad(win, TRUE);

	for (i = 0; i < pgn_index; i++)
	    mvwprintw(win, 2 + i, 1, "%u. %*s: %-*s", i + 1, tlen,
		    pgn[i].token, (x - tlen - (sizeof(buf) + 4)), pgn[i].value);

	mvwprintw(win, y - 2, CENTERX(x, prompt), "%s", prompt);

	while (1) {
	    int c;
	    char *newtoken;

	    wmove(win, cy, 1);
	    update_panels();
	    doupdate();
	    c = wgetch(win);

	    switch (c) {
		case 'a':
		    if ((newtoken = get_input("New tag name", NULL)) == NULL)
			break;

		    if (add_pgn_data(&pgn_index, newtoken, NULL)) {
			message(ERROR, ANYKEY, 
				"Could not add duplicate tag \"%s\"",
				newtoken);
			    continue;
		    }

		    selected = pgn_index;
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
		    /* Get pgn_index number. */
		    mvwinnstr(win, cy, 1, buf, sizeof(buf) - 1);

		    if(sscanf(buf, "%u", &selected) != 1) {
			message(ERROR, ANYKEY, "Could not get index number");
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
	    message(NULL, ANYKEY, "Can't edit the \"Date\" tag.");
	    continue;
	}

	snprintf(editprompt, sizeof(editprompt),
		"Editing roster tag \"%s\"", pgn[selected].token);

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

void reset_history()
{
    history_index = history_total = 0;
    return;
}

void init_history()
{
    send_to_engine("manual\n");
    send_to_engine("show game\n");
    return;
}

/* FIXME scrolling for values wider than COLS and tokens that are more LINES */
/* Just dumps the pgn array to a window. */
void pgn_info()
{
    WINDOW *win;
    PANEL *pan;
    int y = (pgn_index + 5 > LINES - 2) ? LINES - 2 : pgn_index + 5;
    int x = 0;
    int tlen = 0;
    int i;

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

    if (x < strlen(ANYKEY) + 4)
	x = strlen(ANYKEY) + 4;

    win = newwin(y, x, LINES / 2 - y / 2, CALCPOSX(x));
    pan = new_panel(win);
    draw_window_title(win, "PGN Information", x);
    
    for (i = 0; i < pgn_index; i++)
	mvwprintw(win, 2 + i, 1, "%*s: %-*s", tlen, pgn[i].token, 
		(x - tlen - 4), pgn[i].value);

    mvwprintw(win, y - 2, CENTERX(x, ANYKEY), "%s", ANYKEY);

    update_panels();
    doupdate();
    wgetch(win);
    del_panel(pan);
    delwin(win);
    return;
}

void game_loop()
{
    int rrow = 8, rcol = 1;

    cursor_x = 2, cursor_y = 1;

    wtimeout(boardw, 1000);
    send_to_engine("nopost\n");

    if (data.pgnfile[0]) {
	send_to_engine("pgnload %s\n", data.pgnfile);
	browse_history = 1;
	init_history();
    }
    else
	send_to_engine("show board\n");

    while (!quit) {
	int c = 0;
	fd_set fds;
	int n, len;
	char enginebuf[8192] = {0};
	struct timeval tv;
	char *tmp;

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	FD_ZERO(&fds);
	FD_SET(from_engine, &fds);

	if ((n = select(from_engine + 1, &fds, NULL, NULL, &tv)) > 0) {
	    if (FD_ISSET(from_engine, &fds)) {
		if ((len = read(from_engine, enginebuf, sizeof(enginebuf))) 
			> 0) {
		    parse_engine_output(enginebuf);
		    update_status();
		    update_data();
		    update_history();
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
	    case 'i':
		if (!data.pgnfile[0])
		    break;

		pgn_info();
		break;
	    case 'v':
		message(NULL, ANYKEY, "%s\n%s\n\nTerminal supports %i colors.\n"
			"Using %s\n", PACKAGE_STRING, COPYRIGHT, COLORS,
			curses_version());
		break;
	    case 'g':
		if (browse_history || status.engine == ENGINE_THINKING)
		    break;

		send_to_engine("go\n");
		break;
	    case 'b':
		if (status.book_method == -1 || status.engine == ENGINE_THINKING)
		    break;

		if (status.book_method + 1 >= BOOK_MAX)
		    status.book_method = 0;
		else
		    status.book_method++;

		send_to_engine("book %s\n", book_methods[status.book_method]);
		break;
	    case 'h':
		if (browse_history) {
		    if (history_index != history_total) {
			if ((c = message(NULL, YESNO, 
					"Resume game from history?")) != 'y')
			    break;

			history_total = history_index;
			update_history();
		    }

	//	    reset_history();
		    browse_history = 0;

		    /* FIXME thinking isnt updated */
		    if (status.bw != status.turn)
			send_to_engine("go\n");
		    
		    /* FIXME need to 'resume' after leaving history mode */

		    break;
		}

		/* FIXME pgn history init */
		//init_history();
		send_to_engine("manual\n");
		browse_history = 1;
		status.engine = HISTORY_MODE;
		update_status();
		break;
	    case 'u':
		if (browse_history)
		    break;

		send_to_engine("remove\n");
		break;
	    case 'r':
		if (browse_history) {
		    reset_history();
		    browse_history = 0;
		    break;
		}

		if ((tmp = get_input("Load saved game filename", NULL)) == NULL)
		    break;

		if (parse_pgn_file(tmp))
		    message(NULL, ANYKEY, "%s: %s", tmp, strerror(errno));
		else {
		    strncpy(data.pgnfile, tmp, sizeof(data.pgnfile));
		    send_to_engine("pgnload %s\n", data.pgnfile);
		    init_history();
		}
		break;
	    case 's':
		/* FIXME user defined tags */
		if (message(NULL, YESNO, "Edit save game data?") == 'y')
		    edit_pgn_data();

		if ((tmp = get_input("Save game filename", NULL)) == NULL)
		    break;

		if (save_pgn(tmp)) {
		    message(NULL, ANYKEY, "%s: %s", tmp, strerror(errno));
		    break;
		}

		strncpy(data.pgnfile, tmp, sizeof(data.pgnfile));
		parse_pgn_file(data.pgnfile);
		update_data();
		update_status();
		break;
	    case '?':
		help();
		break;
	    case 'N':
		if (message(NULL, YESNO, "Really start a new game?") != 'y')
		    break;

		reset_history();
		browse_history = 0;

		if (data.pgnfile[0]) {
		    data.pgnfile[0] = 0;
		    parse_pgn_file(data.pgnfile);
		}

		status.rounds = 0;
		refresh_all();

		/* FIXME should be done in engine.c*/
		status.engine = ENGINE_READY;
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

		if ((tmp = get_input(ENGINE_COMMAND_PROMPT, NULL)) != NULL) {
		    send_to_engine("%s\n", tmp);
		}
		break;
	    case KEY_ESCAPE:
		selected_piece.icon = selected_y = selected_x = 0;
		break;
		/* FIXME diagonal keys */
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
		    if (history_index - 1 < 0)
			break;

		    send_to_engine("undo\n");
		    history_index--;
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
		    if (history_index + 1 > history_total)
			break;

		    send_to_engine("%s\n", history[history_index++].move);
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
	    case KEY_RETURN:
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
    printf("Usage: %s [-h] [-p <pgnfile>]\n", pn);
    printf("  -p  Load PGN file.\n");
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
	    refresh_all();
	    break;
	default:
	    break;
    }

    return;
}

int main(int argc, char *argv[])
{
    int opt;

    /* FIX THIS STUPID THING */
    printf("GNUChess is modified in main() to setlinebuf(). This fixes the "
	    "PIPE_BUF problem.\n");

    while ((opt = getopt(argc, argv, "hp:v")) != -1) {
	switch (opt) {
	    case 'v':
		printf("%s\n%s\n", PACKAGE_STRING, COPYRIGHT);
		exit(EXIT_SUCCESS);
	    case 'p':
		strncpy(data.pgnfile, optarg, sizeof(data.pgnfile));
		break;
	    case 'h':
	    default:
		usage(argv[0]);
	}
    }

    signal(SIGPIPE, catch_signal);
    signal(SIGCONT, catch_signal);
    signal(SIGSTOP, catch_signal);

    if (parse_pgn_file(data.pgnfile) != 0)
	err(EXIT_FAILURE, "%s", data.pgnfile);

    init_chess_engine();
    initscr();

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
    nonl();

    draw_window_title(dataw, DATA_TITLE, DATA_WIDTH);
    draw_window_title(statusw, STATUS_TITLE, STATUS_WIDTH);
    draw_window_title(historyw, HISTORY_TITLE, HISTORY_WIDTH);

    game_loop();
    send_to_engine("quit\n");
    endwin();
    del_panel(boardp);
    del_panel(statusp);
    delwin(boardw);
    delwin(statusw);
    del_panel(historyp);
    delwin(historyw);
    exit(EXIT_SUCCESS);
}
