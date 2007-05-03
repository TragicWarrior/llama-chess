/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2007 Ben Kibbey <bjk@luxsci.net>

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
#include <time.h>
#include <err.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#include "chess.h"
#include "conf.h"
#include "window.h"
#include "message.h"
#include "colors.h"
#include "input.h"
#include "misc.h"
#include "engine.h"
#include "strings.h"
#include "common.h"
#include "menu.h"
#include "keys.h"
#include "rcfile.h"
#include "filebrowser.h"
#include "cboard.h"

#ifdef DEBUG
#include <debug.h>
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

void update_cursor(GAME g, int idx)
{
    char *p;
    int len;
    int t = pgn_history_total(g->hp);
    struct userdata_s *d = g->data;

    /*
     * If not deincremented then r and c would be the next move.
     */
    idx--;

    if (idx > t || idx < 0 || !t || !g->hp[idx]->move) {
	d->c_row = 2, d->c_col = 5;
	return;
    }

    p = g->hp[idx]->move;
    len = strlen(p);

    if (*p == 'O') {
	if (len <= 4)
	    d->c_col = 7;
	else
	    d->c_col = 3;

	d->c_row = (g->turn == WHITE) ? 8 : 1;
	return;
    }

    p += len;

    while (!isdigit(*p))
	p--;

    d->c_row = RANKTOINT(*p--);
    d->c_col = FILETOINT(*p);
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

    nags = Realloc(nags, (i+2) * sizeof(char *));
    nags[i++] = strdup(NONE);
    nags[i] = NULL;

    while (!feof(fp)) {
	if (fscanf(fp, " %[^\n] ", line) == 1) {
	    nags = Realloc(nags, (i + 2) * sizeof(char *));
	    nags[i++] = strdup(line);
	}
    }

    nags[i] = NULL;
    nag_total = i;
    return 0;
}

void edit_nag_toggle_item(struct menu_input_s *m)
{
    struct input_s *in = m->data;
    struct input_data_s *id = in->data;
    HISTORY *h = id->data;
    int i;

    if (m->selected == 0) {
	for (i = 0; i < MAX_PGN_NAG; i++)
	    h->nag[i] = 0;

	for (i = 0; m->items[i]; i++)
	    m->items[i]->selected = 0;

	return;
    }

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (h->nag[i] == m->selected)
	    h->nag[i] = m->selected = 0;
	else {
	    if (!h->nag[i]) {
		h->nag[i] = m->selected;
		break;
	    }
	}
    }
}

void edit_nag_save(struct menu_input_s *m)
{
    pushkey = -1;
}

void edit_nag_help(struct menu_input_s *m)
{
    message(NAG_EDIT_HELP, ANYKEY, "%s", naghelp);
}

struct menu_item_s **get_nag_items(WIN *win)
{
    int i, n;
    struct menu_input_s *m = win->data;
    struct input_s *in = m->data;
    struct input_data_s *id = in->data;
    struct menu_item_s **items = m->items;
    HISTORY *h = id->data;

    if (items) {
	for (i = 0; items[i]; i++)
	    free(items[i]);
    }

    for (i = 0; nags[i]; i++) {
	items = Realloc(items, (i+2) * sizeof(struct menu_item_s *));
	items[i] = Malloc(sizeof(struct menu_item_s));
	items[i]->name = nags[i];
	items[i]->value = NULL;

	for (n = 0; n < MAX_PGN_NAG; n++) {
	    if (h->nag[n] == i) {
		items[i]->selected = 1;
		n = -1;
		break;
	    }
	}

	if (n >= 0)
	    items[i]->selected = 0;
    }

    items[i] = NULL;
    m->nofree = 1;
    m->items = items;
    return items;
}

void nag_print(WIN *win)
{
    struct menu_input_s *m = win->data;

    mvwprintw(win->w, m->print_line, 1, "%-*s", win->cols - 2, m->item->name);
}

void edit_nag(void *arg)
{
    struct menu_key_s **keys = NULL;

    if (!nags) {
	if (init_nag())
	    return;
    }

    add_menu_key(&keys, ' ', edit_nag_toggle_item);
    add_menu_key(&keys, CTRL('x'), edit_nag_save);
    add_menu_key(&keys, KEY_F(1), edit_nag_help);
    construct_menu(0, 0, -1, -1, NAG_EDIT_TITLE, 1, get_nag_items, keys, arg,
	    nag_print, NULL);
    return;
}

static void *view_nag(void *arg)
{
    HISTORY *h = (HISTORY *)arg;
    char buf[80];
    char line[LINE_MAX] = {0};
    int i = 0;

    snprintf(buf, sizeof(buf), "%s \"%s\"", VIEW_MOVE_NAG, h->move);

    if (!nags) {
	if (init_nag())
	    return NULL;
    }

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (!h->nag[i])
	    break;

	if (h->nag[i] >= nag_total)
	    strncat(line, itoa(h->nag[i]), sizeof(line));
	else
	    strncat(line, nags[h->nag[i]], sizeof(line));

	strncat(line, "\n", sizeof(line));
    }

    line[strlen(line) - 1] = 0;
    message(buf, ANYKEY, "%s", line);
    return NULL;
}

void view_annotation(HISTORY *h)
{
    char buf[MAX_SAN_MOVE_LEN + strlen(ANNOTATION_VIEW_TITLE) + 4];
    int nag = 0, comment = 0;

    if (!h)
	return;

    if (h->comment && h->comment[0])
        comment++;
 
    if (h->nag[0])
 	nag++;

    if (!nag && !comment)
	return;

    snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_VIEW_TITLE, h->move);

    if (comment)
	construct_message(buf, (nag) ? ANY_OTHER_KEY : ANYKEY, 0, 1,
		(nag) ? VIEW_NAG : NULL, 
		(nag) ? view_nag : NULL, (nag) ? h : NULL, NULL,
		(nag) ? 'n' : 0, 0, "%s", h->comment);
    else
	construct_message(buf, ANY_OTHER_KEY, 0, 1, VIEW_NAG, view_nag, h, NULL, 
		'n', 0, "%s", NO_ANNOTATIONS);
}

void do_game_write(char *filename, char *mode, int start, int end)
{
    char *command = NULL;
    FILE *fp;
    int i;
    struct userdata_s *d;

    if (command) {
	if ((fp = popen(command, "w")) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	    goto error;
	}
    }
    else {
	if ((fp = fopen(filename, mode)) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	    goto error;
	}
    }

    for (i = (start == -1) ? 0 : start; i < end; i++) {
	d = game[i]->data;
	pgn_write(fp, game[i]);
	CLEAR_FLAG(d->flags, CF_MODIFIED);
    }

    if (command)
	pclose(fp);
    else
	fclose(fp);

    if (start == -1)
	strncpy(loadfile, filename, sizeof(loadfile));

    update_status_notify(gp, "%s", NOTIFY_SAVED);
    update_all(gp);
    return;

error:
    update_status_notify(gp, "%s", NOTIFY_SAVE_FAILED);
    update_all(gp);
}

struct save_game_s {
    char *filename;
    char *mode;
    int start;
    int end;
};

void do_save_game_overwrite_confirm(WIN *win)
{
    char *mode = "w";
    struct save_game_s *s = win->data;

    switch (win->c) {
	case 'a':
	    if (pgn_is_compressed(s->filename) == E_PGN_OK) {
		cmessage(NULL, ANYKEY, "%s", E_SAVE_COMPRESS);
		goto done;
	    }

	    mode = "a";
	    break;
	case 'o':
	    mode = "w+";
	    break;
	default:
	    goto done;
    }

    do_game_write(s->filename, mode, s->start, s->end);

done:
    free(s->filename);
    free(s);
}

/* If the saveindex argument is -1, all games will be saved. Otherwise it's a
 * game index number.
 */
// FIXME command (compression)
void save_pgn(char *filename, int saveindex)
{
    char buf[FILENAME_MAX];
    struct stat st;
    int end = (saveindex == -1) ? gtotal : saveindex + 1;
    struct save_game_s *s;

    if (filename[0] != '/' && config.savedirectory) {
	if (stat(config.savedirectory, &st) == -1) {
	    if (errno == ENOENT) {
		if (mkdir(config.savedirectory, 0755) == -1) {
		    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			    strerror(errno));
		    return;
		}
	    }
	    else {
		cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			strerror(errno));
		return;
	    }
	}

	stat(config.savedirectory, &st);

	if (!S_ISDIR(st.st_mode)) {
	    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory, E_NOTADIR);
	    return;
	}

	snprintf(buf, sizeof(buf), "%s/%s", config.savedirectory, filename);
	filename = buf;
    }

    if (access(filename, W_OK) == 0) {
	s = Malloc(sizeof(struct save_game_s));
	s->filename = strdup(filename);
	s->start = saveindex;
	s->end = end;
	construct_message(NULL, GAME_SAVE_OVERWRITE_PROMPT, 1, 1, NULL, NULL,
		s, do_save_game_overwrite_confirm, 0, 0, "%s \"%s\"",
		E_FILEEXISTS, filename);
	return;
    }

    do_game_write(filename, "a", saveindex, end);
}

