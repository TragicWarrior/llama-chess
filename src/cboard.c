/* $Id: cboard.c,v 1.42 2002-12-21 21:32:17 bjk Exp $ */
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
	    int attrs = CP_BOARD_WHITE;
	    chtype piece;

	    if (row == 0 || row == maxy - 2) {
		if (col == 0)
		    mvwaddch(boardw, row, col, 
			    (row) ? ACS_LLCORNER | CP_BOARD_GRAPHICS :
			    ACS_ULCORNER | CP_BOARD_GRAPHICS);
		else if (col == maxx - 2)
		    mvwaddch(boardw, row, col,
			    (row) ? ACS_LRCORNER | CP_BOARD_GRAPHICS : 
			    ACS_URCORNER | CP_BOARD_GRAPHICS);
		else if (!(col % 4))
		    mvwaddch(boardw, row, col, 
			    (row) ? ACS_BTEE | CP_BOARD_GRAPHICS : 
			    ACS_TTEE | CP_BOARD_GRAPHICS);
		else {
		    if (col != maxx - 1)
			mvwaddch(boardw, row, col,
				ACS_HLINE | CP_BOARD_GRAPHICS);
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
			    (col) ? ACS_RTEE | CP_BOARD_GRAPHICS : 
			    ACS_LTEE | CP_BOARD_GRAPHICS);
		else
		    mvwaddch(boardw, row, col, ACS_VLINE | CP_BOARD_GRAPHICS);

		continue;
	    }

	    if ((row % 2) && !(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col, ACS_VLINE | CP_BOARD_GRAPHICS);
		continue;
	    }

	    if (!(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col, ACS_PLUS | CP_BOARD_GRAPHICS);
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

		    if (row == cursor_y && col == (cursor_x - 1)) {
			attrs = CP_BOARD_CURSOR;
		    }

		    if (row == selected_y && col == (selected_x - 1)) {
			attrs = CP_BOARD_SELECTED;
		    }

		    if (row == maxy - 1)
			attrs = 0;

		    wattron(boardw, attrs);
		    mvwaddch(boardw, row, col, ' ');

		    if (row == maxy - 1)
			waddch(boardw, x_grid_chars[rcol] | CP_BOARD_COORDS);
		    else {
			piece = board[row / 2][rcol].icon;

			if (status.bw == WHITE && isupper(piece))
			    attrs |= A_BOLD;
			else if (status.bw == BLACK && islower(piece))
			    attrs |= A_BOLD;

			waddch(boardw, (piece && piece !=  '.') ?
				piece | attrs : ' ' | attrs);
		    }

		    waddch(boardw, ' ');
		    wattroff(boardw, attrs);
		    col += 2;
		    rcol++;
		}
	    }
	    else {
		if (col != maxx - 1)
		    mvwaddch(boardw, row, col, ACS_HLINE | CP_BOARD_GRAPHICS);
	    }
	}
    }

    return;
}

void parse_piece_command()
{
    char str[MAX_PGN_MOVE_LEN] = {0};

    snprintf(str, sizeof(str), "%c%i%c%i", x_grid_chars[sp.col - 1], sp.row,
	    x_grid_chars[sp.destcol - 1], sp.destrow);
    SEND_TO_ENGINE("%s\n", str);
    selected_x = selected_y = 0;
    return;
}

char *book_method(int method)
{
    char *book;

    switch (method) {
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

    return book;
}

void update_status()
{
    int w = STATUS_WIDTH - 12;
    int i;
    char *engine;
    char buf[w + 1];

    snprintf(buf, sizeof(buf), "%i of %i", gindex + 1, gtotal);
    mvwprintw(statusw, 2, 1, "    Game: %-*s", w, buf);

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
	case ENGINE_INITIALIZING:
	    engine = "initializing ...";
	    break;
	default:
	    engine = UNKNOWN;
	    break;
    }

    mvwprintw(statusw, 3, 1, "  Engine: %-*s", w, " ");
    wattron(statusw, CP_STATUS_ENGINE);
    mvwaddstr(statusw, 3, 11, engine);
    wattroff(statusw, CP_STATUS_ENGINE);

    mvwprintw(statusw, 4, 1, "   Depth: %-*i", w, config.engine_depth);

    mvwprintw(statusw, 5, 1, "    Book: %-*s", w,
	    book_method(config.book_method));

    mvwprintw(statusw, 6, 1, "    Turn: %-*s", w, 
	    (status.turn == WHITE) ? "white" : "black");

    snprintf(buf, sizeof(buf), "%i white  %i black", game[gindex].wcaptures,
	    game[gindex].bcaptures);
    mvwprintw(statusw, 7, 1, "Captures: %-*s", w, buf);

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
    struct history h = {{0},{0}};

    if (game[gindex].htotal)
	snprintf(buf, sizeof(buf), "%u of %u", game[gindex].hindex,
		game[gindex].htotal);
    else
	strncpy(buf, UNKNOWN, sizeof(buf));

    mvwprintw(historyw, 2, 1, "     Move: %-*s", HISTORY_WIDTH - 13, buf);

    get_history_by_index(game[gindex].hindex, &h);

    snprintf(buf, sizeof(buf), "%s %s", (h.move[0]) ? h.move : NONE,
	    (h.comment[0] || h.nag[0]) ? "(press 'v')" : "");
    mvwprintw(historyw, 3, 1, "Next move: %-*s", HISTORY_WIDTH - 13, buf);

    if (get_history_by_index(game[gindex].hindex - 1, &h))
	h.move[0] = 0;

    snprintf(buf, sizeof(buf), "%s %s", (h.move[0]) ? h.move : NONE,
	    (h.comment[0] || h.nag[0]) ? "(press 'V')" : "");
    mvwprintw(historyw, 4, 1, "Last move: %-*s", HISTORY_WIDTH - 13, buf);
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
	tmp = NONE;

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
    draw_window_title(statusw, STATUS_TITLE, STATUS_WIDTH, CP_STATUS_TITLE,
	    CP_STATUS_BORDER);
    draw_window_title(dataw, DATA_TITLE, DATA_WIDTH, CP_DATA_TITLE, 
	    CP_DATA_BORDER);
    draw_window_title(historyw, HISTORY_TITLE, HISTORY_WIDTH, CP_HISTORY_TITLE,
	    CP_HISTORY_BORDER);
    update_panels();
    doupdate();
    return;
}

static void game_next_prev(int n)
{
    if (gtotal < 2)
	return;

    if (n == 1) {
	if (gindex + 1 == gtotal)
	    gindex = 0;
	else
	    gindex++;
    }
    else {
	if (gindex - 1 < 0)
	    gindex = gtotal - 1;
	else
	    gindex--;
    }

    init_history();

    if (gindex == gactive) {
	browse_history = 0;
	status.engine = ENGINE_READY;
    }

    update_all();
    return;
}