static int castling_state(GAME g, BOARD b, int row, int col, int piece, int mod)
{
    if (pgn_piece_to_int(piece) == ROOK && col == 7
	    && row == 7 &&
	    (TEST_FLAG(g->flags, GF_WK_CASTLE) || mod) &&
	    pgn_piece_to_int(b[7][4].icon) == KING && isupper(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_WK_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 0
	    && row == 7 &&
	    (TEST_FLAG(g->flags, GF_WQ_CASTLE) || mod) &&
	    pgn_piece_to_int(b[7][4].icon) == KING && isupper(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_WQ_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 7
	    && row == 0 &&
	    (TEST_FLAG(g->flags, GF_BK_CASTLE) || mod) &&
	    pgn_piece_to_int(b[0][4].icon) == KING && islower(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_BK_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 0
	    && row == 0 &&
	    (TEST_FLAG(g->flags, GF_BQ_CASTLE) || mod) &&
	    pgn_piece_to_int(b[0][4].icon) == KING && islower(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_BQ_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == KING && col == 4
	    && row == 7 && 
	    (mod || (pgn_piece_to_int(b[7][7].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_WK_CASTLE))
	      ||
	     (pgn_piece_to_int(b[7][0].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_WQ_CASTLE))) && isupper(piece)) {
	if (mod) {
	    if (TEST_FLAG(g->flags, GF_WK_CASTLE) ||
		    TEST_FLAG(g->flags, GF_WQ_CASTLE))
		CLEAR_FLAG(g->flags, GF_WK_CASTLE|GF_WQ_CASTLE);
	    else
		SET_FLAG(g->flags, GF_WK_CASTLE|GF_WQ_CASTLE);
	}
	return 1;
    }
    else if (pgn_piece_to_int(piece) == KING && col == 4
	    && row == 0 &&
	    (mod || (pgn_piece_to_int(b[0][7].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_BK_CASTLE))
	      ||
	     (pgn_piece_to_int(b[0][0].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_BQ_CASTLE))) && islower(piece)) {
	if (mod) {
	    if (TEST_FLAG(g->flags, GF_BK_CASTLE) ||
		    TEST_FLAG(g->flags, GF_BQ_CASTLE))
		CLEAR_FLAG(g->flags, GF_BK_CASTLE|GF_BQ_CASTLE);
	    else
		SET_FLAG(g->flags, GF_BK_CASTLE|GF_BQ_CASTLE);
	}
	return 1;
    }

    return 0;
}

#define IS_ENPASSANT(c)	(c == 'x') ? CP_BOARD_COORDS : isupper(c) ? CP_BOARD_WHITE : CP_BOARD_BLACK
#define ATTRS(cp) (cp & (A_BOLD|A_STANDOUT|A_BLINK|A_DIM|A_UNDERLINE|A_INVIS|A_REVERSE))

static void draw_board(GAME g)
{
    int row, col;
    int bcol = 0, brow = 0;
    int maxy = BOARD_HEIGHT, maxx = BOARD_WIDTH;
    int ncols = 0, offset = 1;
    unsigned coords_y = 8;
    struct userdata_s *d = g->data;

    if (d->mode != MODE_PLAY && d->mode != MODE_EDIT)
	update_cursor(g, g->hindex);

    for (row = 0; row < maxy; row++) {
	bcol = 0;

	for (col = 0; col < maxx; col++) {
	    int attrwhich = -1;
	    chtype attrs = 0, old_attrs = 0;
	    unsigned char p;
	    int pi;

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

		    p = d->b[row / 2][bcol].icon;
		    pi = pgn_piece_to_int(p);

		    if (config.details && d->b[row / 2][bcol].enpassant) {
			p = pi = 'x'; 
			attrs = IS_ENPASSANT(p);
		    }

		    if (config.validmoves && d->b[brow][bcol].valid) {
			old_attrs = -1;

			if (attrwhich == WHITE)
			    attrs = mix_cp(CP_BOARD_MOVES_WHITE, IS_ENPASSANT(p), 
				    ATTRS(CP_BOARD_MOVES_WHITE), B_FG_A_BG);
			else
			    attrs = mix_cp(CP_BOARD_MOVES_BLACK, IS_ENPASSANT(p),
				    ATTRS(CP_BOARD_MOVES_BLACK), B_FG_A_BG);
		    }
		    else if (p != 'x')
			attrs = (attrwhich == WHITE) ? CP_BOARD_WHITE : CP_BOARD_BLACK;

		    if (row == ROWTOMATRIX(d->c_row) && col == 
			    COLTOMATRIX(d->c_col)) {
			attrs = mix_cp(CP_BOARD_CURSOR, IS_ENPASSANT(p), 
				ATTRS(CP_BOARD_CURSOR), B_FG_A_BG);
			old_attrs = -1;
		    }
		    else if (row == ROWTOMATRIX(d->sp.srow) && 
			    col == COLTOMATRIX(d->sp.scol)) {
			attrs = mix_cp(CP_BOARD_SELECTED, IS_ENPASSANT(p),
				ATTRS(CP_BOARD_SELECTED), B_FG_A_BG);
			old_attrs = -1;
		    }

		    if (row == maxy - 1)
			attrs = 0;

		    mvwaddch(boardw, row, col, ' ' | attrs);

		    if (row == maxy - 1)
			waddch(boardw, x_grid_chars[bcol] | CP_BOARD_COORDS);
		    else {
			if (old_attrs == -1) {
			    old_attrs = attrs;
			    goto printc;
			}

			old_attrs = attrs;

			if (pi != OPEN_SQUARE && p != 'x') {
			    if (attrwhich == WHITE) {
				if (isupper(p))
				    attrs = CP_BOARD_W_W;
				else
				    attrs = CP_BOARD_W_B;
			    }
			    else {
				if (isupper(p))
				    attrs = CP_BOARD_B_W;
				else
				    attrs = CP_BOARD_B_B;
			    }
			}

printc:
			if (config.details && castling_state(g, d->b, brow,
				    bcol, p, 0)) {
			    attrs = mix_cp(CP_BOARD_CASTLING, attrs, 
				    ATTRS(CP_BOARD_CASTLING), A_FG_B_BG);
			}

			waddch(boardw, (pi != OPEN_SQUARE) ? p | attrs : ' ' | attrs);
			attrs = old_attrs;
		    }

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

void invalid_move(int n, int e, const char *m)
{
    if (curses_initialized)
	cmessage(ERROR, ANYKEY, "%s \"%s\" (round #%i)", (e == E_PGN_AMBIGUOUS)
		? E_AMBIGUOUS : E_INVALID_MOVE, m, n);
    else
	warnx("%s: %s \"%s\" (round #%i)", loadfile, (e == E_PGN_AMBIGUOUS) 
		? E_AMBIGUOUS : E_INVALID_MOVE, m, n);
}

void gameover(GAME g)
{
    SET_FLAG(g->flags, GF_GAMEOVER);
    stop_engine(g);
}

static void update_clock(GAME g, struct itimerval it)
{
    struct userdata_s *d = g->data;
    long n;

    if (g->turn == WHITE) {
	d->wc.tv_sec += it.it_value.tv_sec;
	d->wc.tv_usec += it.it_value.tv_usec;

	if (d->wc.tv_usec > 1000000 - 1) {
	    d->wc.tv_sec += d->wc.tv_usec / 1000000;
	    d->wc.tv_usec = d->wc.tv_usec % 1000000;
	}

	if (d->wc.tv_sec >= d->limit) {
	    pgn_tag_add(&g->tag, "Result", "0-1");
	    gameover(g);
	}
    }
    else {
	d->bc.tv_sec += it.it_value.tv_sec;
	d->bc.tv_usec += it.it_value.tv_usec;

	if (d->bc.tv_usec > 1000000 - 1) {
	    d->bc.tv_sec += d->bc.tv_usec / 1000000;
	    d->bc.tv_usec = d->bc.tv_usec % 1000000;
	}

	if (d->bc.tv_sec >= d->limit) {
	    pgn_tag_add(&g->tag, "Result", "1-0");
	    gameover(g);
	}
    }

    d->elapsed = d->wc.tv_sec + d->bc.tv_sec;
    n = d->wc.tv_usec + d->bc.tv_usec;
    d->elapsed += (n > 1000000 - 1) ? n / 1000000 : 0;
}

void do_validate_move(char *m)
{
    struct userdata_s *d = gp->data;
    int n;
    char *frfr = NULL;

    if (TEST_FLAG(d->flags, CF_HUMAN)) {
	if ((n = pgn_parse_move(gp, d->b, &m, &frfr)) != E_PGN_OK) {
	    invalid_move(d->n + 1, n, m);
	    free(m);
	    return;
	}

	pgn_history_add(gp, m);
	pgn_switch_turn(gp);
    }
    else {
	if ((n = pgn_validate_move(gp, d->b, &m, &frfr)) != E_PGN_OK) {
	    invalid_move(d->n + 1, n, m);
	    free(m);
	    return;
	}

	add_engine_command(gp, ENGINE_THINKING, "%s\n", 
		(config.engine_protocol == 1) ? frfr : m);
    }

    d->sp.srow = d->sp.scol = d->sp.icon = 0;

    if (config.validmoves)
	pgn_reset_valid_moves(d->b);

    if (TEST_FLAG(gp->flags, GF_GAMEOVER))
	d->mode = MODE_HISTORY;
    else
	SET_FLAG(d->flags, CF_MODIFIED);

    d->paused = 0;
    free(m);
    return;
}

void do_promotion_piece_finalize(WIN *win)
{
    char *p, *str = win->data;

    if (pgn_piece_to_int(win->c) == -1)
	return;

    p = str + strlen(str);
    *p++ = toupper(win->c);
    *p = '\0';
    do_validate_move(str);
}

static void move_to_engine(GAME g)
{
    struct userdata_s *d = g->data;
    char *str;
    int piece;

    if (config.validmoves && 
	    !d->b[RANKTOBOARD(d->sp.row)][FILETOBOARD(d->sp.col)].valid)
	return;

    str = Malloc(MAX_SAN_MOVE_LEN + 1);
    snprintf(str, MAX_SAN_MOVE_LEN + 1, "%c%i%c%i",
	    x_grid_chars[d->sp.scol - 1], 
	    d->sp.srow, x_grid_chars[d->sp.col - 1], d->sp.row);

    piece = pgn_piece_to_int(d->b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon);

    if (piece == PAWN && (d->sp.row == 8 || d->sp.row == 1)) {
	construct_message(PROMOTION_TITLE, PROMOTION_PROMPT, 1, 1, NULL, NULL,
		str, do_promotion_piece_finalize, 0, 0, "%s", PROMOTION_TEXT);
	return;
    }

    do_validate_move(str);
}

static char *clock_to_char(long n)
{
    static char buf[16];
    int h = 0, m = 0, s = 0;

    h = n / 3600;
    m = (n % 3600) / 60;
    s = (n % 3600) % 60;
    snprintf(buf, sizeof(buf), "%.2i:%.2i:%.2i", h, m, s);
    return buf;
}

static char *timeval_to_char(struct timeval t, long limit)
{
    static char buf[11];
    int h = 0, m = 0, s = 0;
    int n = (limit == 0) ? 0 : limit - t.tv_sec;
    int i = -((int)t.tv_usec / 10000 / 10) + 10;

    i = (i == 10) ? i - 10 : i;
    h = n / 3600;
    m = (n % 3600) / 60;
    s = (n % 3600) % 60;
    snprintf(buf, sizeof(buf), "%.2i:%.2i:%.2i.%i", h, m, s, i);
    return buf;
}

void update_status_window(GAME g)
{
    int i = 0;
    char *buf;
    char tmp[15], *engine, *mode;
    int w;
    char *p;
    int maxy, maxx;
    int len;
    struct userdata_s *d = g->data;
    int y;
    int n;

    getmaxyx(statusw, maxy, maxx);
    w = maxx - 2 - 8;
    len = maxx - 2;
    buf = Malloc(len);
    y = 2;

    mvwprintw(statusw, y++, 1, "%*s %-*s", 7, STATUS_FILE_STR, w,
	    (loadfile[0]) ? str_etc(loadfile, w, 1) : UNAVAILABLE);
    snprintf(buf, len, "%i %s %i", gindex + 1, N_OF_N_STR, gtotal);
    mvwprintw(statusw, y++, 1, "%*s %-*s", 7, STATUS_GAME_STR, w, buf);

    *tmp = '\0';
    p = tmp;

    if (config.details) {
	*p++ = 'D';
	i++;
    }

    if (TEST_FLAG(d->flags, CF_DELETE)) {
	if (i)
	    *p++ = '/';

	*p++ = 'X';
	i++;
    }

    if (TEST_FLAG(g->flags, GF_PERROR)) {
	if (i)
	    *p++ = '/';

	*p++ = '!';
	i++;
    }

    if (TEST_FLAG(d->flags, CF_MODIFIED)) {
	if (i)
	    *p++ = '/';

	*p++ = '*';
	i++;
    }

    pgn_config_get(PGN_STRICT_CASTLING, &n);

    if (n == 1) {
	if (i)
	    *p++ = '/';

	*p++ = 'C';
	i++;
    }

    *p = '\0';
    mvwprintw(statusw, y++, 1, "%*s %-*s", 7, STATUS_FLAGS_STR, w, (tmp[0]) ? tmp : "-");

    switch (d->mode) {
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

    snprintf(buf, len - 1, "%*s %s", 7, STATUS_MODE_STR, mode);

    if (d->mode == MODE_PLAY) {
	if (TEST_FLAG(d->flags, CF_HUMAN))
	    strncat(buf, " (human/human)", len - 1);
	else if (TEST_FLAG(d->flags, CF_ENGINE_LOOP))
	    strncat(buf, " (engine/engine)", len - 1);
	else
	    strncat(buf, " (human/engine)", len - 1);
    }

    mvwprintw(statusw, y++, 1, "%-*s", len, buf);

    if (d->engine) {
	switch (d->engine->status) {
	    case ENGINE_THINKING:
		engine = ENGINE_PONDER_STR;
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
    }
    else
	engine = ENGINE_OFFLINE_STR;

    mvwprintw(statusw, y, 1, "%*s %-*s", 7, STATUS_ENGINE_STR, w, " ");
    wattron(statusw, CP_STATUS_ENGINE);
    mvwaddstr(statusw, y++, 9, engine);
    wattroff(statusw, CP_STATUS_ENGINE);

    mvwprintw(statusw, y++, 1, "%*s %-*s", 7, STATUS_TURN_STR, w,
	    (g->turn == WHITE) ? WHITE_STR : BLACK_STR);

    strncpy(tmp, WHITE_STR, sizeof(tmp));
    tmp[0] = toupper(tmp[0]);
    mvwprintw(statusw, y++, 1, "%*s: %-*s", 6, tmp, w, timeval_to_char(d->wc, d->limit));

    strncpy(tmp, BLACK_STR, sizeof(tmp));
    tmp[0] = toupper(tmp[0]);
    mvwprintw(statusw, y++, 1, "%*s: %-*s", 6, tmp, w, timeval_to_char(d->bc, d->limit));
    free(buf);

    mvwprintw(statusw, y++, 1, "%*s %-*s", 7, STATUS_CLOCK_STR, w, 
	    clock_to_char((TEST_FLAG(d->flags, CF_CLOCK)) ?
		    d->elapsed : 0));

    for (i = 0; i < STATUS_WIDTH; i++)
	mvwprintw(stdscr, STATUS_HEIGHT, i, " ");

    if (!status.notify)
	status.notify = strdup(GAME_HELP_PROMPT);

    wattron(stdscr, CP_STATUS_NOTIFY);
    mvwprintw(stdscr, STATUS_HEIGHT, CENTERX(STATUS_WIDTH, status.notify), "%s",
	    status.notify);
    wattroff(stdscr, CP_STATUS_NOTIFY);
}

void update_history_window(GAME g)
{
    char buf[HISTORY_WIDTH - 1];
    HISTORY *h = NULL;
    int n, total;
    int t = pgn_history_total(g->hp);

    n = (g->hindex + 1) / 2;

    if (t % 2)
	total = (t + 1) / 2;
    else
	total = t / 2;

    if (t)
	snprintf(buf, sizeof(buf), "%u %s %u%s", n, N_OF_N_STR, total,
		(movestep == 1) ? HISTORY_PLY_STEP : "");
    else
	strncpy(buf, UNAVAILABLE, sizeof(buf));

    mvwprintw(historyw, 2, 1, "%*s %-*s", 10, HISTORY_MOVE_STR,
	    HISTORY_WIDTH - 13, buf);

    h = pgn_history_by_n(g->hp, g->hindex);
    snprintf(buf, sizeof(buf), "%s", (h && h->move) ? h->move : UNAVAILABLE);
    n = 0;

    if (h && ((h->comment) || h->nag[0])) {
	strncat(buf, " (Annotated", sizeof(buf));
	n++;
    }

    if (h && h->rav) {
	strncat(buf, (n) ? ",+" : " (+", sizeof(buf));
	n++;
    }

    if (g->ravlevel) {
	strncat(buf, (n) ? ",-" : " (-", sizeof(buf));
	n++;
    }

    if (n)
	strncat(buf, ")", sizeof(buf));

    mvwprintw(historyw, 3, 1, "%s %-*s", HISTORY_MOVE_NEXT_STR,
	    HISTORY_WIDTH - 13, buf);

    h = pgn_history_by_n(g->hp, g->hindex - 1);
    snprintf(buf, sizeof(buf), "%s", (h && h->move) ? h->move : UNAVAILABLE);
    n = 0;

    if (h && ((h->comment) || h->nag[0])) {
	strncat(buf, " (Annotated", sizeof(buf));
	n++;
    }

    if (h && h->rav) {
	strncat(buf, (n) ? ",+" : " (+", sizeof(buf));
	n++;
    }

    if (g->ravlevel) {
	strncat(buf, (n) ? ",-" : " (-", sizeof(buf));
	n++;
    }

    if (n)
	strncat(buf, ")", sizeof(buf));

    mvwprintw(historyw, 4, 1, "%s %-*s", HISTORY_MOVE_PREV_STR,
	    HISTORY_WIDTH - 13, buf);
}

void update_tag_window(TAG **t)
{
    int i, l, w;
    int namel = 0, valuel = 0;

    for (i = 0; t[i]; i++) {
	l = strlen(t[i]->name);

	if (l > namel)
	    namel = l;

	l = strlen(t[i]->value);

	if (l > valuel)
	    valuel = l;
    }

    w = TAG_WIDTH - namel - 4;

    for (i = 0; t[i] && i < TAG_HEIGHT - 3; i++)
	mvwprintw(tagw, (i + 2), 1, "%*s: %-*s", namel, t[i]->name, w,
		str_etc(t[i]->value, w, 0));

    for (; i < TAG_HEIGHT - 3; i++)
	mvwprintw(tagw, (i + 2), 1, "%*s", namel + w + 2, " ");
}

void append_enginebuf(GAME g, char *line)
{
    int i = 0;
    struct userdata_s *d = g->data;

    if (d->engine->enginebuf)
	for (i = 0; d->engine->enginebuf[i]; i++);

    if (i >= LINES - 3) {
	free(d->engine->enginebuf[0]);

	for (i = 0; d->engine->enginebuf[i+1]; i++)
	    d->engine->enginebuf[i] = d->engine->enginebuf[i+1];

	d->engine->enginebuf[i] = strdup(line);
    }
    else {
	d->engine->enginebuf = Realloc(d->engine->enginebuf, (i + 2) * sizeof(char *));
	d->engine->enginebuf[i++] = strdup(line);
	d->engine->enginebuf[i] = NULL;
    }
}

void update_engine_window(GAME g)
{
    int i;
    struct userdata_s *d = g->data;

    if (!d->engine || !d->engine->enginebuf)
	return;

    wmove(enginew, 0, 0);
    wclrtobot(enginew);

    if (d->engine->enginebuf) {
	for (i = 0; d->engine->enginebuf[i]; i++)
	    mvwprintw(enginew, i + 2, 1, "%s", d->engine->enginebuf[i]);
    }

    window_draw_title(enginew, ENGINE_IO_TITLE, COLS, CP_MESSAGE_TITLE,
	    CP_MESSAGE_BORDER);
}

void refresh_all()
{
    wmove(stdscr, 0, 0);
    wclrtobot(stdscr);
    update_status_window(gp);
    update_panels();
    doupdate();
}

void update_all(GAME g)
{
    update_status_window(g);
    update_history_window(g);
    update_tag_window(g->tag);
    update_engine_window(g);
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
}

static void delete_game(int which)
{
    GAME *g = NULL;
    int gi = 0;
    int i;
    struct userdata_s *d;

    for (i = 0; i < gtotal; i++) {
	d = game[i]->data;

	if (i == which || TEST_FLAG(d->flags, CF_DELETE)) {
	    free_userdata_once(game[i]);
	    pgn_free(game[i]);
	    continue;
	}

	g = Realloc(g, (gi + 1) * sizeof(GAME *));
	g[gi] = Calloc(1, sizeof(struct game_s));
	memcpy(g[gi], game[i], sizeof(struct game_s));
	g[gi]->tag = game[i]->tag;
	g[gi]->history = game[i]->history;
	g[gi]->hp = game[i]->hp;
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

    gp = game[gindex];
    gp->hp = gp->history;
}

/*
 * FIXME find across multiple games.
 */
static int find_move_exp(GAME g, regex_t r, int which, int count)
{
    int i;
    int ret;
    char errbuf[255];
    int incr;
    int found;

    incr = (which == 0) ? -1 : 1;

    for (i = g->hindex + incr - 1, found = 0; ; i += incr) {
	if (i == g->hindex - 1)
	    break;

	if (i >= pgn_history_total(g->hp))
	    i = 0;
	else if (i < 0)
	    i = pgn_history_total(g->hp) - 1;

	// FIXME RAV
	ret = regexec(&r, g->hp[i]->move, 0, 0, 0);

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
    struct userdata_s *d = game[n]->data;

    TOGGLE_FLAG(d->flags, CF_DELETE);
    gindex = n;
    update_all(gp);

    for (i = x = 0; i < gtotal; i++) {
	d = game[i]->data;

	if (TEST_FLAG(d->flags, CF_DELETE))
	    x++;
    }

    if (x == gtotal) {
	cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
	d = game[n]->data;
	CLEAR_FLAG(d->flags, CF_DELETE);
	return 1;
    }

    return 0;
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

	for (t = 0; game[g]->tag[t]; t++) {
	    if (nstr) {
		if (regexec(&nexp, game[g]->tag[t]->name, 0, 0, 0) == 0) {
		    if (regexec(&vexp, game[g]->tag[t]->value, 0, 0, 0) == 0) {
			if (count == ++found) {
			    ret = g;
			    goto cleanup;
			}
		    }
		}
	    }
	    else {
		if (regexec(&vexp, game[g]->tag[t]->value, 0, 0, 0) == 0) {
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

/*
 * Updates the notification line in the status window then refreshes the
 * status window.
 */
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

int rav_next_prev(GAME g, BOARD b, int n)
{
    // Next RAV.
    if (n) {
	if ((!g->ravlevel && g->hindex && g->hp[g->hindex - 1]->rav == NULL) ||
		(!g->ravlevel && !g->hindex && g->hp[g->hindex]->rav == NULL) ||
		(g->ravlevel && g->hp[g->hindex]->rav == NULL))
	    return 1;

	g->rav = Realloc(g->rav, (g->ravlevel + 1) * sizeof(RAV));
	g->rav[g->ravlevel].hp = g->hp;
	g->rav[g->ravlevel].flags = g->flags;
	g->rav[g->ravlevel].fen = strdup(pgn_game_to_fen(g, b));
	g->rav[g->ravlevel].hindex = g->hindex;
	g->hp = (!g->ravlevel) ? (g->hindex) ? g->hp[g->hindex - 1]->rav : g->hp[g->hindex]->rav : g->hp[g->hindex]->rav;
	g->hindex = 0;
	g->ravlevel++;
	pgn_board_update(g, b, g->hindex + 1);
	return 0;
    }

    if (g->ravlevel - 1 < 0)
	return 1;

    // Previous RAV.
    g->ravlevel--;
    pgn_board_init_fen(g, b, g->rav[g->ravlevel].fen);
    free(g->rav[g->ravlevel].fen);
    g->hp = g->rav[g->ravlevel].hp;
    g->flags = g->rav[g->ravlevel].flags;
    g->hindex = g->rav[g->ravlevel].hindex;
    return 0;
}

static void draw_window_decor()
{
    move_panel(historyp, LINES - HISTORY_HEIGHT, COLS - HISTORY_WIDTH);
    move_panel(boardp, 0, COLS - BOARD_WIDTH);
    move_panel(statusp, 0, 0);
    wbkgd(boardw, CP_BOARD_WINDOW);
    wbkgd(statusw, CP_STATUS_WINDOW);
    window_draw_title(statusw, STATUS_WINDOW_TITLE, STATUS_WIDTH,
	    CP_STATUS_TITLE, CP_STATUS_BORDER);
    wbkgd(tagw, CP_TAG_WINDOW);
    window_draw_title(tagw, TAG_WINDOW_TITLE, TAG_WIDTH, CP_TAG_TITLE, 
	    CP_TAG_BORDER);
    wbkgd(historyw, CP_HISTORY_WINDOW);
    window_draw_title(historyw, HISTORY_WINDOW_TITLE, HISTORY_WIDTH,
	    CP_HISTORY_TITLE, CP_HISTORY_BORDER);
}

#ifdef HAVE_WRESIZE
static void do_window_resize()
{
    if (LINES < 24 || COLS < 80)
	return;

    resizeterm(LINES, COLS);
    wresize(historyw, HISTORY_HEIGHT, HISTORY_WIDTH);
    wresize(statusw, STATUS_HEIGHT, STATUS_WIDTH);
    wresize(tagw, TAG_HEIGHT, TAG_WIDTH);
    wmove(historyw, 0, 0);
    wclrtobot(historyw);
    wmove(tagw, 0, 0);
    wclrtobot(tagw);
    wmove(statusw, 0, 0);
    wclrtobot(statusw);
    draw_window_decor();
    update_all(gp);
}
#endif

void stop_clock()
{
    memset(&clock_timer, 0, sizeof(struct itimerval));
    setitimer(ITIMER_REAL, &clock_timer, NULL);
}

void start_clock()
{
    if (clock_timer.it_interval.tv_usec)
	return;

    clock_timer.it_value.tv_sec = 0;
    clock_timer.it_value.tv_usec = 100000;
    clock_timer.it_interval.tv_sec = 0;
    clock_timer.it_interval.tv_usec = 100000;
    setitimer(ITIMER_REAL, &clock_timer, NULL);
}

static void update_clocks()
{
    int i;
    struct userdata_s *d;
    struct itimerval it;

    getitimer(ITIMER_REAL, &it);

    for (i = 0; i < gtotal; i++) {
	d = game[i]->data;

	if (d && d->mode == MODE_PLAY && TEST_FLAG(d->flags, CF_CLOCK)) {
	    if (d->paused == 1 || TEST_FLAG(d->flags, CF_NEW))
		continue;
	    else if (d->paused == -1) {
		if (game[i]->side == game[i]->turn) {
		    d->paused = 1;
		    continue;
		}
	    }

	    update_clock(game[i], it);
	}
    }
}

static int parse_clock_input(struct userdata_s *d, char *str)
{
    char *p = str;
    long n = 0;
    int t = 0;
    int plus = 0;

    while (isspace(*p))
	p++;

    if (!*p)
	return 0;

    if (*p == '+') {
	plus = 1;
	p++;
    }

    if (isdigit(*p)) {
	while (*p) {
	    if (isdigit(*p)) {
		t = atoi(p);

		while (isdigit(*p))
		    p++;

		continue;
	    }

	    if (!t && *p != ' ')
		return 1;

	    switch (*p) {
		case 'H':
		case 'h':
		    n += t * (60 * 60);
		    t = 0;
		    break;
		case 'M':
		case 'm':
		    n += t * 60;
		    t = 0;
		    break;
		case 'S':
		case 's':
		    n += t;
		    t = 0;
		    break;
		case ' ':
		    t = 0;
		    break;
		default:
		    return 1;
	    }

	    p++;
	}

	if (t)
	    n += t;

	if (!n) {
	    d->limit = 0;
	    CLEAR_FLAG(d->flags, CF_CLOCK);
	}
	else {
	    SET_FLAG(d->flags, CF_CLOCK);

	    if (plus)
		d->limit += n;
	    else
		d->limit = (n <= d->elapsed) ? d->elapsed + n : n;
	}

	return 0;
    }
    else if (toupper(*p++) == 'G') {
	if (strlen(p) < 2)
	    return 1;

	if (*p++ != '/')
	    return 1;

	if (!isinteger(p))
	    return 1;

	n = strtol(p, NULL, 10);

	if (n < 0)
	    return 1;
	else if (n == 0) {
	    d->limit = 0;
	    CLEAR_FLAG(d->flags, CF_CLOCK);
	    return 0;
	}

	SET_FLAG(d->flags, CF_CLOCK);
	d->limit = n * 60;
	d->elapsed = 0;
	memset(&d->wc, 0, sizeof(d->wc));
	memset(&d->bc, 0, sizeof(d->bc));
	return 0;
    }

    return 1;
}

void do_clock_input_finalize(WIN *win)
{
    struct userdata_s *d = gp->data;
    struct input_data_s *in = win->data;

    if (!in->str) {
	free(in);
	return;
    }

    if (parse_clock_input(d, in->str))
	cmessage(ERROR, ANYKEY, "Invalid time specification");

    free(in->str);
    free(in);
}

void do_engine_command_finalize(WIN *win)
{
    struct userdata_s *d = gp->data;
    struct input_data_s *in = win->data;
    int x;

    if (!in->str) {
	free(in);
	return;
    }

    if (!d->engine)
	goto done;

    x = d->engine->status;
    send_to_engine(gp, -1, "%s\n", in->str);
    d->engine->status = x;

done:
    free(in->str);
    free(in);
}

void do_board_details()
{
    config.details = (config.details) ? 0 : 1;
}

void do_toggle_strict_castling()
{
    int n;

    pgn_config_get(PGN_STRICT_CASTLING, &n);

    if (n == 0)
	pgn_config_set(PGN_STRICT_CASTLING, 1);
    else
	pgn_config_set(PGN_STRICT_CASTLING, 0);
}

void do_play_set_clock()
{
    struct input_data_s *in;

    in = Calloc(1, sizeof(struct input_data_s));
    in->efunc = do_clock_input_finalize;
    construct_input(CLOCK_TITLE, NULL, 1, 1, CLOCK_HELP, NULL, NULL, 0, in, -1);
}

void do_play_toggle_human()
{
    struct userdata_s *d = gp->data;

    TOGGLE_FLAG(d->flags, CF_HUMAN);

    if (!TEST_FLAG(d->flags, CF_HUMAN) && pgn_history_total(gp->hp)) {
	if (init_chess_engine(gp))
	    return;
    }

    CLEAR_FLAG(d->flags, CF_ENGINE_LOOP);

    if (d->engine)
	d->engine->status = ENGINE_READY;

    update_all(gp);
}

void do_play_toggle_engine()
{
    struct userdata_s *d = gp->data;

    TOGGLE_FLAG(d->flags, CF_ENGINE_LOOP);
    CLEAR_FLAG(d->flags, CF_HUMAN);

    if (d->engine && TEST_FLAG(d->flags, CF_ENGINE_LOOP)) {
	pgn_board_update(gp, d->b,
		pgn_history_total(gp->hp));
	add_engine_command(gp, ENGINE_READY, 
		"setboard %s\n", pgn_game_to_fen(gp, d->b));
    }

    update_all(gp);
}

/*
 * This will send a command to the engine skipping the command queue.
 */
void do_play_send_command()
{
    struct userdata_s *d = gp->data;
    struct input_data_s *in;

    if (!d->engine || d->engine->status == ENGINE_OFFLINE) {
	if (init_chess_engine(gp))
	    return;
    }

    in = Calloc(1, sizeof(struct input_data_s));
    in->efunc = do_engine_command_finalize;
    construct_input(ENGINE_CMD_TITLE, NULL, 1, 1, NULL, NULL, NULL, 0, in, -1);
}

void do_play_switch_turn()
{
    pgn_switch_side(gp);
    pgn_switch_turn(gp);
    add_engine_command(gp, -1, 
	    (gp->side == WHITE) ? "white\n" : "black\n");
    update_status_window(gp);
}

void do_play_undo()
{
    struct userdata_s *d = gp->data;

    if (!pgn_history_total(gp->hp))
	return;

    if (keycount) {
	if (gp->hindex - keycount < 0)
	    gp->hindex = 0;
	else
	    gp->hindex -= keycount * 2;
    }
    else {
	if (gp->hindex - 2 < 0)
	    gp->hindex = 0;
	else
	    gp->hindex -= 2;
    }

    pgn_history_free(gp->hp, gp->hindex);
    gp->hindex = pgn_history_total(gp->hp);
    pgn_board_update(gp, d->b, gp->hindex);

    if (d->engine && d->engine->status == ENGINE_READY) {
	add_engine_command(gp, ENGINE_READY, "setboard %s\n",
		pgn_game_to_fen(gp, d->b));
	d->engine->status = ENGINE_READY;
    }

    update_history_window(gp);
}

void do_play_toggle_pause()
{
    struct userdata_s *d = gp->data;

    if (!TEST_FLAG(d->flags, CF_HUMAN) && gp->turn != 
	    gp->side) {
	d->paused = -1;
	return;
    }

    d->paused = (d->paused) ? 0 : 1;
}

void do_play_go()
{
    struct userdata_s *d = gp->data;

    if (TEST_FLAG(d->flags, CF_HUMAN))
	return;

    add_engine_command(gp, ENGINE_THINKING, "go\n");
}

void do_play_config_command()
{
    int x, w;

    if (config.keys) {
	for (x = 0; config.keys[x]; x++) {
	    if (config.keys[x]->c == input_c) {
		switch (config.keys[x]->type) {
		    case KEY_DEFAULT:
			add_engine_command(gp, -1, "%s\n", 
				config.keys[x]->str);
			break;
		    case KEY_SET:
			if (!keycount)
			    break;

			add_engine_command(gp, -1, 
				"%s %i\n", config.keys[x]->str, keycount);
			keycount = 0;
			break;
		    case KEY_REPEAT:
			if (!keycount)
			    break;

			for (w = 0; w < keycount; w++)
			    add_engine_command(gp, -1,
				    "%s\n", config.keys[x]->str);
			keycount = 0;
			break;
		}
	    }
	}
    }

    update_status_notify(gp, NULL);
}

void do_play_cancel_selected()
{
    struct userdata_s *d = gp->data;

    d->sp.icon = d->sp.srow = d->sp.scol = 0;
    keycount = 0;
    update_status_notify(gp, NULL);
}

void do_play_commit()
{
    struct userdata_s *d = gp->data;

    pushkey = keycount = 0;
    update_status_notify(gp, NULL);

    if (!TEST_FLAG(d->flags, CF_HUMAN) && 
	    (!d->engine || d->engine->status == ENGINE_THINKING))
	return;

    if (!d->sp.icon)
	return;

    d->sp.row = d->c_row;
    d->sp.col = d->c_col;
    move_to_engine(gp);
}

void do_play_select()
{
    struct userdata_s *d = gp->data;

    if (!TEST_FLAG(d->flags, CF_HUMAN) && (!d->engine ||
		d->engine->status == ENGINE_OFFLINE)) {
	if (init_chess_engine(gp))
	    return;
    }

    if (d->sp.icon || (d->engine && d->engine->status == ENGINE_THINKING))
	return;

    d->sp.icon = d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].icon;

    if (pgn_piece_to_int(d->sp.icon) == OPEN_SQUARE) {
	d->sp.icon = 0;
	return;
    }

    if (((islower(d->sp.icon) && gp->turn != BLACK)
		|| (isupper(d->sp.icon) && gp->turn != WHITE))) {
	message(NULL, ANYKEY, "%s", E_SELECT_TURN);
	d->sp.icon = 0;
	return;
#if 0
	if (pgn_history_total(gp->hp)) {
	    message(NULL, ANYKEY, "%s", E_SELECT_TURN);
	    d->sp.icon = 0;
	    return;
	}
	else {
	    if (pgn_tag_find(gp->tag, "FEN") != E_PGN_ERR)
		return;

	    add_engine_command(gp, ENGINE_READY, "black\n");
	    pgn_switch_turn(gp);

	    if (gp->side != BLACK)
		pgn_switch_side(gp);
	}
#endif
    }

    d->sp.srow = d->c_row;
    d->sp.scol = d->c_col;

    if (config.validmoves)
	pgn_find_valid_moves(gp, d->b, d->sp.scol, d->sp.srow);

    CLEAR_FLAG(d->flags, CF_NEW);
    start_clock();
}

/* FIXME: keys with the same function should comma deliminated. */
static char *build_help(struct key_s **keys)
{
    int i, nlen = 1, len, t, n;
    char *buf = NULL;
    char *p;

    if (!keys)
	return NULL;

    for (i = len = t = 0; keys[i]; i++) {
	if (!keys[i]->d)
	    continue;

	if (keys[i]->key) {
	    if (strlen(keys[i]->key) > nlen) {
		nlen = strlen(keys[i]->key);
		t += nlen;
	    }
	    else
		t++;
	}

	if (keys[i]->d) {
	    if (strlen(keys[i]->d) > len)
		len = strlen(keys[i]->d);
	}

	t += len;
	t += keys[i]->r;
    }

    t += 4 + i;
    buf = Malloc(t);
    p = buf;

    for (i = 0; keys[i]; i++) {
	if (!keys[i]->d)
	    continue;

	if (keys[i]->key)
	    n = strlen(keys[i]->key);
	else
	    n = 1;

	while (n++ <= nlen)
	    *p++ = ' ';

	*p = 0;

	if (keys[i]->key) {
	    strcat(buf, keys[i]->key);
	    p = buf + strlen(buf);
	}
	else
	    *p++ = keys[i]->c;

	*p++ = ' ';
	*p++ = '-';
	*p++ = ' ';
	*p = 0;

	if (keys[i]->d)
	    strcat(buf, keys[i]->d);

	if (keys[i]->r)
	    strcat(buf, "*");

	strcat(buf, "\n");
	p = buf + strlen(buf);
    }

    return buf;
}

void do_more_help(WIN *);
void do_main_help(WIN *win)
{
    char *buf;

    switch (win->c) {
	case 'p':
	    buf = build_help(play_keys);
	    construct_message(GAME_HELP_PLAY_TITLE, ANYKEY, 0, 0,
		    NULL, NULL, buf, do_more_help, 0, 1, "%s", buf);
	    break;
	case 'h':
	    buf = build_help(history_keys);
	    construct_message(GAME_HELP_HISTORY_TITLE, ANYKEY, 0, 0,
		    NULL, NULL, buf, do_more_help, 0, 1, "%s", buf);
	    break;
	case 'e':
	    buf = build_help(edit_keys);
	    construct_message(GAME_HELP_EDIT_TITLE, ANYKEY, 0, 0,
		    NULL, NULL, buf, do_more_help, 0, 1, "%s", buf);
	    break;
	case 'g':
	    buf = build_help(global_keys);
	    construct_message(GAME_HELP_GAME_TITLE, ANYKEY, 0, 0,
		    NULL, NULL, buf, do_more_help, 0, 1, "%s", buf);
	    break;
	default:
	    break;
    }
}

void do_more_help(WIN *win)
{
    if (win->c == KEY_F(1) || win->c == CTRL('g'))
	construct_message(GAME_HELP_INDEX_TITLE, GAME_HELP_INDEX_PROMPT, 0, 0, 
		NULL, NULL, NULL, do_main_help, 0, 0, "%s", mainhelp);
}

void do_play_help()
{
    char *buf = build_help(play_keys);

    construct_message(GAME_HELP_PLAY_TITLE, ANYKEY, 0, 0, NULL, NULL, buf, 
	    do_more_help, 0, 1, "%s", buf);
}

void do_play_history_mode()
{
    struct userdata_s *d = gp->data;

    if (!pgn_history_total(gp->hp) || 
	    (d->engine && d->engine->status == ENGINE_THINKING))
	return;

    d->mode = MODE_HISTORY;
    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
    update_all(gp);
}

void do_play_edit_mode()
{
    struct userdata_s *d = gp->data;

    if (pgn_history_total(gp->hp))
	return;

    pgn_board_init_fen(gp, d->b, NULL);
    config.details++;
    d->mode = MODE_EDIT;
    update_all(gp);
}

void do_edit_insert_finalize(WIN *win)
{
    struct userdata_s *d = win->data;

    if (pgn_piece_to_int(win->c) == -1)
	return;

    d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].icon = win->c;
}

void do_edit_select()
{
    struct userdata_s *d = gp->data;

    if (d->sp.icon)
	return;

    d->sp.icon = d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].icon;

    if (pgn_piece_to_int(d->sp.icon) == OPEN_SQUARE) {
	d->sp.icon = 0;
	return;
    }

    d->sp.srow = d->c_row;
    d->sp.scol = d->c_col;
}

void do_edit_commit()
{
    int p;
    struct userdata_s *d = gp->data;

    pushkey = keycount = 0;
    update_status_notify(gp, NULL);

    if (!d->sp.icon)
	return;

    d->sp.row = d->c_row;
    d->sp.col = d->c_col;
    p = d->b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon;
    d->b[RANKTOBOARD(d->sp.row)][FILETOBOARD(d->sp.col)].icon = p;
    d->b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon =
	pgn_int_to_piece(gp->turn, OPEN_SQUARE);
    d->sp.icon = d->sp.srow = d->sp.scol = 0;
}

void do_edit_delete()
{
    struct userdata_s *d = gp->data;

    if (d->sp.icon)
	d->b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon = 
	    pgn_int_to_piece(gp->turn, OPEN_SQUARE);
    else
	d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].icon =
	    pgn_int_to_piece(gp->turn, OPEN_SQUARE);

    d->sp.icon = d->sp.srow = d->sp.scol = 0;
}

void do_edit_cancel_selected()
{
    struct userdata_s *d = gp->data;

    d->sp.icon = d->sp.srow = d->sp.scol = 0;
    keycount = 0;
    update_status_notify(gp, NULL);
}

void do_edit_switch_turn()
{
    pgn_switch_turn(gp);
    update_all(gp);
}

void do_edit_toggle_castle()
{
    struct userdata_s *d = gp->data;

    castling_state(gp, d->b, RANKTOBOARD(d->c_row), 
	    FILETOBOARD(d->c_col),
	    d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].icon, 1);
}

void do_edit_insert()
{
    struct userdata_s *d = gp->data;

    construct_message(GAME_EDIT_TITLE, GAME_EDIT_PROMPT, 0, 0, NULL, NULL,
	    d->b, do_edit_insert_finalize, 0, 0, "%s", GAME_EDIT_TEXT);
}

void do_edit_enpassant()
{
    struct userdata_s *d = gp->data;

    if (d->c_row == 6 || d->c_row == 3) {
	pgn_reset_enpassant(d->b);
	d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].enpassant = 1;
    }
}

void do_edit_help()
{
    char *buf = build_help(edit_keys);

    construct_message(GAME_HELP_EDIT_TITLE, ANYKEY, 0, 0, NULL, NULL, buf, 
	    do_more_help, 0, 1, "%s", buf);
}

void do_edit_exit()
{
    struct userdata_s *d = gp->data;

    config.details--;
    pgn_tag_add(&gp->tag, "FEN", pgn_game_to_fen(gp, d->b));
    pgn_tag_add(&gp->tag, "SetUp", "1");
    pgn_tag_sort(gp->tag);
    pgn_board_update(gp, d->b, gp->hindex);
    d->mode = MODE_PLAY;
    update_all(gp);
}

void really_do_annotate_finalize(struct input_data_s *in, 
	struct userdata_s *d)
{
    HISTORY *h = in->data;
    int len;

    if (!in->str) {
	if (h->comment) {
	    free(h->comment);
	    h->comment = NULL;
	}
    }
    else {
	len = strlen(in->str) + 1;
	h->comment = Realloc(h->comment, len);
	strncpy(h->comment, in->str, len);
    }

    free(in->str);
    free(in);
    SET_FLAG(d->flags, CF_MODIFIED);
    update_all(gp);
}

void do_annotate_finalize(WIN *win)
{
    struct userdata_s *d = gp->data;
    struct input_data_s *in = win->data;

    really_do_annotate_finalize(in, d);
}

void do_find_move_exp_finalize(int init, int which)
{
    int n;
    struct userdata_s *d = gp->data;
    static int firstrun;
    static regex_t r;
    int ret;
    char errbuf[255];

    if (init || !firstrun) {
	if (!firstrun)
	    regfree(&r);

	if ((ret = regcomp(&r, moveexp, REG_EXTENDED|REG_NOSUB)) != 0) {
	    regerror(ret, &r, errbuf, sizeof(errbuf));
	    cmessage(E_REGCOMP_TITLE, ANYKEY, "%s", errbuf);
	    return;
	}

	firstrun = 1;
    }

    if ((n = find_move_exp(gp, r,
		    (which == -1) ? 0 : 1, (keycount) ? keycount : 1)) == -1)
	return;

    gp->hindex = n;
    pgn_board_update(gp, d->b, gp->hindex);
    update_all(gp);
}

void do_find_move_exp(WIN *win)
{
    struct input_data_s *in = win->data;
    int *n = in->data;
    int which = *n;

    if (in->str) {
	strncpy(moveexp, in->str, sizeof(moveexp));
	do_find_move_exp_finalize(1, which);
	free(in->str);
    }

    free(in->data);
    free(in);
}

void do_move_jump_finalize(int n)
{
    struct userdata_s *d = gp->data;

    if (n < 0 || n > (pgn_history_total(gp->hp) / 2))
	return;

    keycount = 0;
    update_status_notify(gp, NULL);
    gp->hindex = (n) ? n * 2 - 1 : n * 2;
    pgn_board_update(gp, d->b, gp->hindex);
    update_all(gp);
}

void do_move_jump(WIN *win)
{
    struct input_data_s *in = win->data;

    if (!in->str || !isinteger(in->str)) {
	if (in->str)
	    free(in->str);

	free(in);
	return;
    }

    do_move_jump_finalize(atoi(in->str));
    free(in->str);
    free(in);
}

struct history_menu_s {
    char *line;
    int hindex;
    int ravlevel;
    int move;
    int indent;
};

void free_history_menu_data(struct history_menu_s **h)
{
    int i;
    
    if (!h)
	return;

    for (i = 0; h[i]; i++) {
	free(h[i]->line);
	free(h[i]);
    }

    free(h);
}

void get_history_data(HISTORY **hp, struct history_menu_s ***menu, int m,
	int turn)
{
    int i, n = 0;
    int t = pgn_history_total(hp);
    char buf[MAX_SAN_MOVE_LEN + 4];
    static int depth;
    struct history_menu_s **hmenu = *menu;

    if (hmenu)
	for (n = 0; hmenu[n]; n++);
    else
	depth = 0;

    for (i = 0; i < t; i++) {
	hmenu = Realloc(hmenu, (n + 2) * sizeof(struct history_menu_s *));
	hmenu[n] = Malloc(sizeof(struct history_menu_s));
	snprintf(buf, sizeof(buf), "%c%s%s", (turn == WHITE) ? 'W' : 'B',
		hp[i]->move, (hp[i]->comment || hp[i]->nag[0]) ? " !" : "");
	hmenu[n]->line = strdup(buf);
	hmenu[n]->hindex = i;
	hmenu[n]->indent = 0;
	hmenu[n]->ravlevel = depth;
	hmenu[n]->move = (n && depth > hmenu[n-1]->ravlevel) ? m++ : m;
	n++;
	hmenu[n] = NULL;
	
#if 0
	if (hp[i]->rav) {
	    depth++;
	    get_history_data(hp[i]->rav, &hmenu, m, turn);
	    for (n = 0; hmenu[n]; n++);
	    depth--;

	    if (depth)
		m--;
	}
#endif

	turn = (turn == WHITE) ? BLACK : WHITE;
    }

    *menu = hmenu;
}

void history_draw_update(struct menu_input_s *m)
{
    GAME g = m->data;
    struct userdata_s *d = g->data;

    g->hindex = m->selected + 1;
    update_cursor(g, m->selected);
    pgn_board_update(g, d->b, m->selected + 1);
}

struct menu_item_s **get_history_items(WIN *win)
{
    struct menu_input_s *m = win->data;
    GAME g = m->data;
    struct userdata_s *d = g->data;
    struct history_menu_s **hm = d->data;
    struct menu_item_s **items = m->items;
    int i;

    if (!hm) {
	get_history_data(g->history, &hm, 0,
		TEST_FLAG(g->flags, GF_BLACK_OPENING));
	m->selected = g->hindex - 1;

	if (m->selected < 0)
	    m->selected = 0;

	m->draw_exit_func = history_draw_update;
    }

    d->data = hm;

    if (items) {
	for (i = 0; items[i]; i++)
	    free(items[i]);

	free(items);
	items = NULL;
    }

    for (i = 0; hm[i]; i++) {
	items = Realloc(items, (i+2) * sizeof(struct menu_item_s *));
	items[i] = Malloc(sizeof(struct menu_item_s));
	items[i]->name = hm[i]->line;
	items[i]->value = NULL;
	items[i]->selected = 0;
    }

    if (items)
	items[i] = NULL;

    m->nofree = 1;
    m->items = items;
    return items;
}

void history_menu_quit(struct menu_input_s *m)
{
    pushkey = -1;
}

void history_menu_exit(WIN *win)
{
    GAME g = win->data;
    struct userdata_s *d = g->data;
    struct history_menu_s **hm = d->data;
    int i;

    if (!hm)
	return;

    for (i = 0; hm[i]; i++) {
	free(hm[i]->line);
	free(hm[i]);
    }

    free(hm);
    d->data = NULL;
}

// FIXME RAV
void history_menu_next(struct menu_input_s *m)
{
    GAME g = m->data;
    struct userdata_s *d = g->data;
    struct history_menu_s **hm = d->data;
    int n, t;

    for (t = 0; hm[t]; t++);

    if (m->selected + 1 == t)
	n = 0;
    else
	n = hm[m->selected + 1]->hindex;
    
    n++;
    g->hindex = n;
}

// FIXME RAV
void history_menu_prev(struct menu_input_s *m)
{
    GAME g = m->data;
    struct userdata_s *d = g->data;
    struct history_menu_s **hm = d->data;
    int n, t;

    for (t = 0; hm[t]; t++);

    if (m->selected - 1 < 0)
	n = t - 1;
    else
	n = hm[m->selected - 1]->hindex;

    n++;
    g->hindex = n;
}

void history_menu_help(struct menu_input_s *m)
{
    message("History Menu Help", ANYKEY, "%s", history_menu_help_str);
}

void do_annotate_move(HISTORY *hp)
{
    char buf[COLS - 4];
    struct input_data_s *in;

    snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_EDIT_TITLE, hp->move);
    in = Calloc(1, sizeof(struct input_data_s));
    in->data = hp;
    in->efunc = do_annotate_finalize;
    construct_input(buf, hp->comment, MAX_PGN_LINE_LEN / INPUT_WIDTH, 0, 
	    NAG_PROMPT, edit_nag, NULL, CTRL('T'), in, -1);
}

void history_menu_view_annotation(struct menu_input_s *m)
{
    GAME g = m->data;

    // FIXME RAV
    view_annotation(g->history[m->selected]);
}

void history_menu_annotate_finalize(WIN *win)
{
    struct input_data_s *in = win->data;
    GAME g = in->moredata;
    struct userdata_s *d = g->data;
    struct history_menu_s **hm = d->data;

    really_do_annotate_finalize(in, d);
    free_history_menu_data(hm);
    hm = NULL;
    get_history_data(g->history, &hm, 0, TEST_FLAG(g->flags, GF_BLACK_OPENING));
    d->data = hm;
    pushkey = REFRESH_MENU;
}

void history_menu_annotate(struct menu_input_s *m)
{
    GAME g = m->data;
    char buf[COLS - 4];
    struct input_data_s *in;
    HISTORY *hp = g->history[m->selected]; // FIXME RAV

    snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_EDIT_TITLE, hp->move);
    in = Calloc(1, sizeof(struct input_data_s));
    in->data = hp;
    in->moredata = m->data;
    in->efunc = history_menu_annotate_finalize;
    construct_input(buf, hp->comment, MAX_PGN_LINE_LEN / INPUT_WIDTH, 0, 
	    NAG_PROMPT, edit_nag, NULL, CTRL('T'), in, -1);
}

void history_menu_details(struct menu_input_s *m)
{
    do_board_details();
}

// FIXME RAV
void history_menu_print(WIN *win)
{
    struct menu_input_s *m = win->data;
    GAME g = m->data;
    struct userdata_s *d = g->data;
    struct history_menu_s **hm = d->data;
    struct history_menu_s *h = hm[m->top];
    int i;
    char *p = m->item->name;
    int line = m->print_line - 2;
/*
 * Solaris 5.9 doesn't have wattr_get() or any function that requires an
 * attr_t data type.
 */
#ifdef HAVE_ATTR_T
    attr_t attrs;
    short pair;
#endif
    int total;

    for (total = 0; hm[total]; total++);
#ifdef HAVE_ATTR_T
    wattr_get(win->w, &attrs, &pair, NULL);
    wattroff(win->w, COLOR_PAIR(pair));
#endif
    mvwaddch(win->w, m->print_line, 1, *p++);

    if (h->hindex == 0 && line == 0)
	waddch(win->w, ACS_ULCORNER | CP_HISTORY_MENU_LG);
    else if ((!hm[h->hindex + (win->rows - 5) + 1] && line == win->rows - 5) ||
	    (m->top + line == total - 1))
	waddch(win->w, ACS_LLCORNER | CP_HISTORY_MENU_LG);
    else if (hm[m->top + 1]->ravlevel != h->ravlevel || !h->ravlevel)
	waddch(win->w, ACS_LTEE | CP_HISTORY_MENU_LG);
    else
	waddch(win->w, ACS_VLINE | CP_HISTORY_MENU_LG);

#ifdef HAVE_ATTR_T
    wattron(win->w, COLOR_PAIR(pair) | attrs);
#endif

    for (i = 2; *p; p++, i++)
	waddch(win->w, (*p == '!') ? *p | A_BOLD : *p);

    while (i++ < win->cols - 2)
	waddch(win->w, ' ');
}

void history_menu(GAME g)
{
    struct menu_key_s **keys = NULL;

    add_menu_key(&keys, KEY_ESCAPE, history_menu_quit);
    add_menu_key(&keys, KEY_UP, history_menu_prev);
    add_menu_key(&keys, KEY_DOWN, history_menu_next);
    add_menu_key(&keys, KEY_F(1), history_menu_help);
    add_menu_key(&keys, CTRL('a'), history_menu_annotate);
    add_menu_key(&keys, CTRL('d'), history_menu_details);
    add_menu_key(&keys, '\n', history_menu_view_annotation);
    construct_menu(LINES, TAG_WIDTH, 0, 0, HISTORY_MENU_TITLE, 1, 
	    get_history_items, keys, g, history_menu_print, history_menu_exit);
}

void do_history_menu()
{
    history_menu(gp);
}

void do_history_half_move_toggle()
{
    movestep = (movestep == 1) ? 2 : 1;
    update_history_window(gp);
}

void do_history_jump_next()
{
    struct userdata_s *d = gp->data;

    pgn_history_next(gp, d->b, (keycount > 0) ?
	    config.jumpcount * keycount * movestep : 
	    config.jumpcount * movestep);
    update_all(gp);
}

void do_history_jump_prev()
{
    struct userdata_s *d = gp->data;

    pgn_history_prev(gp, d->b, (keycount) ?
	    config.jumpcount * keycount * movestep : 
	    config.jumpcount * movestep);
    update_all(gp);
}

void do_history_prev()
{
    struct userdata_s *d = gp->data;

    pgn_history_prev(gp, d->b,
	    (keycount) ? keycount * movestep : movestep);
    update_all(gp);
}

void do_history_next()
{
    struct userdata_s *d = gp->data;

    pgn_history_next(gp, d->b, (keycount) ? 
	    keycount * movestep : movestep);
    update_all(gp);
}

void do_history_mode_finalize(struct userdata_s *d)
{
    pushkey = 0;
    d->mode = MODE_PLAY;
    update_all(gp);
}

void do_history_mode_confirm(WIN *win)
{
    struct userdata_s *d = gp->data;

    switch (win->c) {
	case 'R':
	case 'r':
	    pgn_history_free(gp->hp, 
		    gp->hindex);
	    pgn_board_update(gp, d->b, 
		    pgn_history_total(gp->hp));
	    break;
#if 0
	case 'C':
	case 'c':
	    if (pgn_history_rav_new(gp, d->b,
			gp->hindex) != E_PGN_OK)
		return;

	    break;
#endif
	default:
	    return;
    }

    if (!TEST_FLAG(d->flags, CF_HUMAN))
	add_engine_command(gp, ENGINE_READY,
		"setboard %s\n", pgn_game_to_fen(gp, d->b));

    do_history_mode_finalize(d);
}

void do_history_toggle()
{
    struct userdata_s *d = gp->data;

    // FIXME Resuming from previous history could append to a RAV.
    if (gp->hindex != pgn_history_total(gp->hp)) {
	if (!pushkey)
	    construct_message(NULL, "(r)esume or abort", 0, 1, NULL, NULL, NULL, 
		    do_history_mode_confirm, 0, 0, "%s", 
		    GAME_RESUME_HISTORY_TEXT);

	return;
    }
    else {
	if (TEST_FLAG(gp->flags, GF_GAMEOVER))
	    return;
    }

    do_history_mode_finalize(d);
}

void do_history_annotate()
{
    int n = gp->hindex;

    if (n && gp->hp[n - 1]->move)
	n--;
    else
	return;

    do_annotate_move(gp->hp[n]);
}

void do_history_help()
{
    char *buf = build_help(history_keys);

    construct_message(GAME_HELP_HISTORY_TITLE, ANYKEY, 0, 0, NULL, NULL, buf, 
	    do_more_help, 0, 1, "%s", buf);
}

void do_history_find(int which)
{
    struct input_data_s *in;
    int *p;

    if (pgn_history_total(gp->hp) < 2)
	return;

    in = Calloc(1, sizeof(struct input_data_s));
    p = Malloc(sizeof(int));
    *p = which;
    in->data = p;
    in->efunc = do_find_move_exp;

    if (!*moveexp || which == 0) {
	construct_input(FIND_REGEXP, moveexp, 1, 0, NULL, NULL, NULL, 
		0, in, -1);
	return;
    }

    do_find_move_exp_finalize(0, which);
}

void do_history_find_new()
{
    do_history_find(0);
}

void do_history_find_prev()
{
    do_history_find(-1);
}

void do_history_find_next()
{
    do_history_find(1);
}

void do_history_rav(int which)
{
    struct userdata_s *d = gp->data;

    rav_next_prev(gp, d->b, which);
    update_all(gp);
}

void do_history_rav_next()
{
    do_history_rav(1);
}

void do_history_rav_prev()
{
    do_history_rav(0);
}

void do_history_jump()
{
    struct input_data_s *in;

    if (pgn_history_total(gp->hp) < 2)
	return;

    if (!keycount) {
	in = Calloc(1, sizeof(struct input_data_s));
	in->efunc = do_move_jump;

	construct_input(GAME_HISTORY_JUMP_TITLE, NULL, 1, 1, NULL, 
		NULL, NULL, 0, in, 0);
	return;
    }

    do_move_jump_finalize(keycount);
}

static void free_userdata_once(GAME g)
{
    struct userdata_s *d = g->data;

    if (!d)
	return;

    if (d->engine) {
	stop_engine(g);

	if (d->engine->enginebuf) {
	    int n;

	    for (n = 0; d->engine->enginebuf[n]; n++)
		free(d->engine->enginebuf[n]);

	    free(d->engine->enginebuf);
	}

	if (d->engine->queue) {
	    struct queue_s **q;

	    for (q = d->engine->queue; *q; q++)
		free(*q);

	    free(d->engine->queue);
	}

	free(d->engine);
    }

    free(d);
    g->data = NULL;
}

static void free_userdata()
{
    int i;

    for (i = 0; i < gtotal; i++) {
	free_userdata_once(game[i]);
	game[i]->data = NULL;
    }
}

void update_loading_window(int n)
{
    if (!loadingw) {
	loadingw = newwin(3, COLS / 2, CALCPOSY(3), CALCPOSX(COLS / 2));
	loadingp = new_panel(loadingw);
	wbkgd(loadingw, CP_MESSAGE_WINDOW);
    }

    wmove(loadingw, 0, 0);
    wclrtobot(loadingw);
    wattron(loadingw, CP_MESSAGE_BORDER);
    box(loadingw, ACS_VLINE, ACS_HLINE);
    wattroff(loadingw, CP_MESSAGE_BORDER);
    mvwprintw(loadingw, 1, CENTER_INT((COLS / 2),
		11 + strlen(itoa(gtotal))), "Loading... %i%% (%i games)", n, 
	    gtotal);
    refresh_all();
}

static void init_userdata_once(GAME g, int n)
{
    struct userdata_s *d = NULL;

    d = Calloc(1, sizeof(struct userdata_s));
    d->n = n;
    d->c_row = 2, d->c_col = 5;
    SET_FLAG(d->flags, CF_NEW);
    g->data = d;

    if (pgn_board_init_fen(g, d->b, NULL) != E_PGN_OK)
	pgn_board_init(d->b);
}

void init_userdata()
{
    int i;

    for (i = 0; i < gtotal; i++)
	init_userdata_once(game[i], i);
}

void fix_marks(int *start, int *end)
{
    int i;

    *start = (*start < 0) ? 0 : *start;
    *end = (*end < 0) ? 0 : *end;

    if (*start > *end) {
	i = *start;
	*start = *end;
        *end = i + 1;
    }

    *end = (*end > gtotal) ? gtotal : *end;
}

void do_new_game_finalize(GAME g)
{
    struct userdata_s *d = g->data;

    d->mode = MODE_PLAY;
    update_status_notify(g, NULL);
    update_all(g);
}

void do_new_game_from_scratch(WIN *win)
{
    if (tolower(win->c) != 'y')
	return;

    stop_clock();
    free_userdata();
    pgn_parse(NULL);
    add_custom_tags(&gp->tag);
    init_userdata();
    loadfile[0] = 0;
    do_new_game_finalize(gp);
}

void do_new_game()
{
    pgn_new_game();
    gp = game[gindex];
    add_custom_tags(&gp->tag);
    init_userdata_once(gp, gindex);
    do_new_game_finalize(gp);
}

void do_game_delete_finalize(int n)
{
    struct userdata_s *d;

    delete_game((!n) ? gindex : -1);
    d = gp->data;
    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
    update_all(gp);
}

void do_game_delete_confirm(WIN *win)
{
    int *n;

    if (tolower(win->c) != 'y') {
	free(win->data);
	return;
    }

    
    n = (int *)win->data;
    do_game_delete_finalize(*n);
    free(win->data);
}

void do_game_delete()
{
    char *tmp = NULL;
    int i, n;
    struct userdata_s *d;
    int *p;

    if (gtotal < 2) {
	cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
	return;
    }

    tmp = NULL;

    for (i = n = 0; i < gtotal; i++) {
	d = game[i]->data;

	if (TEST_FLAG(d->flags, CF_DELETE))
	    n++;
    }

    if (!n)
	tmp = GAME_DELETE_GAME_TEXT;
    else {
	if (n == gtotal) {
	    cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
	    return;
	}

	tmp = GAME_DELETE_ALL_TEXT;
    }

    if (config.deleteprompt) {
	p = Malloc(sizeof(int));
	*p = n;
	construct_message(NULL, YESNO, 1, 1, NULL, NULL, p,
		do_game_delete_confirm, 0, 0, tmp);
	return;
    }

    do_game_delete_finalize(n);
}

void do_find_game_exp_finalize(int which)
{
    struct userdata_s *d = gp->data;
    int n;

    if ((n = find_game_exp(gameexp, (which == -1) ? 0 : 1, 
		    (keycount) ? keycount : 1)) == -1)
	return;

    gindex = n;
    d = gp->data;

    if (pgn_history_total(gp->hp))
	d->mode = MODE_HISTORY;

    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
    update_all(gp);
}

void do_find_game_exp(WIN *win)
{
    struct input_data_s *in = win->data;
    int *n = in->data;
    int c = *n;

    if (in->str) {
	strncpy(gameexp, in->str, sizeof(gameexp));

	if (c == '?')
	    c = '}';

	do_find_game_exp_finalize(c);
	free(in->str);
    }

    free(in->data);
    free(in);
}

void do_game_jump_finalize(int n)
{
    struct userdata_s *d;

    if (--n > gtotal - 1 || n < 0)
	return;

    gindex = n;
    d = gp->data;
    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
    update_status_notify(gp, NULL);
    update_all(gp);
}

void do_game_jump(WIN *win)
{
    struct input_data_s *in = win->data;

    if (!in->str || !isinteger(in->str)) {
	if (in->str)
	    free(in->str);

	free(in);
	return;
    }

    do_game_jump_finalize(atoi(in->str));
    free(in->str);
    free(in);
}

void do_load_file(WIN *win)
{
    FILE *fp;
    struct input_data_s *in = win->data;
    char *tmp = in->str;
    struct userdata_s *d;

    if (!in->str) {
	free(in);
	return;
    }

    if ((tmp = pathfix(tmp)) == NULL)
	goto done;

    if ((fp = pgn_open(tmp)) == NULL) {
	cmessage(ERROR, ANYKEY, "%s\n%s", tmp, strerror(errno));
	goto done;
    }

    free_userdata();

    /*
     * FIXME what is the game state after a parse error?
     */
    if (pgn_parse(fp) == E_PGN_ERR) {
	del_panel(loadingp);
	delwin(loadingw);
	loadingw = NULL;
	loadingp = NULL;
	init_userdata();
	update_all(gp);
	goto done;
    }

    del_panel(loadingp);
    delwin(loadingw);
    loadingw = NULL;
    loadingp = NULL;
    init_userdata();
    strncpy(loadfile, tmp, sizeof(loadfile));
    gp = game[gindex];
    d = gp->data;

    if (pgn_history_total(gp->hp))
	d->mode = MODE_HISTORY;

    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
    update_all(gp);

done:
    if (in->str)
	free(in->str);

    free(in);
}

void do_game_save(WIN *win)
{
    struct input_data_s *in = win->data;
    int *x = in->data;
    int n = *x;
    char *tmp = in->str;
    char tfile[FILENAME_MAX];
    char *p;
    int i;
    struct userdata_s *d;

    if (!tmp || (tmp = pathfix(tmp)) == NULL)
	goto done;

    if (pgn_is_compressed(tmp)) {
	p = tmp + strlen(tmp) - 1;

	if (*p != 'n' || *(p-1) != 'g' || *(p-2) != 'p' ||
		*(p-3) != '.') {
	    snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
	    tmp = tfile;
	}
    }
    else {
	if ((p = strchr(tmp, '.')) != NULL) {
	    if (strcmp(p, ".pgn") != 0) {
		snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
		tmp = tfile;
	    }
	}
	else {
	    snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
	    tmp = tfile;
	}
    }

    /*
     * When in edit mode, update the FEN tag.
     */
    if (n == -1) {
	for (i = 0; i < gtotal; i++) {
	    d = game[i]->data;

	    if (d->mode == MODE_EDIT)
		pgn_tag_add(&game[i]->tag, "FEN", pgn_game_to_fen(game[i], d->b));
	}
    }
    else {
	d = game[n]->data;
	pgn_tag_add(&game[n]->tag, "FEN", pgn_game_to_fen(game[n], d->b));
    }

    save_pgn(tmp, n);

done:
    if (in->str)
	free(in->str);

    free(in->data);
    free(in);
}

void do_get_game_save_input(int n)
{
    struct input_data_s *in = Calloc(1, sizeof(struct input_data_s));
    int *p = Malloc(sizeof(int));

    in->efunc = do_game_save;
    *p = n;
    in->data = p;

    construct_input(GAME_SAVE_TITLE, loadfile, 1, 1, BROWSER_PROMPT,
	file_browser, NULL, '\t', in, -1);
}

void do_game_save_multi_confirm(WIN *win)
{
    int i;

    if (win->c == 'c')
	i = gindex;
    else if (win->c == 'a')
	i = -1;
    else {
	update_status_notify(gp, "%s", NOTIFY_SAVE_ABORTED);
	return;
    }

    do_get_game_save_input(i);
}

void do_global_about()
{
    cmessage("ABOUT", ANYKEY, "%s\nUsing %s with %i colors "
	    "and %i color pairs\n%s",
	    PACKAGE_STRING, curses_version(), COLORS, COLOR_PAIRS,
	    COPYRIGHT);
}

void global_game_next_prev(int which)
{
    struct userdata_s *d;

    game_next_prev(gp, (which == 1) ? 1 : 0,
	    (keycount) ? keycount : 1);
    d = gp->data;

    if (delete_count) {
	if (which == 1) {
	    markend = markstart + delete_count;
	    delete_count = 0;
	}
	else {
	    markend = markstart - delete_count + 1;
	    delete_count = -1; // to fix gindex in the other direction
	}

	fix_marks(&markstart, &markend);
	do_global_toggle_delete();
    }

    if (d->mode == MODE_HISTORY)
	pgn_board_update(gp, d->b, gp->hindex);
    else if (d->mode == MODE_PLAY)
	pgn_board_update(gp, d->b, pgn_history_total(gp->hp));

    update_status_notify(gp, NULL);
    update_all(gp);
}

void do_global_next_game()
{
    global_game_next_prev(1);
}

void do_global_prev_game()
{
    global_game_next_prev(0);
}

void global_find(int which)
{
    struct input_data_s *in;
    int *p;

    if (gtotal < 2)
	return;

    in = Calloc(1, sizeof(struct input_data_s));
    p = Malloc(sizeof(int));
    *p = which;
    in->data = p;
    in->efunc = do_find_game_exp;

    if (!*gameexp || which == 0) {
	construct_input(GAME_FIND_EXPRESSION_TITLE, gameexp, 1, 0, 
		GAME_FIND_EXPRESSION_PROMPT, NULL, NULL, 0, in, -1);
	return;
    }

    do_find_game_exp_finalize(which);
}

void do_global_find_new()
{
    global_find(0);
}

void do_global_find_next()
{
    global_find(1);
}

void do_global_find_prev()
{
    global_find(-1);
}

void do_global_game_jump()
{
    struct input_data_s *in;

    if (gtotal < 2)
	return;

    in = Calloc(1, sizeof(struct input_data_s));
    in->efunc = do_game_jump;

    if (!keycount) {
	construct_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL, NULL, NULL, 0, in, 
		0);
	return;
    }

    do_game_jump_finalize(keycount);
}

void do_global_toggle_delete()
{
    int i;

    pushkey = 0;

    if (gtotal < 2)
	return;

    if (keycount && delete_count == 0) {
	markstart = gindex;
	delete_count = keycount;
	update_status_notify(gp, "%s (delete)", status.notify);
	return;
    }

    if (markstart >= 0 && markend >= 0) {
	for (i = markstart; i < markend; i++) {
	    if (toggle_delete_flag(i)) {
		update_all(gp);
		return;
	    }
	}

	gindex = (delete_count < 0) ? markstart : i - 1;
	update_all(gp);
    }
    else {
	if (toggle_delete_flag(gindex))
	    return;
    }

    markstart = markend = -1;
    delete_count = 0;
    update_status_window(gp);
}

void do_global_delete_game()
{
    do_game_delete();
}

void do_global_tag_edit()
{
    struct userdata_s *d = gp->data;

    edit_tags(gp, d->b, 1);
}

void do_global_tag_view()
{
    struct userdata_s *d = gp->data;

    edit_tags(gp, d->b, 0);
}

void do_global_resume_game()
{
    struct input_data_s *in;

    in = Calloc(1, sizeof(struct input_data_s));
    in->efunc = do_load_file;
    construct_input(GAME_LOAD_TITLE, NULL, 1, 1, BROWSER_PROMPT, file_browser,
	    NULL, '\t', in, -1);
}

void do_global_save_game()
{
    if (gtotal > 1) {
	construct_message(NULL, GAME_SAVE_MULTI_PROMPT, 1, 1, NULL, NULL, NULL, 
		do_game_save_multi_confirm, 0, 0, "%s", GAME_SAVE_MULTI_TEXT);
	return;
    }

    do_get_game_save_input(-1);
}

void do_global_new_game()
{
    do_new_game();
}

void do_global_copy_game()
{
    int g = gindex;
    int i, n;
    struct userdata_s *d;

    do_global_new_game();
    n = pgn_history_total(game[g]->history);

    // FIXME RAV
    for (i = 0; i < n; i++)
	pgn_history_add(gp, game[g]->history[i]->move);

    n = pgn_tag_total(game[g]->tag);

    for (i = 0; i < n; i++)
	pgn_tag_add(&gp->tag, game[g]->tag[i]->name,
		game[g]->tag[i]->value);

    d = gp->data;
    pgn_board_update(gp, d->b, 
	    pgn_history_total(gp->hp));
}

void do_global_new_all()
{
    construct_message(NULL, YESNO, 1, 1, NULL, NULL, NULL, 
	    do_new_game_from_scratch, 0, 0, "%s", GAME_NEW_PROMPT);
}

void do_global_quit()
{
    quit = 1;
}

void do_global_toggle_engine_window()
{
    if (!enginew) {
	enginew = newwin(LINES, COLS, 0, 0);
	enginep = new_panel(enginew);
	window_draw_title(enginew, ENGINE_IO_TITLE, COLS, CP_MESSAGE_TITLE,
		CP_MESSAGE_BORDER);
	hide_panel(enginep);
    }

    if (panel_hidden(enginep)) {
	update_engine_window(gp);
	top_panel(enginep);
	refresh_all();
    }
    else {
	hide_panel(enginep);
	refresh_all();
    }
}

void do_global_toggle_board_details()
{
    do_board_details();
}

void do_global_toggle_strict_castling()
{
    do_toggle_strict_castling();
}

// Global and other keys.
static int globalkeys()
{
    struct userdata_s *d = gp->data;
    int i;

    /*
     * These cannot be modified and other game mode keys cannot conflict with
     * these.
     */
    switch (input_c) {
	case CTRL('L'):
	    endwin();
	    keypad(boardw, TRUE);
	    refresh_all();
	    return 1;
	case KEY_ESCAPE:
	    d->sp.icon = d->sp.srow = d->sp.scol = 0;
	    markend = markstart = 0;

	    if (keycount) {
		keycount = 0;
		update_status_notify(gp, NULL);
	    }

	    if (config.validmoves)
		pgn_reset_valid_moves(d->b);

	    return 1;
	case '0' ... '9':
		  i = input_c - '0';

		  if (keycount)
		      keycount = keycount * 10 + i;
		  else
		      keycount = i;

		  update_status_notify(gp, "Repeat %i", keycount);
		  return -1;
	case KEY_UP:
		  if (d->mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      d->c_row += keycount;
		      pushkey = '\n';
		  }
		  else
		      d->c_row++;

		  if (d->c_row > 8)
		      d->c_row = 1;

		  return 1;
	case KEY_DOWN:
		  if (d->mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      d->c_row -= keycount;
		      pushkey = '\n';
		      update_status_notify(gp, NULL);
		  }
		  else
		      d->c_row--;

		  if (d->c_row < 1)
		      d->c_row = 8;

		  return 1;
	case KEY_LEFT:
		  if (d->mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      d->c_col -= keycount;
		      pushkey = '\n';
		  }
		  else
		      d->c_col--;

		  if (d->c_col < 1)
		      d->c_col = 8;

		  return 1;
	case KEY_RIGHT:
		  if (d->mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      d->c_col += keycount;
		      pushkey = '\n';
		  }
		  else
		      d->c_col++;

		  if (d->c_col > 8)
		      d->c_col = 1;

		  return 1;
#ifdef HAVE_WRESIZE
	case KEY_RESIZE:
		  do_window_resize();
		  return 1;
#endif
	case 0:
	default:
		  for (i = 0; global_keys[i]; i++) {
		      if (input_c == global_keys[i]->c) {
			  (*global_keys[i]->f)();
			  return 1;
		      }
		  }
		  break;
    }

    return 0;
}

void game_loop()
{  
    struct userdata_s *d;
    int macro_match = -1;

    gindex = gtotal - 1;
    gp = game[gindex];
    d = gp->data;

    if (pgn_history_total(gp->hp))
	d->mode = MODE_HISTORY;
    else
	d->mode = MODE_PLAY;

    if (d->mode == MODE_HISTORY)
	pgn_board_update(gp, d->b, pgn_history_total(gp->hp));

    update_status_notify(gp, "%s", GAME_HELP_PROMPT);
    movestep = 2;
    flushinp();
    update_all(gp);
    update_tag_window(gp->tag);
    wtimeout(boardw, WINDOW_TIMEOUT);

    while (!quit) {
	int n = 0, i;
	char fdbuf[8192] = {0};
	int len;
	struct timeval tv = {0, 0};
	fd_set rfds, wfds;
	WIN *win = NULL;
	WINDOW *wp = NULL;

	FD_ZERO(&rfds);
	FD_ZERO(&wfds);

	for (i = 0; i < gtotal; i++) {
	    d = game[i]->data;

	    if (d->engine && d->engine->pid != -1) {
		if (d->engine->fd[ENGINE_IN_FD] > 2) {
		    if (d->engine->fd[ENGINE_IN_FD] > n)
			n = d->engine->fd[ENGINE_IN_FD];

		    FD_SET(d->engine->fd[ENGINE_IN_FD], &rfds);
		}

		if (d->engine->fd[ENGINE_OUT_FD] > 2) {
		    if (d->engine->fd[ENGINE_OUT_FD] > n)
			n = d->engine->fd[ENGINE_OUT_FD];

		    FD_SET(d->engine->fd[ENGINE_OUT_FD], &wfds);
		}
	    }
	}

	if (n) {
	    if ((n = select(n + 1, &rfds, &wfds, NULL, &tv)) > 0) {
		for (i = 0; i < gtotal; i++) {
		    d = game[i]->data;

		    if (d->engine && d->engine->pid != -1) {
			if (FD_ISSET(d->engine->fd[ENGINE_IN_FD], &rfds)) {
			    len = read(d->engine->fd[ENGINE_IN_FD], fdbuf,
				    sizeof(fdbuf));

			    if (len > 0) {
				if (d->engine->iobuf)
				    d->engine->iobuf = Realloc(d->engine->iobuf, d->engine->len + len + 1); 
				else
				    d->engine->iobuf = Calloc(1, len + 1);

				memcpy(&(d->engine->iobuf[d->engine->len]), &fdbuf, len);
				d->engine->len += len;
				d->engine->iobuf[d->engine->len] = 0;

				/*
				 * The fdbuf is full or no newline
				 * was found. So we'll append the next
				 * read() to this games buffer.
				 */
				if (d->engine->iobuf[d->engine->len - 1] != '\n')
				    continue;

				parse_engine_output(game[i], d->engine->iobuf);
				free(d->engine->iobuf);
				d->engine->iobuf = NULL;
				d->engine->len = 0;
			    }
			    else if (len == -1) {
				if (errno != EAGAIN) {
				    cmessage(ERROR, ANYKEY, "Engine read(): %s",
					    strerror(errno));
				    waitpid(d->engine->pid, &n, 0);
				    free(d->engine);
				    d->engine = NULL;
				    break;
				}
			    }
			}

			if (FD_ISSET(d->engine->fd[ENGINE_OUT_FD], &wfds)) {
			    if (d->engine->queue)
				send_engine_command(game[i]);
			}
		    }
		}
	    }
	    else {
		if (n == -1)
		    cmessage(ERROR, ANYKEY, "select(): %s", strerror(errno));
		/* timeout */
	    }
	}

	gp = game[gindex];
	d = gp->data;

	if (TEST_FLAG(gp->flags, GF_GAMEOVER))
	    d->mode = MODE_HISTORY;

	draw_board(gp);
	update_all(gp);
	wmove(boardw, ROWTOMATRIX(d->c_row), COLTOMATRIX(d->c_col));

	if (macro_match == -1)
	    refresh_all();

	/*
	 * Finds the top level window in the window stack so we know what
	 * window the wgetch()ed key belongs to.
	 */
	if (wins) {
	    for (i = 0; wins[i]; i++);
	    win = wins[i-1];
	    wp = win->w;
	    wtimeout(wp, WINDOW_TIMEOUT);
	}
	else
	    wp = boardw;

	if (!i && pushkey)
	    input_c = pushkey;
	else {
	    if (!pushkey) {
		if (macros && macro_match >= 0) {
		    if (macros[macro_match]->n >= macros[macro_match]->total) {
			macros[macro_match]->n = 0;
			macro_match = -1;
			continue;
		    }
		    else 
			input_c = macros[macro_match]->keys[macros[macro_match]->n++];
		}
		else {
		    if ((input_c = wgetch(wp)) == ERR)
			continue;
		}
	    }
	    else
		input_c = pushkey;

	    if (win) {
		switch (input_c) {
		    case CTRL('L'):
			endwin();
			keypad(boardw, TRUE);
			refresh_all();
			continue;
		}

		win->c = input_c;

		/*
		 * Run the function associated with the window. When the
		 * function returns 0 win->efunc is ran (if not NULL) with
		 * win as the one and only parameter. Then the window is
		 * destroyed.
		 *
		 * The exit function may create another window which will
		 * mess up the window stack when window_destroy() is called.
		 * So don't destory the window until the top window is
		 * destroyable. See window_destroy().
		 */
		if ((*win->func)(win) == 0) {
		    if (win->efunc)
			(*win->efunc)(win);

		    win->keep = 1;
		    window_destroy(win);
		}

		continue;
	    }
	}

	if (!keycount && status.notify)
	    update_status_notify(gp, NULL);

	if (macros && macro_match < 0) {
	    for (i = 0; macros[i]; i++) {
		if ((macros[i]->mode == -1 || macros[i]->mode == d->mode) &&
			input_c == macros[i]->c) {
		    input_c = macros[i]->keys[macros[i]->n++];
		    macro_match = i;
		    break;
		}
	    }
	}

	if ((n = globalkeys()) == 1) {
	    if (macro_match == -1)
		keycount = 0;

	    continue;
	}
	else if (n == -1)
	    continue;

	switch (d->mode) {
	    case MODE_EDIT:
		for (i = 0; edit_keys[i]; i++) {
		    if (input_c == edit_keys[i]->c) {
			(*edit_keys[i]->f)();
			break;
		    }
		}
		break;
	    case MODE_PLAY:
		for (i = 0; play_keys[i]; i++) {
		    if (input_c == play_keys[i]->c) {
			(*play_keys[i]->f)();
			goto done;
		    }
		}

		do_play_config_command();
		break;
	    case MODE_HISTORY:
		for (i = 0; history_keys[i]; i++) {
		    if (input_c == history_keys[i]->c) {
			(*history_keys[i]->f)();
			break;
		    }
		}
		break;
	    default:
		break;
	}

done:
	if (keycount)
	    update_status_notify(gp, NULL);

	keycount = 0;
    }
}

void usage(const char *pn, int ret)
{
    fprintf((ret) ? stderr : stdout, "%s",
#ifdef DEBUG
    "Usage: cboard [-hvCD] [-p [-VtRSE] <file>]\n"
    "  -D  Dump libchess debugging info to \"libchess.debug\" (stderr)\n"
#else
    "Usage: cboard [-hvC] [-p [-VtRSE] <file>]\n"
#endif
    "  -p  Load PGN file.\n"
    "  -V  Validate a game file.\n"
    "  -S  Validate and output a PGN formatted game.\n"
    "  -R  Like -S but write a reduced PGN formatted game.\n"
    "  -t  Also write custom PGN tags from config file.\n"
    "  -E  Stop processing on file parsing error (overrides config).\n"
    "  -C  Enable strict castling (overrides config).\n"
    "  -v  Version information.\n"
    "  -h  This help text.\n");

    exit(ret);
}

void cleanup_all()
{
    int i;

    stop_clock();
    free_userdata();
    pgn_free_all();
    free(config.engine_cmd);
    free(config.pattern);
    free(config.ccfile);
    free(config.nagfile);
    free(config.configfile);

    if (config.keys) {
	for (i = 0; config.keys[i]; i++) {
	    free(config.keys[i]->str);
	    free(config.keys[i]);
	}

	free(config.keys);
    }

    if (config.einit) {
	for (i = 0; config.einit[i]; i++)
	    free(config.einit[i]);

	free(config.einit);
    }

    if (config.tag)
	pgn_tag_free(config.tag);

    if (curses_initialized) {
	del_panel(boardp);
	del_panel(historyp);
	del_panel(statusp);
	del_panel(tagp);
	delwin(boardw);
	delwin(historyw);
	delwin(statusw);
	delwin(tagw);

	if (enginew) {
	    del_panel(enginep);
	    delwin(enginew);
	}

	endwin();
    }
}

void catch_signal(int which)
{
    switch (which) {
	case SIGALRM:
	    update_clocks();
	    break;
	case SIGPIPE:
	    if (which == SIGPIPE && quit)
		break;

	    if (which == SIGPIPE)
		cmessage(NULL, ANYKEY, "%s", E_BROKEN_PIPE);

	    cleanup_all();
	    exit(EXIT_FAILURE);
	    break;
	case SIGSTOP:
	    savetty();
	    break;
	case SIGCONT:
	    resetty();
	    keypad(boardw, TRUE);
	    curs_set(0);
	    cbreak();
	    noecho();
	    break;
	case SIGINT:
	case SIGTERM:
	    quit = 1;
	    break;
	default:
	    break;
    }
}

void loading_progress(long total, long offset)
{
    int n = (100 * (offset / 100) / (total / 100));

    if (curses_initialized)
	update_loading_window(n);
    else {
	fprintf(stderr, "Loading... %i%% (%i games)\r", n, gtotal);
	fflush(stderr);
    }
}

static void set_defaults()
{
    set_config_defaults();
    set_default_keys();
    filetype = NO_FILE;
    pgn_config_set(PGN_PROGRESS, 1024);
    pgn_config_set(PGN_PROGRESS_FUNC, loading_progress);
}

int main(int argc, char *argv[])
{
    int opt;
    struct stat st;
    char buf[FILENAME_MAX];
    char datadir[FILENAME_MAX];
    int ret = EXIT_SUCCESS;
    int validate_only = 0, validate_and_write = 0;
    int write_custom_tags = 0;
    FILE *fp;
    int i = 0;

/* Solaris 5.9 */
#ifndef HAVE_PROGNAME
    __progname = argv[0];
#endif

    if ((config.pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    snprintf(datadir, sizeof(datadir), "%s/.cboard", config.pwd->pw_dir);
    snprintf(buf, sizeof(buf), "%s/cc.data", datadir);
    config.ccfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/nag.data", datadir);
    config.nagfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/config", datadir);
    config.configfile = strdup(buf);

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

    set_defaults();

#ifdef DEBUG
    while ((opt = getopt(argc, argv, "DCEVtSRhp:v")) != -1) {
#else
    while ((opt = getopt(argc, argv, "ECVtSRhp:v")) != -1) {
#endif
	switch (opt) {
#ifdef DEBUG
	    case 'D':
		unlink("libchess.debug");
		pgn_config_set(PGN_DEBUG, 1);
		break;
#endif
	    case 'C':
		pgn_config_set(PGN_STRICT_CASTLING, 1);
		break;
	    case 't':
		write_custom_tags = 1;
		break;
	    case 'E':
		i = 1;
		break;
	    case 'R':
		pgn_config_set(PGN_REDUCED, 1);
	    case 'S':
		validate_and_write = 1;
	    case 'V':
		validate_only = 1;
		break;
	    case 'v':
		printf("%s (%s)\n%s\n", PACKAGE_STRING, curses_version(),
			COPYRIGHT);
		exit(EXIT_SUCCESS);
	    case 'p':
		filetype = PGN_FILE;
		strncpy(loadfile, optarg, sizeof(loadfile));
		break;
	    case 'h':
	    default:
		usage(argv[0], EXIT_SUCCESS);
	}
    }

    if ((validate_only || validate_and_write) && !*loadfile)
	usage(argv[0], EXIT_FAILURE);

    if (access(config.configfile, R_OK) == 0)
	parse_rcfile(config.configfile);

    if (i)
	pgn_config_set(PGN_STOP_ON_ERROR, 1);

    signal(SIGPIPE, catch_signal);
    signal(SIGCONT, catch_signal);
    signal(SIGSTOP, catch_signal);
    signal(SIGINT, catch_signal);
    signal(SIGALRM, catch_signal);
    signal(SIGTERM, catch_signal);

    srandom(getpid());

    switch (filetype) {
	case PGN_FILE:
	    if ((fp = pgn_open(loadfile)) == NULL)
		err(EXIT_FAILURE, "%s", loadfile);

	    ret = pgn_parse(fp);
	    break;
	case FEN_FILE:
	    //ret = parse_fen_file(loadfile);
	    break;
	case EPD_FILE: // Not implemented.
	case NO_FILE:
	default:
	    // No file specified. Empty game.
	    ret = pgn_parse(NULL);
	    gp = game[gindex];
	    add_custom_tags(&gp->tag);
	    break;
    }

    if (validate_only || validate_and_write) {
	if (validate_and_write) {
	    for (i = 0; i < gtotal; i++) {
		if (write_custom_tags)
		    add_custom_tags(&game[i]->tag);

		pgn_write(stdout, game[i]);
	    }
	}

	cleanup_all();
	exit(ret);
    }
    else if (ret == E_PGN_ERR)
	exit(ret);

    init_userdata();

    /*
     * This fixes window resizing in an xterm.
     */
    if (getenv("DISPLAY") != NULL) {
	putenv("LINES=");
	putenv("COLUMNS=");
    }

    if (initscr() == NULL)
	errx(EXIT_FAILURE, "%s", E_INITCURSES);
    else
	curses_initialized = 1;

    if (LINES < 24 || COLS < 80) {
	endwin();
	errx(EXIT_FAILURE, "Need at least an 80x24 terminal.");
    }

    if (has_colors() == TRUE && start_color() == OK)
	init_color_pairs();

    boardw = newwin(BOARD_HEIGHT, BOARD_WIDTH, 0, COLS - BOARD_WIDTH);
    boardp = new_panel(boardw);
    historyw = newwin(HISTORY_HEIGHT, HISTORY_WIDTH, LINES - HISTORY_HEIGHT,
	    COLS - HISTORY_WIDTH);
    historyp = new_panel(historyw);
    statusw = newwin(STATUS_HEIGHT, STATUS_WIDTH, 0, 0);
    statusp = new_panel(statusw);
    tagw = newwin(TAG_HEIGHT, TAG_WIDTH, STATUS_HEIGHT + 1, 0);
    tagp = new_panel(tagw);
    keypad(boardw, TRUE);
//  leaveok(boardw, TRUE);
    leaveok(tagw, TRUE);
    leaveok(statusw, TRUE);
    leaveok(historyw, TRUE);
    curs_set(0);
    cbreak();
    noecho();
    draw_window_decor();
    game_loop();
    cleanup_all();
    exit(EXIT_SUCCESS);
}