void game_loop()
{
    int rrow = 8, rcol = 1;
    int error_recover = 0;

    gactive = gindex;
    cursor_x = 2, cursor_y = 1;
    gindex = gtotal - 1;

    if (pgnfile[0])
	init_history();

    flushinp();
    wtimeout(boardw, 70);
    update_all();

    while (!quit) {
	int c = 0;
	fd_set fds;
	int n, len;
	char enginebuf[8192] = {0};
	struct timeval tv;
	char *tmp;
	char buf[78];
	struct pgndata *tmppgn = NULL;

	if (status.engine != ENGINE_OFFLINE) {
	    tv.tv_sec = 0;
	    tv.tv_usec = 0;

	    FD_ZERO(&fds);
	    FD_SET(enginefd[0], &fds);

	    if ((n = select(enginefd[0] + 1, &fds, NULL, NULL, &tv)) > 0) {
		if (FD_ISSET(enginefd[0], &fds)) {
		    len = read(enginefd[0], enginebuf, sizeof(enginebuf));

		    if (len == -1) {
			if (errno == EAGAIN)
			    goto blah;
			else {
			    message(ERROR, ANYKEY, "Attempt #%i. read(): %s",
				    ++error_recover, strerror(errno));
			    continue;
			}
		    }

		    if (len)
			parse_engine_output(enginebuf);

		    update_all();
		}
	    }
	    else {
		/* timeout */
	    }
	}

blah:
	error_recover = 0;
	draw_board();
	wmove(boardw, cursor_y, cursor_x);
	update_panels();
	doupdate();

	if ((c = wgetch(boardw)) == ERR)
	    continue;

	switch (c) {
	    int annotate;

	    case 'v':
	        view_annotation(game[gindex].hindex);
		break;
	    case 'V':
	        view_annotation(game[gindex].hindex - 1);
		break;
	    case '>':
		game_next_prev(1);
		break;
	    case '<':
		game_next_prev(0);
		break;
	    case 'a':
	        annotate = game[gindex].hindex;

		if (annotate && game[gindex].history[annotate - 1].move[0])
		    annotate--;
		else
		    break;

		snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATE_HISTORY,
			game[gindex].history[annotate].move);

		tmp = get_input(buf, game[gindex].history[annotate].comment, 
			0, 0, NAG_PROMPT, history_edit_nag, (void *)annotate,
			-1);

		if (tmp)
		    strncpy(game[gindex].history[annotate].comment, tmp,
			sizeof(game[gindex].history[annotate].comment));
		else
		    game[gindex].history[annotate].comment[0] = 0;

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
		    if (game[gindex].hindex != game[gindex].htotal) {
			if ((c = message(NULL, YESNO, 
					"Resume game from history?")) != 'y')
			    break;
		    }

		    if (!engine_initialized) {
			if (start_chess_engine() < 0)
			    break;
		    }

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

		init_history();
		break;
	    case 'u':
		if (browse_history || !game[gindex].htotal)
		    break;

		history_previous(2);
		game[gindex].htotal = game[gindex].hindex;

		SEND_TO_ENGINE("remove\n");
		update_history();
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
		gactive = gindex;
		strncpy(pgnfile, tmp, sizeof(pgnfile));
		init_history();
		update_all();
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

		if (save_pgn(tmp, (tmppgn) ? tmppgn : game[gindex].pgn, 0)) {
		    if (tmppgn)
			free(tmppgn);
		    break;
		}

		free(tmppgn);
		strncpy(pgnfile, tmp, sizeof(pgnfile));
		parse_pgn_file(pgnfile);
		gindex = gtotal - 1;
		gactive = gindex;
		update_all();
		break;
	    case CTRL('G'):
		help(MAIN_HELP, mainhelp);
		break;
	    case 'N':
		if (message(NULL, YESNO, "Really start a new game?") != 'y')
		    break;

		reset_history();
		browse_history = pgnfile[0] = sp.icon = 0;

		parse_pgn_file(pgnfile);
		status.bw = WHITE;
		game[gindex].wcaptures = game[gindex].bcaptures = 0;

		if (status.engine == ENGINE_OFFLINE) {
		    if (start_chess_engine() < 0)
			break;
		}

		SEND_TO_ENGINE("\nnew\n");
		set_engine_defaults();
		update_all();
		break;
	    case 'R':
		refresh_all();
		break;
	    case 'c':
		if (status.engine == ENGINE_THINKING)
		    break;

		if (status.engine == ENGINE_OFFLINE)
		    break;

		if ((tmp = get_input_str_clear(ENGINE_COMMAND_PROMPT, NULL)) 
			!= NULL) {
		    SEND_TO_ENGINE("%s\n", tmp);
		}
		break;
	    case KEY_ESCAPE:
		sp.icon = selected_y = selected_x = 0;
		break;
	    case 'j':
	    case KEY_UP:
		if (browse_history) {
		    history_next(config.history_jump);
		    update_history();
		    update_status();
		    break;
		}

		if (cursor_y - 2 < 1)
		    cursor_y = BOARD_HEIGHT - 3, rrow = 1;
		else
		    cursor_y -= 2, rrow++;
		break;
	    case 'k':
	    case KEY_DOWN:
		if (browse_history) {
		    history_previous(config.history_jump);
		    update_status();
		    update_history();
		    break;
		}

		if (cursor_y + 2 > BOARD_HEIGHT - 2)
		    cursor_y = 1, rrow = 8;
		else
		    cursor_y += 2, rrow--;
		break;
	    case 'l':
	    case KEY_LEFT:
		if (browse_history) {
		    history_previous(1);
		    update_status();
		    update_history();
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
		    history_next(1);
		    update_status();
		    update_history();
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

		SEND_TO_ENGINE("\nswitch\n");
		break;
	    case ' ':
		if (status.engine != ENGINE_READY) {
		    message(NULL, ANYKEY, "Use the 'N' command to start a "
			    "new game or the 'r' command to load a previous "
			    "game");
		    break;
		}

		if (sp.icon || status.engine == ENGINE_THINKING) {
		    beep();
		    break;
		}

		if (browse_history)
		    break;

		if ((sp.icon = winch(boardw) & A_CHARTEXT) == ' ') {
		    sp.icon = 0;
		    break;
		}

		if ((islower(sp.icon) && status.turn != BLACK) ||
			(isupper(sp.icon) && status.turn != WHITE)) {
		    sp.icon = 0;
		    break;
		}

		sp.row = rrow;
		sp.col = rcol;
		selected_x = cursor_x;
		selected_y = cursor_y;
		break;
	    case '\015':
	    case '\n':
		if (browse_history)
		    break;

		if (status.engine == ENGINE_THINKING) {
		    beep();
		    break;
		}

		if (!sp.icon)
		    break;

		sp.destrow = rrow;
		sp.destcol = rcol;
		parse_piece_command();
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
	case SIGINT:
	    stop_engine();
	    endwin();
	    exit(EXIT_FAILURE);
	    break;
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

void free_game_data()
{
    int i;

    if (!gtotal)
	return;

    for (i = 0; i < gtotal; i++) {
	free(game[i].pgn);
	free(game[i].history);
    }

    /*
    free(game);
    */
    return;
}

static void set_defaults()
{
    status.engine = ENGINE_OFFLINE;

    config.history_jump = 5;
    config.book_method = BOOK_RANDOM;
    config.engine_depth = 0;
    config.historyagony = 0;
    config.agony = 1;

    set_default_colors();
    set_pgn_defaults();
    return;
}

int main(int argc, char *argv[])
{
    int opt;
    char datadir[FILENAME_MAX] = {0};
    struct passwd *pwd;
    struct stat st;

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

    if ((pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    snprintf(datadir, sizeof(datadir), "%s/.cboard", pwd->pw_dir);
    snprintf(config.nagfile, sizeof(config.nagfile), "%s/nag.data", datadir);
    snprintf(config.agonyfile, sizeof(config.agonyfile), "%s/agony.data",
	    datadir);
    snprintf(config.configfile, sizeof(config.configfile),  "%s/config",
	    datadir);
    snprintf(config.fifo, sizeof(config.fifo), "%s/fifo", datadir);

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
	errx(EXIT_FAILURE, "%s: not a directory", datadir);

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

    if ((opt = parse_pgn_file(pgnfile)) != 0) {
	if (opt > 0)
	    err(EXIT_FAILURE, "%s", pgnfile);
	else
	    errx(EXIT_FAILURE, "%s: parse error", pgnfile);
    }

    srandom(getpid());
    initscr();

    if (start_chess_engine()) {
	endwin();
	exit(EXIT_FAILURE);
    }

    if (has_colors() == TRUE && start_color() == OK)
	init_color_pairs();

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

    wbkgd(statusw, CP_STATUS_WINDOW);
    draw_window_title(statusw, STATUS_TITLE, STATUS_WIDTH, CP_STATUS_TITLE,
	    CP_STATUS_BORDER);
    wbkgd(dataw, CP_DATA_WINDOW);
    draw_window_title(dataw, DATA_TITLE, DATA_WIDTH, CP_DATA_TITLE, 
	    CP_DATA_BORDER);
    wbkgd(historyw, CP_HISTORY_WINDOW);
    draw_window_title(historyw, HISTORY_TITLE, HISTORY_WIDTH, CP_HISTORY_TITLE,
	    CP_HISTORY_BORDER);

    game_loop();
    stop_engine();

    endwin();
    free_game_data();
    del_panel(boardp);
    del_panel(historyp);
    /* this segfaults */
    del_panel(statusp);
    del_panel(datap);
    delwin(boardw);
    delwin(historyw);
    delwin(statusw);
    delwin(dataw);
    exit(EXIT_SUCCESS);
}
