/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    Copyright (C) 2002-2024 Ben Kibbey <bjk@luxsci.net>

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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>
#include <err.h>
#include <locale.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#ifdef WITH_LIBPERL
#include "perl-plugin.h"
#endif

#include <vdk.h>

#include "common.h"
#include "conf.h"
#include "window.h"
#include "message.h"
#include "colors.h"
#include "input.h"
#include "misc.h"
#include "engine.h"
#include "strings.h"
#include "menu.h"
#include "keys.h"
#include "rcfile.h"
#include "filebrowser.h"
#include "ui_screen.h"
#include "menubar.h"
#include "mouse.h"

#ifdef DEBUG
#include <debug.h>
#endif

#define CBOARD_URL "https://gitlab.com/bjk/cboard/wikis"
#define COPYRIGHT "Copyright (C) 2002-2024 " PACKAGE_BUGREPORT
#define STATUS_HEIGHT 12
/* Leave row 0 for the menubar; all chrome is laid out below it. */
#define UI_TOP CBOARD_MENUBAR_H
#define WORK_LINES (LINES - UI_TOP)
#define MEGA_BOARD (WORK_LINES >= 49 && COLS >= 144)
#define BOARD_HEIGHT_MB 50
#define BOARD_WIDTH_MB 98
#define STATUS_WIDTH_MB (COLS - BOARD_WIDTH_MB)
#define TAG_HEIGHT_MB 31
#define TAG_WIDTH_MB (COLS - BOARD_WIDTH_MB)
#define HISTORY_HEIGHT_MB (WORK_LINES - (STATUS_HEIGHT + TAG_HEIGHT_MB + 1))
#define HISTORY_WIDTH_MB (COLS - BOARD_WIDTH_MB)
#define BIG_BOARD (WORK_LINES >= 39 && COLS >= 112)
#define BOARD_HEIGHT ((MEGA_BOARD) ? BOARD_HEIGHT_MB : (BIG_BOARD) ? 34 \
                                                                   : 18)
#define BOARD_WIDTH ((MEGA_BOARD) ? BOARD_WIDTH_MB : (BIG_BOARD) ? 66 \
                                                                 : 34)
#define STATUS_WIDTH ((MEGA_BOARD) ? STATUS_WIDTH_MB : COLS - BOARD_WIDTH)
#define TAG_HEIGHT ((MEGA_BOARD) ? TAG_HEIGHT_MB : WORK_LINES - STATUS_HEIGHT - 1)
#define TAG_WIDTH ((MEGA_BOARD) ? TAG_WIDTH_MB : COLS - BOARD_WIDTH)
#define HISTORY_HEIGHT ((MEGA_BOARD) ? HISTORY_HEIGHT_MB : WORK_LINES - BOARD_HEIGHT)
#define HISTORY_WIDTH ((MEGA_BOARD) ? HISTORY_WIDTH_MB : COLS - STATUS_WIDTH)
#define MAX_VALUE_WIDTH (COLS - 8)

enum
{
    UP,
    DOWN,
    LEFT,
    RIGHT
};

static WINDOW *boardw;
static cboard_widget_t *board_vk; /* host canvas: board + coord gutters */
/* Inset 8x8 table (vk_grid + dividers); paint-mode cells. */
static vk_table_t *board_table;
/* 1-col rank gutter + 1-row file gutter outside the table. */
#define BOARD_RANK_GUTTER 1
#define BOARD_FILE_GUTTER 1
static WINDOW *tagw;
static cboard_widget_t *tag_vk;
static WINDOW *statusw;
static cboard_widget_t *status_vk;
static WINDOW *historyw;
static cboard_widget_t *history_vk;
static WINDOW *loadingw;
static cboard_widget_t *loading_vk;
static WINDOW *enginew;
static cboard_widget_t *engine_vk;

static char gameexp[255];
static char moveexp[255];
static struct itimerval clock_timer;
static int delete_count = 0;
static int markstart = -1, markend = -1;
static int keycount;
static char loadfile[FILENAME_MAX];
static int quit;
static wint_t input_c;

// Loaded filename from the command line or from the file input dialog.
static int filetype;
enum
{
    FILE_NONE,
    FILE_PGN,
    FILE_FEN,
    FILE_EPD
};

static char **nags;
static int nag_total;
static int macro_match;

// Primer movimiento de juego cargado
// First move loaded game
static char fm_loaded_file = FALSE;

// Status window.
static struct
{
    wchar_t *notify; // The status window notification line buffer.
} status;

static int curses_initialized;

// When in history mode a full step is to the next move of the same playing
// side. Half stepping is alternating sides.
static int movestep;

static wchar_t *w_pawn_wchar;
static wchar_t *w_rook_wchar;
static wchar_t *w_bishop_wchar;
static wchar_t *w_knight_wchar;
static wchar_t *w_queen_wchar;
static wchar_t *w_king_wchar;
static wchar_t *b_pawn_wchar;
static wchar_t *b_rook_wchar;
static wchar_t *b_bishop_wchar;
static wchar_t *b_knight_wchar;
static wchar_t *b_queen_wchar;
static wchar_t *b_king_wchar;
static wchar_t *empty_wchar;
static wchar_t *enpassant_wchar;

static wchar_t *yes_wchar;
static wchar_t *all_wchar;       // do_save_game_overwrite_confirm()
static wchar_t *overwrite_wchar; // do_save_game_overwrite_confirm()
static wchar_t *resume_wchar;    // do_history_mode_confirm()
static wchar_t *current_wchar;   // do_game_save_multi_confirm()
static wchar_t *append_wchar;    // save_pgn()

static const char piece_chars[] = "PpRrNnBbQqKkxx";
static char *translatable_tag_names[7];
static const char *f_pieces[] = {
    "       ", // 0
    "   O   ",
    "  /_\\  ",
    " |-|-| ", // 3
    "  ] [  ",
    " /___\\ ",
    "  /?M  ", // 6
    " (@/)) ",
    "  /__))",
    "   O   ", // 9
    "  (+)  ",
    "  /_\\  ",
    "•°°°°°•", // 12
    " \\\\|// ",
    " |___| ",
    " __+__ ", // 15
    "(__|__)",
    " |___| ",
    "  \\ /  ", // 18
    "   X   ",
    "  / \\  "};

static const bool cb[8][8] = {
    {1, 0, 1, 0, 1, 0, 1, 0},
    {0, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 0},
    {0, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 0},
    {0, 1, 0, 1, 0, 1, 0, 1},
    {1, 0, 1, 0, 1, 0, 1, 0},
    {0, 1, 0, 1, 0, 1, 0, 1}};

static void free_userdata_once(GAME g);
static void do_more_help(WIN *);
static void do_play_help();
static void do_history_help();
static void do_edit_help();

GAME gp;

void coordofmove(GAME g, char *move, char *prow, char *pcol)
{
    char l = strlen(move);

    if (*move == 'O')
    {
        *prow = (g->turn == WHITE) ? 8 : 1;
        *pcol = (l <= 4) ? 7 : 3;
        return;
    }

    move += l;

    while (!isdigit(*move))
        move--;

    *prow = RANKTOINT(*move--);
    *pcol = FILETOINT(*move);
}

#define INV_INT(x) (9 - x)
#define INV_INT0(x) (7 - x)

// Posición por rotación de tablero.
// Rotation board position.
void rotate_position(char *prow, char *pcol)
{
    *prow = INV_INT(*prow);
    *pcol = INV_INT(*pcol);
}

void update_cursor(GAME g, int idx)
{
    int t = pgn_history_total(g->hp);
    struct userdata_s *d = g->data;

    /*
   * If not deincremented then r and c would be the next move.
   */
    idx--;

    if (idx > t || idx < 0 || !t || !g->hp[idx]->move)
        d->c_row = 2, d->c_col = 5;
    else
        coordofmove(g, g->hp[idx]->move, &d->c_row, &d->c_col);

    if (d->mode == MODE_HISTORY && d->rotate)
        rotate_position(&d->c_row, &d->c_col);
}

static int
init_nag()
{
    FILE *fp;
    char line[LINE_MAX];
    int i = 0;

    if ((fp = fopen(config.nagfile, "r")) == NULL)
    {
        cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s", config.nagfile,
                 strerror(errno));
        return 1;
    }

    nags = Realloc(nags, (i + 2) * sizeof(char *));
    nags[i++] = strdup(_("none"));
    nags[i] = NULL;

    while (!feof(fp))
    {
        if (fscanf(fp, " %[^\n] ", line) == 1)
        {
            nags = Realloc(nags, (i + 2) * sizeof(char *));
            nags[i++] = strdup(line);
        }
    }

    nags[i] = NULL;
    nag_total = i;
    fclose(fp);
    return 0;
}

void edit_nag_toggle_item(struct menu_input_s *m)
{
    struct input_s *in = m->data;
    struct input_data_s *id = in->data;
    HISTORY *h = id->data;
    int i;

    if (m->selected == 0)
    {
        for (i = 0; i < MAX_PGN_NAG; i++)
            h->nag[i] = 0;

        for (i = 0; m->items[i]; i++)
            m->items[i]->selected = 0;

        return;
    }

    for (i = 0; i < MAX_PGN_NAG; i++)
    {
        if (h->nag[i] == m->selected)
            h->nag[i] = m->selected = 0;
        else
        {
            if (!h->nag[i])
            {
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
    message(_("NAG Menu Keys"), ANY_KEY_STR, "%s",
            _("    UP/DOWN - previous/next menu item\n"
              "   HOME/END - first/last menu item\n"
              "  PGDN/PGUP - next/previous page\n"
              "  a-zA-Z0-9 - jump to item\n"
              "      SPACE - toggle selected item\n"
              "     CTRL-X - quit with changes"));
}

struct menu_item_s **
get_nag_items(WIN *win)
{
    int i, n;
    struct menu_input_s *m = win->data;
    struct input_s *in = m->data;
    struct input_data_s *id = in->data;
    struct menu_item_s **items = m->items;
    HISTORY *h = id->data;

    if (items)
    {
        for (i = 0; items[i]; i++)
            free(items[i]);
    }

    for (i = 0; nags[i]; i++)
    {
        items = Realloc(items, (i + 2) * sizeof(struct menu_item_s *));
        items[i] = Malloc(sizeof(struct menu_item_s));
        items[i]->name = nags[i];
        items[i]->value = NULL;

        for (n = 0; n < MAX_PGN_NAG; n++)
        {
            if (h->nag[n] == i)
            {
                items[i]->selected = 1;
                n = -1;
                break;
            }
        }

        if (n >= 0)
            items[i]->selected = 0;
    }

    if (items)
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

    if (!nags)
    {
        if (init_nag())
            return;
    }

    add_menu_key(&keys, ' ', edit_nag_toggle_item);
    add_menu_key(&keys, CTRL_KEY('x'), edit_nag_save);
    add_menu_help_key(&keys, edit_nag_help);
    construct_menu(0, 0, -1, -1, _("Numeric Annotation Glyphs"), 1,
                   get_nag_items, keys, arg, nag_print, NULL, NULL);
    return;
}

static void *
view_nag(void *arg)
{
    HISTORY *h = (HISTORY *) arg;
    char buf[80];
    char line[LINE_MAX] = {0};
    int i = 0;

    snprintf(buf, sizeof(buf), "%s \"%s\"", _("Viewing NAG for"), h->move);

    if (!nags)
    {
        if (init_nag())
            return NULL;
    }

    for (i = 0; i < MAX_PGN_NAG; i++)
    {
        char buf2[16];

        if (!h->nag[i])
            break;

        if (h->nag[i] >= nag_total)
            strncat(line, itoa(h->nag[i], buf2), sizeof(line) - 1);
        else if (nags)
            strncat(line, nags[h->nag[i]], sizeof(line) - 1);

        strncat(line, "\n", sizeof(line) - 1);
    }

    line[strlen(line) - 1] = 0;
    message(buf, ANY_KEY_STR, "%s", line);
    return NULL;
}

void view_annotation(HISTORY *h)
{
    char buf[MAX_SAN_MOVE_LEN + strlen(_("Viewing Annotation for")) + 4];
    int nag = 0, comment = 0;

    if (!h)
        return;

    if (h->comment && h->comment[0])
        comment++;

    if (h->nag[0])
        nag++;

    if (!nag && !comment)
        return;

    snprintf(buf, sizeof(buf), "%s \"%s\"", _("Viewing Annotation for"),
             h->move);

    if (comment)
        construct_message(buf,
                          (nag) ? _("Any other key to continue") : ANY_KEY_STR,
                          0, 1, (nag) ? _("Press 'n' to view NAG") : NULL,
                          (nag) ? view_nag : NULL, (nag) ? h : NULL, NULL,
                          (nag) ? 'n' : 0, 0, NULL, "%s", h->comment);
    else
        construct_message(buf, _("Any other key to continue"), 0, 1,
                          _("Press 'n' to view NAG"), view_nag, h, NULL, 'n', 0,
                          NULL, "%s", _("No comment text for this move"));
}

int do_game_write(char *filename, const char *mode, int start, int end)
{
    int i;
    struct userdata_s *d;
    PGN_FILE *pgn;

    i = pgn_open(filename, mode, &pgn);

    if (i == E_PGN_ERR)
    {
        cmessage(ERROR_STR, ANY_KEY_STR, "%s\n%s", filename, strerror(errno));
        return 1;
    }
    else if (i == E_PGN_INVALID)
    {
        cmessage(ERROR_STR, ANY_KEY_STR, "%s\n%s", filename,
                 _("Not a regular file"));
        return 1;
    }

    for (i = (start == -1) ? 0 : start; i < end; i++)
    {
        d = game[i]->data;
        pgn_write(pgn, game[i]);
        CLEAR_FLAG(d->flags, CF_MODIFIED);
    }

    if (pgn_close(pgn) != E_PGN_OK)
        message(ERROR_STR, ANY_KEY_STR, "%s", strerror(errno));

    if (start == -1)
    {
        strncpy(loadfile, filename, sizeof(loadfile));
        loadfile[sizeof(loadfile) - 1] = 0;
    }

    return 0;
}

struct save_game_s
{
    char *filename;
    char *mode;
    int start;
    int end;
};

void do_save_game_overwrite_confirm(WIN *win)
{
    const char *mode = "w";
    struct save_game_s *s = win->data;
    wchar_t str[] = {win->c, 0};

    if (!wcscmp(str, append_wchar))
        mode = "a";
    else if (!wcscmp(str, overwrite_wchar))
        mode = "w";
    else
        goto done;

    if (do_game_write(s->filename, mode, s->start, s->end))
        update_status_notify(gp, "%s", _("Save game failed."));
    else
        update_status_notify(gp, "%s", _("Game saved."));

done:
    free(s->filename);
    free(s);
}

/* If the saveindex argument is -1, all games will be saved. Otherwise it's a
 * game index number.
 */
void save_pgn(char *filename, int saveindex)
{
    char buf[FILENAME_MAX];
    struct stat st;
    int end = (saveindex == -1) ? gtotal : saveindex + 1;
    struct save_game_s *s;

    if (filename[0] != '/' && config.savedirectory)
    {
        if (stat(config.savedirectory, &st) == -1)
        {
            if (errno == ENOENT)
            {
                if (mkdir(config.savedirectory, 0755) == -1)
                {
                    cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s",
                             config.savedirectory, strerror(errno));
                    return;
                }
            }
            else
            {
                cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s",
                         config.savedirectory, strerror(errno));
                return;
            }
        }

        if (stat(config.savedirectory, &st) == -1)
        {
            cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s", config.savedirectory,
                     strerror(errno));
            return;
        }

        if (!S_ISDIR(st.st_mode))
        {
            cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s", config.savedirectory,
                     _("Not a directory."));
            return;
        }

        snprintf(buf, sizeof(buf), "%s/%s", config.savedirectory, filename);
        filename = buf;
    }

    if (access(filename, W_OK) == 0)
    {
        s = Malloc(sizeof(struct save_game_s));
        s->filename = strdup(filename);
        s->start = saveindex;
        s->end = end;
        construct_message(NULL, _("What would you like to do?"), 0, 1, NULL,
                          NULL, s, do_save_game_overwrite_confirm, 0, 0, NULL,
                          "%s \"%s\"\nPress \"%ls\" to append to this file, \"%ls\" to overwrite or any other key to cancel.",
                          _("File exists:"), filename, append_wchar,
                          overwrite_wchar);
        return;
    }

    if (do_game_write(filename, "a", saveindex, end))
        update_status_notify(gp, "%s", _("Save game failed."));
    else
        update_status_notify(gp, "%s", _("Game saved."));
}

static int
castling_state(GAME g, BOARD b, int row, int col, int piece, int mod)
{
    if (pgn_piece_to_int(piece) == ROOK && col == 7 && row == 7 &&
        (TEST_FLAG(g->flags, GF_WK_CASTLE) || mod) &&
        pgn_piece_to_int(b[7][4].icon) == KING && isupper(piece))
    {
        if (mod)
            TOGGLE_FLAG(g->flags, GF_WK_CASTLE);
        return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 0 && row == 7 &&
             (TEST_FLAG(g->flags, GF_WQ_CASTLE) || mod) &&
             pgn_piece_to_int(b[7][4].icon) == KING && isupper(piece))
    {
        if (mod)
            TOGGLE_FLAG(g->flags, GF_WQ_CASTLE);
        return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 7 && row == 0 &&
             (TEST_FLAG(g->flags, GF_BK_CASTLE) || mod) &&
             pgn_piece_to_int(b[0][4].icon) == KING && islower(piece))
    {
        if (mod)
            TOGGLE_FLAG(g->flags, GF_BK_CASTLE);
        return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 0 && row == 0 &&
             (TEST_FLAG(g->flags, GF_BQ_CASTLE) || mod) &&
             pgn_piece_to_int(b[0][4].icon) == KING && islower(piece))
    {
        if (mod)
            TOGGLE_FLAG(g->flags, GF_BQ_CASTLE);
        return 1;
    }
    else if (pgn_piece_to_int(piece) == KING && col == 4 && row == 7 &&
             (mod || (pgn_piece_to_int(b[7][7].icon) == ROOK && TEST_FLAG(g->flags, GF_WK_CASTLE)) ||
              (pgn_piece_to_int(b[7][0].icon) == ROOK &&
               TEST_FLAG(g->flags, GF_WQ_CASTLE))) &&
             isupper(piece))
    {
        if (mod)
        {
            if (TEST_FLAG(g->flags, GF_WK_CASTLE) ||
                TEST_FLAG(g->flags, GF_WQ_CASTLE))
                CLEAR_FLAG(g->flags, GF_WK_CASTLE | GF_WQ_CASTLE);
            else
                SET_FLAG(g->flags, GF_WK_CASTLE | GF_WQ_CASTLE);
        }
        return 1;
    }
    else if (pgn_piece_to_int(piece) == KING && col == 4 && row == 0 &&
             (mod || (pgn_piece_to_int(b[0][7].icon) == ROOK && TEST_FLAG(g->flags, GF_BK_CASTLE)) ||
              (pgn_piece_to_int(b[0][0].icon) == ROOK &&
               TEST_FLAG(g->flags, GF_BQ_CASTLE))) &&
             islower(piece))
    {
        if (mod)
        {
            if (TEST_FLAG(g->flags, GF_BK_CASTLE) ||
                TEST_FLAG(g->flags, GF_BQ_CASTLE))
                CLEAR_FLAG(g->flags, GF_BK_CASTLE | GF_BQ_CASTLE);
            else
                SET_FLAG(g->flags, GF_BK_CASTLE | GF_BQ_CASTLE);
        }
        return 1;
    }

    return 0;
}

#define IS_ENPASSANT(c) (c == 'x') ? CP_BOARD_ENPASSANT : isupper(c) ? CP_BOARD_WHITE \
                                                                     : CP_BOARD_BLACK
#define ATTRS(cp) (cp & (A_BOLD | A_STANDOUT | A_BLINK | A_DIM | A_UNDERLINE | A_INVIS | A_REVERSE))

static void
init_wchar_pieces()
{
    w_pawn_wchar = str_to_wchar(config.utf8_pieces ? "♙" : "P");
    w_rook_wchar = str_to_wchar(config.utf8_pieces ? "♖" : "R");
    w_bishop_wchar = str_to_wchar(config.utf8_pieces ? "♗" : "B");
    w_knight_wchar = str_to_wchar(config.utf8_pieces ? "♘" : "N");
    w_queen_wchar = str_to_wchar(config.utf8_pieces ? "♕" : "Q");
    w_king_wchar = str_to_wchar(config.utf8_pieces ? "♔" : "K");
    b_pawn_wchar = str_to_wchar(config.utf8_pieces ? "♟" : "p");
    b_rook_wchar = str_to_wchar(config.utf8_pieces ? "♜" : "r");
    b_bishop_wchar = str_to_wchar(config.utf8_pieces ? "♝" : "b");
    b_knight_wchar = str_to_wchar(config.utf8_pieces ? "♞" : "n");
    b_queen_wchar = str_to_wchar(config.utf8_pieces ? "♛" : "q");
    b_king_wchar = str_to_wchar(config.utf8_pieces ? "♚" : "k");
    empty_wchar = str_to_wchar(" ");
    enpassant_wchar = str_to_wchar("x");
}

static wchar_t *
piece_to_wchar(unsigned char p)
{
    switch (p)
    {
    case 'P':
        return w_pawn_wchar;
    case 'p':
        return b_pawn_wchar;
    case 'R':
        return w_rook_wchar;
    case 'r':
        return b_rook_wchar;
    case 'B':
        return w_bishop_wchar;
    case 'b':
        return b_bishop_wchar;
    case 'N':
        return w_knight_wchar;
    case 'n':
        return b_knight_wchar;
    case 'Q':
        return w_queen_wchar;
    case 'q':
        return b_queen_wchar;
    case 'K':
        return w_king_wchar;
    case 'k':
        return b_king_wchar;
    case 'x':
        return enpassant_wchar;
    }

    return empty_wchar;
}

static int
piece_can_attack(GAME g, int rank, int file)
{
    struct userdata_s *d = g->data;
    char *m, *frfr = NULL;
    pgn_error_t e;
    int row, col, p, v, pi, cpi;

    if (d->rotate)
    {
        rotate_position(&d->c_row, &d->c_col);
        rotate_position(&d->sp.srow, &d->sp.scol);
    }

    v = d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].valid;
    pi = pgn_piece_to_int(d->b[RANKTOBOARD(rank)][FILETOBOARD(file)].icon);
    cpi = d->sp.icon
              ? pgn_piece_to_int(d->b[RANKTOBOARD(d->sp.srow)]
                                     [FILETOBOARD(d->sp.scol)]
                                         .icon)
              : pgn_piece_to_int(d->b[RANKTOBOARD(d->c_row)]
                                     [FILETOBOARD(d->c_col)]
                                         .icon);

    if (pi == OPEN_SQUARE || cpi == OPEN_SQUARE || !VALIDFILE(file) || !VALIDRANK(rank))
    {
        if (d->rotate)
        {
            rotate_position(&d->c_row, &d->c_col);
            rotate_position(&d->sp.srow, &d->sp.scol);
        }

        return 0;
    }

    if (d->sp.icon)
    {
        col = v ? d->c_col : d->sp.scol;
        row = v ? d->c_row : d->sp.srow;
    }
    else
    {
        col = d->c_col;
        row = d->c_row;
    }

    m = Malloc(MAX_SAN_MOVE_LEN + 1);
    m[0] = INTTOFILE(file);
    m[1] = INTTORANK(rank);
    m[2] = INTTOFILE(col);
    m[3] = INTTORANK(row);
    m[4] = 0;

    if (d->sp.icon && v)
    {
        BOARD b;

        memcpy(b, d->b, sizeof(BOARD));
        p = b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon;
        b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon =
            pgn_int_to_piece(WHITE, OPEN_SQUARE);
        b[RANKTOBOARD(row)][FILETOBOARD(col)].icon = p;
        pgn_switch_turn(g);
        e = pgn_validate_move(g, b, &m, &frfr);
        pgn_switch_turn(g);
        free(m);
        free(frfr);

        if (e != E_PGN_OK && pgn_piece_to_int(d->sp.icon) == PAWN)
        {
            int n = (d->sp.srow == 7 && islower(d->sp.icon) && rank == 5) ? 6 : (d->sp.srow == 2 && isupper(d->sp.icon) && rank == 4) ? 3
                                                                                                                                      : 0;

            if (n && (file == d->c_col - 1 || file == d->c_col + 1))
            {
                memcpy(b, d->b, sizeof(BOARD));
                p = b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon;
                b[RANKTOBOARD(d->sp.srow)][FILETOBOARD(d->sp.scol)].icon =
                    pgn_int_to_piece(WHITE, OPEN_SQUARE);
                b[RANKTOBOARD(row)][FILETOBOARD(col)].icon = p;
                b[RANKTOBOARD(n)][FILETOBOARD(d->sp.scol)].enpassant = 1;
                m = Malloc(MAX_SAN_MOVE_LEN + 1);
                m[0] = INTTOFILE(file);
                m[1] = INTTORANK(rank);
                m[2] = INTTOFILE(col);
                m[3] = INTTORANK(n);
                m[4] = 0;
                pgn_switch_turn(g);
                SET_FLAG(g->flags, GF_ENPASSANT);
                e = pgn_validate_move(g, b, &m, &frfr);
                CLEAR_FLAG(g->flags, GF_ENPASSANT);
                pgn_switch_turn(g);
                free(m);
                free(frfr);
            }
        }

        goto pca_quit;
    }

    pgn_switch_turn(g);
    e = pgn_validate_move(g, d->b, &m, &frfr);
    pgn_switch_turn(g);

    if (!strcmp(m, "O-O") || !strcmp(m, "O-O-O"))
        e = E_PGN_INVALID;

    if (e == E_PGN_OK)
    {
        int sf = FILETOINT(frfr[0]), sr = RANKTOINT(frfr[1]);
        int df = FILETOINT(frfr[2]);

        pi = d->b[RANKTOBOARD(sr)][FILETOBOARD(sf)].icon;
        pi = pgn_piece_to_int(pi);
        if (pi == PAWN && sf == df)
            e = E_PGN_INVALID;
    }

    free(m);
    free(frfr);

pca_quit:
    if (d->rotate)
    {
        rotate_position(&d->c_row, &d->c_col);
        rotate_position(&d->sp.srow, &d->sp.scol);
    }

    return e == E_PGN_OK ? 1 : 0;
}

void print_piece(WINDOW *w, int l, int c, char p)
{
    int i, y, ff = 0;

    for (i = 0; i < 13; i += 2)
    {
        if (p == piece_chars[i] || p == piece_chars[i + 1])
        {
            for (y = 0; y < 3; y++)
                mvwprintw(w, l + y, c, "%s", f_pieces[i + ff + y]);
            return;
        }

        ff++;
    }

    for (y = 0; y < 3; y++)
        mvwprintw(w, l + y, c, "%s", f_pieces[0]);
}

void board_prev_move_play(GAME g)
{
    struct userdata_s *d = g->data;
    char l = strlen(d->pm_frfr);

    if (l)
    {
        char q = (l > 4) ? 2 : 1;

        d->pm_row = RANKTOINT(d->pm_frfr[l - q++]);
        d->pm_col = FILETOINT(d->pm_frfr[l - q++]);
        d->ospm_row = RANKTOINT(d->pm_frfr[l - q++]);
        d->ospm_col = FILETOINT(d->pm_frfr[l - q]);
        if (d->rotate)
        {
            rotate_position(&d->pm_row, &d->pm_col);
            rotate_position(&d->ospm_row, &d->ospm_col);
        }
    }
    else
    {
        d->pm_row = 0;
        d->pm_col = 0;
        d->ospm_row = 0;
        d->ospm_col = 0;
    }
}

void board_prev_move_history(GAME g)
{
    struct userdata_s *d = g->data;

    if (g->hindex)
    {
        char *move = g->hp[g->hindex - 1]->move;

        if (move)
        {
            if (d->mode == MODE_PLAY)
                coordofmove(g, move, &d->pm_row, &d->pm_col);
            else
            {
                d->pm_row = 0;
                d->pm_col = 0;
            }

            if (*move == 'O')
            {
                d->ospm_row = g->turn == WHITE ? 8 : 1;
                d->ospm_col = 5;
                return;
            }

            BOARD ob;
            unsigned char f, r;

            pgn_board_init(ob);

            if (g->hindex > 1)
            {
                HISTORY *h = pgn_history_by_n(g->hp, g->hindex - 2);
                if (h)
                {
                    pgn_board_init_fen(g, ob, h->fen);
                    pgn_switch_turn(g);
                }
            }

            for (f = 0; f < 8; f++)
            {
                for (r = 0; r < 8; r++)
                {
                    if (ob[f][r].icon != '.' && d->b[f][r].icon == '.')
                    {
                        d->ospm_row = INV_INT0(f) + 1;
                        d->ospm_col = r + 1;
                        break;
                    }
                }
            }

            if (d->rotate)
            {
                if (d->mode == MODE_PLAY)
                    rotate_position(&d->pm_row, &d->pm_col);

                rotate_position(&d->ospm_row, &d->ospm_col);
            }

            return;
        }
    }
    else
    {
        d->ospm_row = 0;
        d->ospm_col = 0;
        d->pm_row = 0;
        d->pm_col = 0;
    }
}


/*
 * Display ↔ chess mapping for the 8x8 board_table.
 * Grid (0,0) is top-left on screen. Unrotated: rank 8 / file a.
 * Cursor c_row/c_col stay in chess 1..8 display coordinates (same as keys).
 */
static void
board_display_to_chess(const struct userdata_s *d, int gcol, int grow,
                       int *rank, int *file)
{
    if (d && d->rotate)
    {
        *rank = grow + 1;
        *file = 8 - gcol;
    }
    else
    {
        *rank = 8 - grow;
        *file = gcol + 1;
    }
}

static void
board_chess_to_display(const struct userdata_s *d, int rank, int file,
                       int *gcol, int *grow)
{
    if (d && d->rotate)
    {
        *grow = rank - 1;
        *gcol = 8 - file;
    }
    else
    {
        *grow = 8 - rank;
        *gcol = file - 1;
    }
}

static int
board_square_color(int rank, int file)
{
    /* a1 is dark; rank+file even → light when rank/file are 1-based. */
    return ((rank + file) % 2) ? WHITE : BLACK;
}

static void
board_fill_cell(WINDOW *cv, int x, int y, int w, int h, chtype attrs)
{
    int yy, xx;

    if (!cv || w < 1 || h < 1)
        return;
    for (yy = 0; yy < h; yy++)
        for (xx = 0; xx < w; xx++)
            mvwaddch(cv, y + yy, x + xx, ' ' | attrs);
}

static void
board_paint_piece(WINDOW *cv, int x, int y, int w, int h,
                  unsigned char p, int pi, chtype attrs)
{
    if (!cv)
        return;

    wattron(cv, attrs);
    if (h >= 3 && w >= 7 && BIG_BOARD)
    {
        /* ASCII art pieces when the cell is large enough. */
        int px = x + (w > 7 ? (w - 7) / 2 : 0);
        int py = y + (h - 3) / 2;
        if (py < y)
            py = y;
        print_piece(cv, py, px, (pi != OPEN_SQUARE) ? (char) p : 0);
    }
    else
    {
        const wchar_t *ws = piece_to_wchar(pi != OPEN_SQUARE ? p : 0);
        int px = x + w / 2;
        int py = y + h / 2;
        if (px < x)
            px = x;
        if (py < y)
            py = y;
        mvwaddwstr(cv, py, px, ws);
    }
    wattroff(cv, attrs);
}

/*
 * Junction glyph for a divider intersection at grid line (r, c) within
 * an 8x8 table (r,c in 0..8).  Mirrors vk_table / vk_color focus overdraw.
 */
static chtype
board_table_junction(int style, int r, int c)
{
    int top = (r > 0);
    int bot = (r < 8);
    int left = (c > 0);
    int right = (c < 8);

    if (style == VK_BORDER_ASCII)
    {
        if (!top && !left)
            return '+';
        if (!top && !right)
            return '+';
        if (!bot && !left)
            return '+';
        if (!bot && !right)
            return '+';
        return '+';
    }

    if (!top && !left)
        return ACS_ULCORNER;
    if (!top && !right)
        return ACS_URCORNER;
    if (!bot && !left)
        return ACS_LLCORNER;
    if (!bot && !right)
        return ACS_LRCORNER;
    if (!top)
        return ACS_TTEE;
    if (!bot)
        return ACS_BTEE;
    if (!left)
        return ACS_LTEE;
    if (!right)
        return ACS_RTEE;
    return ACS_PLUS;
}

/*
 * Light up the subfocus cell by recoloring its table dividers (border),
 * not the square fill — same mechanism as vk_color focus overdraw.
 * Cell interior stays normal; only the surrounding grid lines change.
 */
static void
board_paint_subfocus_border(vk_table_t *table, WINDOW *cv, int gcol, int grow)
{
    vk_grid_t *grid;
    int x, y, w, h;
    int i;
    int style;
    short pair;
    attr_t attrs;
    chtype hl, vl;

    if (!table || !cv)
        return;

    grid = VK_GRID(table);
    style = vk_table_get_divider_style(table);
    if (style == VK_BORDER_NONE)
        return;

    if (vk_grid_get_cell_rect(grid, gcol, grow, &x, &y, &w, &h) != 0)
        return;
    if (w < 1 || h < 1)
        return;

    /*
     * With gap=1, dividers sit immediately outside the cell rect:
     *   top    y-1,  left x-1,  bottom y+h,  right x+w
     * Cursor green is CONF_BCURSOR.bg; draw that as the line fg.
     */
    pair = vdk_color_pair(config.color[CONF_BCURSOR].bg,
                          config.color[CONF_BGRAPHICS].bg);
    attrs = A_BOLD;

    if (style == VK_BORDER_ASCII)
    {
        hl = '-';
        vl = '|';
    }
    else
    {
        hl = ACS_HLINE;
        vl = ACS_VLINE;
    }

    wattr_set(cv, attrs, pair, NULL);

    /* Top + bottom edges (between corners). */
    for (i = 0; i < w; i++)
    {
        if (y > 0)
            mvwaddch(cv, y - 1, x + i, hl);
        mvwaddch(cv, y + h, x + i, hl);
    }
    /* Left + right edges. */
    for (i = 0; i < h; i++)
    {
        if (x > 0)
            mvwaddch(cv, y + i, x - 1, vl);
        mvwaddch(cv, y + i, x + w, vl);
    }
    /* Four junctions (table line intersections). */
    if (y > 0 && x > 0)
        mvwaddch(cv, y - 1, x - 1, board_table_junction(style, grow, gcol));
    if (y > 0)
        mvwaddch(cv, y - 1, x + w, board_table_junction(style, grow, gcol + 1));
    if (x > 0)
        mvwaddch(cv, y + h, x - 1, board_table_junction(style, grow + 1, gcol));
    mvwaddch(cv, y + h, x + w, board_table_junction(style, grow + 1, gcol + 1));

    wattr_set(cv, A_NORMAL, 0, NULL);
}


static int
board_table_ox(void)
{
    return config.coordsyleft ? BOARD_RANK_GUTTER : 0;
}

static int
board_table_oy(void)
{
    return 0;
}

static int
board_table_width(void)
{
    int w = BOARD_WIDTH - BOARD_RANK_GUTTER;
    return w < 10 ? 10 : w;
}

static int
board_table_height(void)
{
    int h = BOARD_HEIGHT - BOARD_FILE_GUTTER;
    return h < 10 ? 10 : h;
}

/*
 * Size/place the 8x8 table inset on the host canvas so rank/file labels
 * live in the outer gutters (outside the squares), like classic cboard.
 */
static void
board_layout_table(void)
{
    vk_widget_t *tw;
    WINDOW *host;
    int ox, oy, tw_w, tw_h;

    if (!board_table || !board_vk)
        return;

    host = cboard_ui_widget_canvas(board_vk);
    boardw = host;
    if (!host)
        return;

    tw = VK_WIDGET(board_table);
    ox = board_table_ox();
    oy = board_table_oy();
    tw_w = board_table_width();
    tw_h = board_table_height();

    vk_widget_resize(tw, tw_w, tw_h);
    vk_widget_set_surface(tw, host);
    vk_widget_move(tw, ox, oy);
}

/* Rank numbers + file letters in host gutters (never inside cells). */
static void
board_paint_coord_labels(struct userdata_s *d, WINDOW *host)
{
    vk_grid_t *grid;
    int gcol, grow;
    int ox, oy;
    int x, y, w, h;
    int rank, file;

    if (!board_table || !host || !d)
        return;

    grid = VK_GRID(board_table);
    ox = board_table_ox();
    oy = board_table_oy();

    wattron(host, CP_BOARD_COORDS);

    for (grow = 0; grow < 8; grow++)
    {
        if (vk_grid_get_cell_rect(grid, 0, grow, &x, &y, &w, &h) != 0)
            continue;
        board_display_to_chess(d, 0, grow, &rank, &file);
        if (config.coordsyleft)
            mvwprintw(host, oy + y + h / 2, 0, "%d", rank);
        else
            mvwprintw(host, oy + y + h / 2, BOARD_WIDTH - 1, "%d", rank);
    }

    for (gcol = 0; gcol < 8; gcol++)
    {
        /* Center each file letter under its column (bottom gutter). */
        if (vk_grid_get_cell_rect(grid, gcol, 7, &x, &y, &w, &h) != 0)
            continue;
        board_display_to_chess(d, gcol, 7, &rank, &file);
        mvwprintw(host, BOARD_HEIGHT - 1, ox + x + (w > 1 ? w / 2 : 0), "%c",
                  "abcdefgh"[file - 1]);
    }

    wattroff(host, CP_BOARD_COORDS);
}

static void
board_sync_subfocus(struct userdata_s *d)
{
    int gcol, grow;

    if (!board_table || !d)
        return;
    if (d->c_row < 1 || d->c_row > 8 || d->c_col < 1 || d->c_col > 8)
        return;
    board_chess_to_display(d, d->c_row, d->c_col, &gcol, &grow);
    vk_grid_set_subfocus(VK_GRID(board_table), gcol, grow);
}

void update_board_window(GAME g)
{
    struct userdata_s *d;
    vk_grid_t *grid;
    WINDOW *cv;
    WINDOW *host;
    int gcol, grow;
    int focus_col, focus_row;

    if (!g || !g->data || !board_table || !board_vk)
        return;

    d = g->data;
    grid = VK_GRID(board_table);

    if (config.bprevmove && d->mode != MODE_EDIT)
    {
        if (!d->pm_undo && d->mode == MODE_PLAY)
            board_prev_move_play(g);
        else
            board_prev_move_history(g);
    }
    else
    {
        d->pm_row = 0;
        d->pm_col = 0;
        d->ospm_row = 0;
        d->ospm_col = 0;
    }

    if (d->mode != MODE_PLAY && d->mode != MODE_EDIT)
        update_cursor(g, g->hindex);

    board_layout_table();
    host = cboard_ui_widget_canvas(board_vk);
    boardw = host;
    if (!host)
        return;

    board_sync_subfocus(d);
    focus_col = vk_grid_get_subfocus_col(grid);
    focus_row = vk_grid_get_subfocus_row(grid);

    /* Clear host (includes gutters); table paints onto its own canvas. */
    werase(host);
    wbkgd(host, CP_BOARD_WINDOW);

    /* Background + ACS dividers from VDK. */
    vk_widget_set_colors(VK_WIDGET(board_table),
                         config.color[CONF_BDWINDOW].fg,
                         config.color[CONF_BDWINDOW].bg);
    vk_table_set_border_colors(board_table,
                               config.color[CONF_BGRAPHICS].fg,
                               config.color[CONF_BGRAPHICS].bg);
    vk_table_update(board_table);

    cv = vk_widget_get_canvas(VK_WIDGET(board_table));

    for (grow = 0; grow < 8; grow++)
    {
        for (gcol = 0; gcol < 8; gcol++)
        {
            int rank, file, br, bc;
            int x, y, w, h;
            unsigned char p;
            int pi;
            int sq_white;
            int valid = 0;
            int can_attack = 0;
            chtype cell_attrs;
            chtype piece_attrs;
            int is_selected;
            int is_prev;

            board_display_to_chess(d, gcol, grow, &rank, &file);
            br = RANKTOBOARD(rank);
            bc = FILETOBOARD(file);
            if (br < 0 || br > 7 || bc < 0 || bc > 7)
                continue;

            if (vk_grid_get_cell_rect(grid, gcol, grow, &x, &y, &w, &h) != 0)
                continue;

            p = d->b[br][bc].icon;
            pi = pgn_piece_to_int(p);
            sq_white = board_square_color(rank, file) == WHITE;

            if (config.validmoves && d->b[br][bc].valid)
                valid = 1;

            if (config.showattacks && config.details
                && piece_can_attack(g, rank, file))
                can_attack = 1;

            is_selected = (d->sp.icon && d->sp.srow == rank
                           && d->sp.scol == file);
            is_prev = ((d->pm_row == rank && d->pm_col == file)
                       || (d->ospm_row == rank && d->ospm_col == file));

            /* Base square colour (cursor uses subfocus rim, not fill). */
            if (valid)
                cell_attrs = sq_white ? CP_BOARD_MOVES_WHITE
                                      : CP_BOARD_MOVES_BLACK;
            else if (is_selected)
                cell_attrs = CP_BOARD_SELECTED;
            else if (is_prev && !valid)
                cell_attrs = CP_BOARD_PREVMOVE;
            else
                cell_attrs = sq_white ? CP_BOARD_WHITE : CP_BOARD_BLACK;

            if (config.details && d->b[br][bc].enpassant)
            {
                p = pi = 'x';
                cell_attrs = mix_cp(CP_BOARD_ENPASSANT, cell_attrs,
                                    ATTRS(CP_BOARD_ENPASSANT), A_FG_B_BG);
            }

            if (can_attack)
                cell_attrs = mix_cp(CP_BOARD_ATTACK, cell_attrs,
                                    ATTRS(CP_BOARD_ATTACK), A_FG_B_BG);

            board_fill_cell(cv, x, y, w, h, cell_attrs);

            if (pi != OPEN_SQUARE && p != 'x' && !can_attack)
            {
                if (sq_white)
                    piece_attrs = isupper(p) ? CP_BOARD_W_W : CP_BOARD_W_B;
                else
                    piece_attrs = isupper(p) ? CP_BOARD_B_W : CP_BOARD_B_B;
            }
            else
                piece_attrs = cell_attrs;

            if (config.details && !can_attack
                && castling_state(g, d->b, br, bc, p, 0))
                piece_attrs = mix_cp(CP_BOARD_CASTLING, piece_attrs,
                                     ATTRS(CP_BOARD_CASTLING), A_FG_B_BG);

            board_paint_piece(cv, x, y, w, h, p, pi, piece_attrs);
        }
    }

    /*
     * Cursor = table border highlight on the subfocus cell (not a green
     * tile fill).  Dividers around the cell are overdrawn in cursor color.
     */
    if (focus_col >= 0 && focus_row >= 0)
        board_paint_subfocus_border(board_table, cv, focus_col, focus_row);

    /* Composite inset table onto host, then coords in the gutters. */
    vk_widget_draw(VK_WIDGET(board_table));
    board_paint_coord_labels(d, host);
}

void invalid_move(int n, int e, const char *m)
{
    if (curses_initialized)
        cmessage(ERROR_STR, ANY_KEY_STR, "%s \"%s\" (round #%i)",
                 (e ==
                  E_PGN_AMBIGUOUS)
                     ? _("Ambiguous move")
                     : _("Invalid move"),
                 m,
                 n);
    else
        warnx("%s: %s \"%s\" (round #%i)", loadfile, (e == E_PGN_AMBIGUOUS) ? _("Ambiguous move") : _("Invalid move"), m, n);
}

void gameover(GAME g)
{
    struct userdata_s *d = g->data;

    SET_FLAG(g->flags, GF_GAMEOVER);
    d->mode = MODE_HISTORY;
    stop_engine(g);
}

static void
update_clock(GAME g, struct itimerval it)
{
    struct userdata_s *d = g->data;

    if (TEST_FLAG(d->flags, CF_CLOCK) && g->turn == WHITE)
    {
        d->wclock.elapsed.tv_sec += it.it_value.tv_sec;
        d->wclock.elapsed.tv_usec += it.it_value.tv_usec;

        if (d->wclock.elapsed.tv_usec > 1000000 - 1)
        {
            d->wclock.elapsed.tv_sec += d->wclock.elapsed.tv_usec / 1000000;
            d->wclock.elapsed.tv_usec = d->wclock.elapsed.tv_usec % 1000000;
        }

        if (d->wclock.tc[d->wclock.tcn][1] &&
            d->wclock.elapsed.tv_sec >= d->wclock.tc[d->wclock.tcn][1])
        {
            pgn_tag_add(&g->tag, (char *) "Result", (char *) "0-1");
            gameover(g);
        }
    }
    else if (TEST_FLAG(d->flags, CF_CLOCK) && g->turn == BLACK)
    {
        d->bclock.elapsed.tv_sec += it.it_value.tv_sec;
        d->bclock.elapsed.tv_usec += it.it_value.tv_usec;

        if (d->bclock.elapsed.tv_usec > 1000000 - 1)
        {
            d->bclock.elapsed.tv_sec += d->bclock.elapsed.tv_usec / 1000000;
            d->bclock.elapsed.tv_usec = d->bclock.elapsed.tv_usec % 1000000;
        }

        if (d->bclock.tc[d->bclock.tcn][1] &&
            d->bclock.elapsed.tv_sec >= d->bclock.tc[d->bclock.tcn][1])
        {
            pgn_tag_add(&g->tag, (char *) "Result", (char *) "1-0");
            gameover(g);
        }
    }

    d->elapsed.tv_sec += it.it_value.tv_sec;
    d->elapsed.tv_usec += it.it_value.tv_usec;

    if (d->elapsed.tv_usec > 1000000 - 1)
    {
        d->elapsed.tv_sec += d->elapsed.tv_usec / 1000000;
        d->elapsed.tv_usec = d->elapsed.tv_usec % 1000000;
    }
}

static void
update_time_control(GAME g)
{
    struct userdata_s *d = g->data;
    struct clock_s *clk = (g->turn == WHITE) ? &d->wclock : &d->bclock;

    if (clk->incr)
        clk->tc[clk->tcn][1] += clk->incr;

    if (!clk->tc[clk->tcn][1])
        return;

    clk->move++;

    if (!clk->tc[clk->tcn][0] || clk->move >= clk->tc[clk->tcn][0])
    {
        clk->move = 0;
        clk->tc[clk->tcn + 1][1] +=
            labs(clk->elapsed.tv_sec - clk->tc[clk->tcn][1]);
        memset(&clk->elapsed, 0, sizeof(clk->elapsed));
        clk->tcn++;
    }
}

void update_history_window(GAME g)
{
    char buf[256];
    HISTORY *h = NULL;
    int n, total;
    int t = pgn_history_total(g->hp);
    int maxy, maxx, field_w;

    if (!historyw)
        return;

    getmaxyx(historyw, maxy, maxx);
    (void) maxy;
    field_w = maxx > 14 ? maxx - 12 : maxx;
    if (field_w < 1)
        field_w = 1;

    n = (g->hindex + 1) / 2;

    if (t % 2)
        total = (t + 1) / 2;
    else
        total = t / 2;

    if (t)
        snprintf(buf, sizeof(buf), "%u %s %u%s", n, _("of"), total,
                 (movestep == 1) ? _(" (ply)") : "");
    else
        strncpy(buf, _("not available"), sizeof(buf) - 1);

    buf[sizeof(buf) - 1] = 0;
    /* Interior of a VDK frame (border/title are the window, not this canvas). */
    mvwprintw(historyw, 0, 0, "%*s %-*s", 10, _("Move:"), field_w, buf);

    h = pgn_history_by_n(g->hp, g->hindex);
    snprintf(buf, sizeof(buf), "%s",
             (h && h->move) ? h->move
             : (LINES < 24) ? _("empty")
                            : _("not available"));
    n = 0;

    if (h && ((h->comment) || h->nag[0]))
    {
        strncat(buf, _(" (Annotated"), sizeof(buf) - 1);
        n++;
    }

    if (h && h->rav)
    {
        strncat(buf, (n) ? ",+" : " (+", sizeof(buf) - 1);
        n++;
    }

    if (g->ravlevel)
    {
        strncat(buf, (n) ? ",-" : " (-", sizeof(buf) - 1);
        n++;
    }

    if (n)
        strncat(buf, ")", sizeof(buf) - 1);

    mvwprintw(historyw, 1, 0, "%s %-*s",
              (LINES < 24) ? _("Next:") : _("Next move:"),
              field_w > 2 ? field_w - 2 : field_w, buf);

    h = pgn_history_by_n(g->hp, g->hindex - 1);
    snprintf(buf, sizeof(buf), "%s",
             (h && h->move) ? h->move
             : (LINES < 24) ? _("empty")
                            : _("not available"));
    n = 0;

    if (h && ((h->comment) || h->nag[0]))
    {
        strncat(buf, _(" (Annotated"), sizeof(buf) - 1);
        n++;
    }

    if (h && h->rav)
    {
        strncat(buf, (n) ? ",+" : " (+", sizeof(buf) - 1);
        n++;
    }

    if (g->ravlevel)
    {
        strncat(buf, (n) ? ",-" : " (-", sizeof(buf) - 1);
        n++;
    }

    if (n)
        strncat(buf, ")", sizeof(buf) - 1);

    mvwprintw(historyw, 2, 0, "%s %-*s",
              (LINES < 24) ? _("Prev.:") : _("Prev move:"),
              field_w > 2 ? field_w - 2 : field_w, buf);

    cboard_ui_frame_paint(history_vk);
}

void do_validate_move(char **move)
{
    struct userdata_s *d = gp->data;
    int n;
    char *frfr = NULL;

    if (TEST_FLAG(d->flags, CF_HUMAN))
    {
        if ((n = pgn_parse_move(gp, d->b, move, &frfr)) != E_PGN_OK)
        {
            invalid_move(d->n + 1, n, *move);
            return;
        }

        strcpy(d->pm_frfr, frfr);
        update_time_control(gp);
        pgn_history_add(gp, d->b, *move);
        pgn_switch_turn(gp);
    }
    else
    {
        if ((n = pgn_validate_move(gp, d->b, move, &frfr)) != E_PGN_OK)
        {
            invalid_move(d->n + 1, n, *move);
            return;
        }

        add_engine_command(gp, ENGINE_THINKING, "%s\n",
                           (config.engine_protocol == 1) ? frfr : *move);
    }

    d->sp.srow = d->sp.scol = d->sp.icon = 0;

    if (config.validmoves)
        pgn_reset_valid_moves(d->b);

    if (TEST_FLAG(gp->flags, GF_GAMEOVER))
        d->mode = MODE_HISTORY;
    else
        SET_FLAG(d->flags, CF_MODIFIED);

    free(frfr);
    d->paused = 0;
    update_history_window(gp);
    update_board_window(gp);
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
    do_validate_move(&str);
    free(str);
    win->data = NULL;
}

static void
move_to_engine(GAME g)
{
    struct userdata_s *d = g->data;
    char *str;
    int piece;

    if (config.validmoves &&
        !d->b[RANKTOBOARD(d->sp.row)][FILETOBOARD(d->sp.col)].valid)
        return;

    str = Malloc(MAX_SAN_MOVE_LEN + 1);
    snprintf(str, MAX_SAN_MOVE_LEN + 1, "%c%u%c%u",
             _("abcdefgh")[d->sp.scol - 1],
             d->sp.srow, _("abcdefgh")[d->sp.col - 1], d->sp.row);

    piece =
        pgn_piece_to_int(d->b[RANKTOBOARD(d->sp.srow)]
                             [FILETOBOARD(d->sp.scol)]
                                 .icon);

    if (piece == PAWN && (d->sp.row == 8 || d->sp.row == 1))
    {
        construct_message(_("Select Pawn Promotion Piece"), _("R/N/B/Q"), 1, 1,
                          NULL, NULL, str, do_promotion_piece_finalize, 0, 0,
                          NULL, "%s",
                          _("R = Rook, N = Knight, B = Bishop, Q = Queen"));
        return;
    }

    do_validate_move(&str);
    free(str);
}

static char *
clock_to_char(long n)
{
    static char buf[16];
    int h = 0, m = 0, s = 0;

    h = n / 3600;
    m = (n % 3600) / 60;
    s = (n % 3600) % 60;
    snprintf(buf, sizeof(buf), "%.2i:%.2i:%.2i", h, m, s);
    return buf;
}

static char *
timeval_to_char(struct timeval t, long limit)
{
    static char buf[9];
    unsigned h = 0, m = 0, s = 0;
    int n = limit ? labs(limit - t.tv_sec) : 0;

    h = n / 3600;
    m = (n % 3600) / 60;
    s = (n % 3600) % 60;
    snprintf(buf, sizeof(buf), "%.2u:%.2u:%.2u", h, m, s);
    return buf;
}

static char *
time_control_status(struct clock_s *clk)
{
    static char buf[80] = {0};

    buf[0] = 0;

    if (clk->tc[clk->tcn][0] && clk->tc[clk->tcn + 1][1])
        snprintf(buf, sizeof(buf), " M%.2i/%s",
                 abs(clk->tc[clk->tcn][0] - clk->move),
                 clock_to_char(clk->tc[clk->tcn + 1][1]));
    else if (!clk->incr)
        return (char *) "";

    if (clk->incr)
    {
        char tbuf[16];

        strncat(tbuf, " I", sizeof(tbuf) - 1);
        strncat(tbuf, itoa(clk->incr, buf), sizeof(tbuf) - 1);
    }

    return buf;
}

void update_status_window(GAME g)
{
    int i = 0;
    char *buf;
    char tmp[15] = {0}, *engine, *mode;
    char t[COLS];
    int w;
    char *p;
    int maxy, maxx;
    int len;
    struct userdata_s *d = g->data;
    int y;
    int n;

    if (!curses_initialized || !statusw)
        return;

    getmaxyx(statusw, maxy, maxx);
    (void) maxy;
    /* Interior canvas: margin col 0 is optional; field after 7-char label. */
    w = maxx > 10 ? maxx - 8 : maxx;
    if (w < 1)
        w = 1;
    len = maxx > 2 ? maxx : 2;
    buf = Malloc(len);
    y = 0;

    wchar_t *loadfilew = loadfile[0]
                             ? str_etc(loadfile, w, 1)
                             : str_to_wchar(_("not available"));
    mvwprintw(statusw, y++, 0, "%*s %-*ls", 7, _("File:"), w, loadfilew);
    free(loadfilew);
    snprintf(buf, len, "%i %s %i", gindex + 1, _("of"), gtotal);
    mvwprintw(statusw, y++, 0, "%*s %-*s", 7, _("Game:"), w, buf);

    *tmp = '\0';
    p = tmp;

    if (config.details)
    {
        *p++ = 'D';
        i++;
    }

    if (TEST_FLAG(d->flags, CF_DELETE))
    {
        if (i)
            *p++ = '/';

        *p++ = 'X';
        i++;
    }

    if (TEST_FLAG(g->flags, GF_PERROR))
    {
        if (i)
            *p++ = '/';

        *p++ = '!';
        i++;
    }

    if (TEST_FLAG(d->flags, CF_MODIFIED))
    {
        if (i)
            *p++ = '/';

        *p++ = '*';
        i++;
    }

    pgn_config_get(PGN_STRICT_CASTLING, &n);

    if (n == 1)
    {
        if (i)
            *p++ = '/';

        *p++ = 'C';
        i++;
    }
#ifdef WITH_LIBPERL
    if (TEST_FLAG(d->flags, CF_PERL))
    {
        if (i)
            *p++ = '/';

        *p++ = 'P';
        i++;
    }
#endif

    *p = '\0';
    mvwprintw(statusw, y++, 0, "%*s %-*s", 7, _("Flags:"), w,
              (tmp[0]) ? tmp : "-");

    switch (d->mode)
    {
    case MODE_HISTORY:
        mode = _("move history");
        break;
    case MODE_EDIT:
        mode = _("edit");
        break;
    case MODE_PLAY:
        mode = _("play");
        break;
    default:
        mode = _("(empty value)");
        break;
    }

    snprintf(buf, len - 1, "%*s %s", 7, _("Mode:"), mode);

    if (d->mode == MODE_PLAY)
    {
        if (TEST_FLAG(d->flags, CF_HUMAN))
            strncat(buf, _(" (human/human)"), len - 1);
        else if (TEST_FLAG(d->flags, CF_ENGINE_LOOP))
            strncat(buf, _(" (engine/engine)"), len - 1);
        else
            strncat(buf, (d->play_mode == PLAY_EH) ? _(" (engine/human)") : _(" (human/engine)"), len - 1);
    }

    buf[len - 1] = 0;
    mvwprintw(statusw, y++, 0, "%-*s", len, buf);
    free(buf);

    mvwprintw(statusw, y++, 0, "%*s %-*s", 7, _("Valid:"), w,
              config.validmoves ? _("on") : _("off"));

    if (d->engine)
    {
        switch (d->engine->status)
        {
        case ENGINE_THINKING:
            engine = _("pondering...");
            break;
        case ENGINE_READY:
            engine = _("ready");
            break;
        case ENGINE_INITIALIZING:
            engine = _("initializing...");
            break;
        case ENGINE_OFFLINE:
            engine = _("offline");
            break;
        default:
            engine = _("(empty value)");
            break;
        }
    }
    else
        engine = _("offline");

    mvwprintw(statusw, y, 0, "%*s %-*s", 7, _("Engine:"), w, " ");
    wattron(statusw, CP_STATUS_ENGINE);
    mvwaddstr(statusw, y++, 8, engine);
    wattroff(statusw, CP_STATUS_ENGINE);

    mvwprintw(statusw, y++, 0, "%*s %-*s", 7, _("Turn:"), w,
              (g->turn == WHITE) ? _("white") : _("black"));

    strncpy(tmp, _("white"), sizeof(tmp) - 1);
    tmp[0] = toupper(tmp[0]);
    snprintf(t, sizeof(t), "%s%s",
             timeval_to_char(d->wclock.elapsed,
                             d->wclock.tc[d->wclock.tcn][1]),
             time_control_status(&d->wclock));
    mvwprintw(statusw, y++, 0, "%*s: %-*s", 6, tmp, w, t);

    strncpy(tmp, _("black"), sizeof(tmp) - 1);
    tmp[0] = toupper(tmp[0]);
    snprintf(t, sizeof(t), "%s%s",
             timeval_to_char(d->bclock.elapsed,
                             d->bclock.tc[d->bclock.tcn][1]),
             time_control_status(&d->bclock));
    mvwprintw(statusw, y++, 0, "%*s: %-*s", 6, tmp, w, t);

    mvwprintw(statusw, y++, 0, "%*s %-*s", 7, _("Total:"), w,
              clock_to_char(d->elapsed.tv_sec));

    cboard_ui_frame_paint(status_vk);

    if (!status.notify)
    {
        char tbuf[255];

        snprintf(tbuf, sizeof(tbuf), _("Type %ls for help"),
                 key_lookup(global_keys, do_global_help));
        status.notify = str_to_wchar(tbuf);
    }

    /* One row under the status frame (menubar + status height). */
    {
        int notify_y = UI_TOP + STATUS_HEIGHT;

        wattron(stdscr, CP_STATUS_NOTIFY);
        for (i = (config.boardleft) ? BOARD_WIDTH : 0;
             i < ((config.boardleft) ? COLS : STATUS_WIDTH); i++)
            mvwprintw(stdscr, notify_y, i, " ");
        mvwprintw(stdscr, notify_y,
                  CENTERX(STATUS_WIDTH, status.notify) + ((config.boardleft) ? BOARD_WIDTH : 0), "%ls", status.notify);
        wattroff(stdscr, CP_STATUS_NOTIFY);
    }
}

wchar_t *
translate_tag_name(const char *tag)
{
    if (!strcmp(tag, "Event"))
        return str_to_wchar(translatable_tag_names[0]);
    else if (!strcmp(tag, "Site"))
        return str_to_wchar(translatable_tag_names[1]);
    else if (!strcmp(tag, "Date"))
        return str_to_wchar(translatable_tag_names[2]);
    else if (!strcmp(tag, "Round"))
        return str_to_wchar(translatable_tag_names[3]);
    else if (!strcmp(tag, "White"))
        return str_to_wchar(translatable_tag_names[4]);
    else if (!strcmp(tag, "Black"))
        return str_to_wchar(translatable_tag_names[5]);
    else if (!strcmp(tag, "Result"))
        return str_to_wchar(translatable_tag_names[6]);

    return str_to_wchar(tag);
}

void update_tag_window(TAG **t)
{
    int i, l, w;
    int namel = 0;
    int maxy, maxx, rows;

    if (!tagw)
        return;

    getmaxyx(tagw, maxy, maxx);
    rows = maxy > 0 ? maxy : 1;

    for (i = 0; t[i]; i++)
    {
        wchar_t *namewc = translate_tag_name(t[i]->name);

        l = wcslen(namewc);
        free(namewc);
        if (l > namel)
            namel = l;
    }

    w = maxx - namel - 2;
    if (w < 1)
        w = 1;

    for (i = 0; t[i] && i < rows; i++)
    {
        wchar_t *namewc = translate_tag_name(t[i]->name);
        wchar_t *valuewc = str_etc(t[i]->value, w, 0);

        mvwprintw(tagw, i, 0, "%*ls: %-*ls", namel, namewc, w, valuewc);
        free(namewc);
        free(valuewc);
    }

    for (; i < rows; i++)
        mvwprintw(tagw, i, 0, "%*s", namel + w + 2, " ");

    cboard_ui_frame_paint(tag_vk);
}

void append_enginebuf(GAME g, char *line)
{
    int i = 0;
    struct userdata_s *d = g->data;

    if (d->engine->enginebuf)
        for (i = 0; d->engine->enginebuf[i]; i++)
            ;

    if (d->engine->enginebuf && i >= LINES - 3)
    {
        free(d->engine->enginebuf[0]);

        for (i = 0; d->engine->enginebuf[i + 1]; i++)
            d->engine->enginebuf[i] = d->engine->enginebuf[i + 1];

        d->engine->enginebuf[i] = strdup(line);
    }
    else
    {
        d->engine->enginebuf =
            Realloc(d->engine->enginebuf, (i + 2) * sizeof(char *));
        d->engine->enginebuf[i++] = strdup(line);
        d->engine->enginebuf[i] = NULL;
    }
}

void update_engine_window(GAME g)
{
    int i;
    struct userdata_s *d = g->data;

    if (!enginew)
        return;

    wmove(enginew, 0, 0);
    wclrtobot(enginew);

    if (d->engine && d->engine->enginebuf)
    {
        for (i = 0; d->engine->enginebuf[i]; i++)
            mvwprintw(enginew, i + 2, 1, "%s", d->engine->enginebuf[i]);
    }

    window_draw_title(enginew, _("Engine IO Window"), COLS, CP_MESSAGE_TITLE,
                      CP_MESSAGE_BORDER);
}

void update_all(GAME g)
{
    struct userdata_s *d = g->data;

    /*
   * In the middle of a macro. Don't update the screen.
   */
    if (macro_match != -1)
        return;

    update_board_window(g);
    update_status_window(g);
    update_history_window(g);
    update_tag_window(g->tag);
    update_engine_window(g);
    cboard_menubar_refresh();
    cboard_ui_refresh();
    /*
   * Board/status canvases were written above and are dirty.  Clear the
   * touch flags so a later wget_wch on any of them (or a regression that
   * reads keys from boardw again) cannot wrefresh the board over a menu.
   */
    if (boardw)
        untouchwin(boardw);
    if (statusw)
        untouchwin(statusw);
    if (historyw)
        untouchwin(historyw);
    if (tagw)
        untouchwin(tagw);
}

static void
game_next_prev(GAME g, int n, int count)
{
    if (gtotal < 2)
        return;

    if (n == 1)
    {
        if (gindex + count > gtotal - 1)
        {
            if (count != 1)
                gindex = gtotal - 1;
            else
                gindex = 0;
        }
        else
            gindex += count;
    }
    else
    {
        if (gindex - count < 0)
        {
            if (count != 1)
                gindex = 0;
            else
                gindex = gtotal - 1;
        }
        else
            gindex -= count;
    }

    gp = game[gindex];
}

static void
delete_game(int which)
{
    int i, w = which;
    struct userdata_s *d;

    for (i = 0; i < gtotal; i++)
    {
        d = game[i]->data;

        if (i == w || TEST_FLAG(d->flags, CF_DELETE))
        {
            int n;

            free_userdata_once(game[i]);
            pgn_free(game[i]);

            for (n = i; n + 1 < gtotal; n++)
                game[n] = game[n + 1];

            gtotal--;
            i--;
            w = -1;
        }
    }

    if (which != -1)
    {
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
static int
find_move_exp(GAME g, regex_t r, int which, int count)
{
    int i;
    int ret;
    char errbuf[255];
    int incr;
    int found;

    incr = (which == 0) ? -1 : 1;

    for (i = g->hindex + incr - 1, found = 0;; i += incr)
    {
        if (i == g->hindex - 1)
            break;

        if (i >= pgn_history_total(g->hp))
            i = 0;
        else if (i < 0)
            i = pgn_history_total(g->hp) - 1;

        // FIXME RAV
        ret = regexec(&r, g->hp[i]->move, 0, 0, 0);

        if (ret == 0)
        {
            if (count == ++found)
            {
                return i + 1;
            }
        }
        else
        {
            if (ret != REG_NOMATCH)
            {
                regerror(ret, &r, errbuf, sizeof(errbuf));
                cmessage(_("Error Matching Regular Expression"), ANY_KEY_STR,
                         "%s", errbuf);
                return -1;
            }
        }
    }

    return -1;
}

static int
toggle_delete_flag(int n)
{
    int i, x;
    struct userdata_s *d = game[n]->data;

    TOGGLE_FLAG(d->flags, CF_DELETE);
    gindex = n;

    for (i = x = 0; i < gtotal; i++)
    {
        d = game[i]->data;

        if (TEST_FLAG(d->flags, CF_DELETE))
            x++;
    }

    if (x == gtotal)
    {
        cmessage(NULL, ANY_KEY_STR, "%s", _("Cannot delete last game."));
        d = game[n]->data;
        CLEAR_FLAG(d->flags, CF_DELETE);
        return 1;
    }

    return 0;
}

static int
find_game_exp(char *str, int which, int count)
{
    char *nstr = NULL, *exp = NULL;
    regex_t nexp, vexp;
    int ret = -1;
    int g = 0;
    char buf[255] = {0}, *tmp;
    char errbuf[255];
    int found = 0;
    int incr = (which == 0) ? -(1) : 1;

    strncpy(buf, str, sizeof(buf));
    buf[sizeof(buf) - 1] = 0;
    tmp = buf;

    if (strstr(tmp, ":") != NULL)
    {
        nstr = strsep(&tmp, ":");

        if ((ret = regcomp(&nexp, nstr,
                           REG_ICASE | REG_EXTENDED | REG_NOSUB)) != 0)
        {
            regerror(ret, &nexp, errbuf, sizeof(errbuf));
            cmessage(_("Error Compiling Regular Expression"), ANY_KEY_STR,
                     "%s", errbuf);
            ret = g = -1;
            goto cleanup;
        }
    }

    exp = tmp;

    while (exp && *exp && isspace(*exp))
        exp++;

    if (exp == NULL)
        goto cleanup;

    if ((ret = regcomp(&vexp, exp, REG_EXTENDED | REG_NOSUB)) != 0)
    {
        regerror(ret, &vexp, errbuf, sizeof(errbuf));
        cmessage(_("Error Compiling Regular Expression"), ANY_KEY_STR, "%s",
                 errbuf);
        ret = -1;
        goto cleanup;
    }

    ret = -1;

    for (g = gindex + incr, found = 0;; g += incr)
    {
        int t;

        if (g == gtotal)
            g = 0;
        else if (g < 0)
            g = gtotal - 1;

        if (g == gindex)
            break;

        for (t = 0; game[g]->tag[t]; t++)
        {
            if (nstr)
            {
                if (regexec(&nexp, game[g]->tag[t]->name, 0, 0, 0) == 0)
                {
                    if (regexec(&vexp, game[g]->tag[t]->value, 0, 0, 0) == 0)
                    {
                        if (count == ++found)
                        {
                            ret = g;
                            goto cleanup;
                        }
                    }
                }
            }
            else
            {
                if (regexec(&vexp, game[g]->tag[t]->value, 0, 0, 0) == 0)
                {
                    if (count == ++found)
                    {
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
void update_status_notify(GAME g, const char *fmt, ...)
{
    va_list ap;
#ifdef HAVE_VASPRINTF
    char *line;
#else
    char line[COLS];
#endif

    free(status.notify);
    status.notify = NULL;

    if (!fmt)
        return;

    va_start(ap, fmt);
#ifdef HAVE_VASPRINTF
    if (vasprintf(&line, fmt, ap) < 0)
        line = NULL;
#else
    vsnprintf(line, sizeof(line), fmt, ap);
#endif
    va_end(ap);

    if (line)
        status.notify = str_to_wchar(line);

#ifdef HAVE_VASPRINTF
    free(line);
#endif
}

int rav_next_prev(GAME g, BOARD b, int n)
{
    // Next RAV.
    if (n)
    {
        if ((!g->ravlevel && g->hindex && g->hp[g->hindex - 1]->rav == NULL) ||
            (!g->ravlevel && !g->hindex && g->hp[g->hindex]->rav == NULL) ||
            (g->ravlevel && g->hp[g->hindex]->rav == NULL))
            return 1;

        g->rav = Realloc(g->rav, (g->ravlevel + 1) * sizeof(RAV));
        g->rav[g->ravlevel].hp = g->hp;
        g->rav[g->ravlevel].flags = g->flags;
        g->rav[g->ravlevel].fen = pgn_game_to_fen(g, b);
        g->rav[g->ravlevel].hindex = g->hindex;
        g->hp =
            (!g->ravlevel) ? (g->hindex) ? g->hp[g->hindex -
                                                 1]
                                               ->rav
                                         : g->hp[g->hindex]->rav
                           : g->hp[g->hindex]->rav;
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

static void
draw_window_decor()
{
    cboard_ui_widget_move(board_vk, UI_TOP,
                          (config.boardleft) ? 0 : COLS - BOARD_WIDTH);
    cboard_ui_widget_move(history_vk, LINES - HISTORY_HEIGHT,
                          (config.boardleft) ? (MEGA_BOARD) ? BOARD_WIDTH : 0 : (MEGA_BOARD) ? 0
                                                                                             : COLS - HISTORY_WIDTH);
    cboard_ui_widget_move(status_vk, UI_TOP,
                          (config.boardleft) ? BOARD_WIDTH : 0);
    cboard_ui_widget_move(tag_vk, UI_TOP + STATUS_HEIGHT + 1,
                          (config.boardleft) ? (MEGA_BOARD) ? BOARD_WIDTH : HISTORY_WIDTH : 0);

    wbkgd(boardw, CP_BOARD_WINDOW);
    /* Status/tags/history are VDK frames; only the board is a bare canvas. */
    if (statusw)
        wbkgd(statusw, CP_STATUS_WINDOW);
    if (tagw)
        wbkgd(tagw, CP_TAG_WINDOW);
    if (historyw)
        wbkgd(historyw, CP_HISTORY_WINDOW);
}

static void
history_menu_resize(WIN *w)
{
    struct menu_input_s *m;

    if (!w)
        return;

    w->rows = MEGA_BOARD ? WORK_LINES - HISTORY_HEIGHT_MB : WORK_LINES;
    w->cols = TAG_WIDTH;
    w->posy = UI_TOP;
    w->posx = config.boardleft ? BOARD_WIDTH : 0;
    m = w->data;
    m->ystatic = w->posy;
    m->xstatic = w->posx;
    redraw_menu(w);
}

/*
 * Single terminal-resize cascade (VWM / VDK style).
 *
 * KEY_RESIZE is the only entry for geometry changes.  Order:
 *   1) vk_screen_resize — owns resize_term + surface canvases
 *   2) re-arm kmio / tty modes (reattach / reset survival)
 *   3) layout chrome with vk_widget_resize / vk_widget_move only
 *   4) modal stack via window_resize_all (each rfunc uses VDK)
 *   5) repaint content + one cboard_ui_refresh composite
 *
 * Do not mix a parallel LINES/COLS poll rebuild or a second window_resize_all
 * + update_all after this; that races ncurses resizeterm during wrefresh.
 */
void do_window_resize(void)
{
    /* 1–2: terminal geometry + input modes (VDK owns both). */
    cboard_ui_resize();
    cboard_ui_input_rearm();

    if (LINES < 24 || COLS < 74)
    {
        /* Too small for chrome; still keep term/kmio in sync. */
        cboard_ui_refresh();
        return;
    }

    /* 3: permanent chrome — size then place (no raw wresize/wclear). */
    cboard_menubar_resize();

    boardw = cboard_ui_widget_resize(board_vk, BOARD_HEIGHT, BOARD_WIDTH);
    historyw =
        cboard_ui_frame_resize(history_vk, HISTORY_HEIGHT, HISTORY_WIDTH);
    statusw =
        cboard_ui_frame_resize(status_vk, STATUS_HEIGHT, STATUS_WIDTH);
    tagw = cboard_ui_frame_resize(tag_vk, TAG_HEIGHT, TAG_WIDTH);

    draw_window_decor();

    if (loading_vk)
        loadingw = cboard_ui_widget_canvas(loading_vk);
    if (engine_vk)
    {
        enginew = cboard_ui_widget_resize(engine_vk, LINES, COLS);
        cboard_ui_widget_move(engine_vk, 0, 0);
    }

    /* 4: open modals (menus, inputs, dialogs) — VDK geometry only. */
    window_resize_all();

    /* 5: one content paint + composite (no keypad/cbreak redo on boardw). */
    if (gp)
        update_all(gp);
    else
    {
        cboard_menubar_refresh();
        cboard_ui_refresh();
    }
}

void do_global_redraw(void)
{
    /* Ctrl-L: full cascade (same path as KEY_RESIZE / reattach). */
    do_window_resize();
}

void stop_clock()
{
    memset(&clock_timer, 0, sizeof(struct itimerval));
    setitimer(ITIMER_REAL, &clock_timer, NULL);
}

void start_clock(GAME g)
{
    struct userdata_s *d = g->data;

    if (clock_timer.it_interval.tv_usec)
        return;

    memset(&d->elapsed, 0, sizeof(struct timeval));
    clock_timer.it_value.tv_sec = 0;
    clock_timer.it_value.tv_usec = 100000;
    clock_timer.it_interval.tv_sec = 0;
    clock_timer.it_interval.tv_usec = 100000;
    setitimer(ITIMER_REAL, &clock_timer, NULL);
}

static void
update_clocks()
{
    int i;
    struct userdata_s *d;
    struct itimerval it;
    int update = 0;

    getitimer(ITIMER_REAL, &it);

    for (i = 0; i < gtotal; i++)
    {
        d = game[i]->data;

        if (d && d->mode == MODE_PLAY)
        {
            if (d->paused == 1 || TEST_FLAG(d->flags, CF_NEW))
                continue;
            else if (d->paused == -1)
            {
                if (game[i]->side == game[i]->turn)
                {
                    d->paused = 1;
                    continue;
                }
            }

            update_clock(game[i], it);

            if (game[i] == gp)
                update = 1;
        }
    }

    if (update)
    {
        update_status_window(gp);
        /* Keep menubar/dropdown registered as the front layer before composite. */
        cboard_menubar_refresh();
        cboard_ui_refresh();
    }
}

#define SKIP_SPACE(str)       \
    {                         \
        while (isspace(*str)) \
            str++;            \
    }

static int
parse_clock_time(char **str)
{
    char *p = *str;
    int n = 0, t = 0;

    SKIP_SPACE(p);

    if (!isdigit(*p))
        return -1;

    while (*p)
    {
        if (isdigit(*p))
        {
            t = atoi(p);

            while (isdigit(*p))
                p++;

            continue;
        }

        switch (*p)
        {
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
            p++;
        case '/':
        case '+':
            goto done;
        default:
            *str = p;
            return -1;
        }

        p++;
    }

done:
    n += t;
    *str = p;
    return n;
}

static int
parse_clock_input(struct clock_s *clk, char *str, int *incr)
{
    char *p = str;
    long n = 0;
    int plus = 0;
    int m = 0;
    int tc = 0;

    SKIP_SPACE(p);

    if (!*p)
        return 0;

    if (*p == '+')
    {
        plus = 1;
        p++;
        SKIP_SPACE(p);

        if (*p == '+')
            goto move_incr;
    }
    else
        memset(clk, 0, sizeof(struct clock_s));

again:
    /* Sudden death. */
    if (strncasecmp(p, "SD", 2) == 0)
    {
        n = 0;
        p += 2;
        goto tc;
    }

    n = parse_clock_time(&p);

    if (n == -1)
        return 1;

    if (!n)
        goto done;

    /* Time control. */
tc:
    if (*p == '/')
    {
        if (plus)
            return 1;

        /* Sudden death without a previous time control. */
        if (!n && !tc)
            return 1;

        m = n;
        p++;
        n = parse_clock_time(&p);

        if (n == -1)
            return 1;

        if (tc >= MAX_TC)
        {
            message(ERROR_STR, ANY_KEY_STR, "%s (%i)",
                    _("Maximum number of time controls reached"), MAX_TC);
            return 1;
        }

        clk->tc[tc][0] = m;
        clk->tc[tc++][1] = n;
        SKIP_SPACE(p);

        if (*p == '+')
            goto move_incr;

        if (*p)
            goto again;

        goto done;
    }

    if (plus)
        *incr = n;
    else
        clk->tc[clk->tcn][1] =
            (n <= clk->elapsed.tv_sec) ? clk->elapsed.tv_sec + n : n;

move_incr:
    if (*p)
    {
        if (*p++ == '+')
        {
            if (!isdigit(*p))
                return 1;

            n = parse_clock_time(&p);

            if (n == -1 || *p)
                return 1;

            clk->incr = n;

            SKIP_SPACE(p);

            if (*p)
                return 1;
        }
        else
            return 1;
    }

done:
    return 0;
}

static int
parse_which_clock(struct clock_s *clk, char *str)
{
    struct clock_s tmp;
    int incr = 0;

    memcpy(&tmp, clk, sizeof(struct clock_s));

    if (parse_clock_input(&tmp, str, &incr))
    {
        cmessage(ERROR_STR, ANY_KEY_STR, _("Invalid clock specification"));
        return 1;
    }

    memcpy(clk, &tmp, sizeof(struct clock_s));
    clk->tc[clk->tcn][1] += incr;
    return 0;
}

void do_clock_input_finalize(WIN *win)
{
    struct userdata_s *d = gp->data;
    struct input_data_s *in = win->data;
    char *p = in->str;

    if (!in->str)
    {
        free(in);
        return;
    }

    SKIP_SPACE(p);

    if (tolower(*p) == 'w')
    {
        p++;

        if (parse_which_clock(&d->wclock, p))
            goto done;
    }
    else if (tolower(*p) == 'b')
    {
        p++;

        if (parse_which_clock(&d->bclock, p))
            goto done;
    }
    else
    {
        if (parse_which_clock(&d->wclock, p))
            goto done;

        if (parse_which_clock(&d->bclock, p))
            goto done;
    }

    if (!d->wclock.tc[0][1] && !d->bclock.tc[0][1])
        CLEAR_FLAG(d->flags, CF_CLOCK);
    else
        SET_FLAG(d->flags, CF_CLOCK);

done:
    free(in->str);
    free(in);
}

void do_engine_command_finalize(WIN *win)
{
    struct userdata_s *d = gp->data;
    struct input_data_s *in = win->data;
    int x;

    if (!in->str)
    {
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
    construct_input(_("Set Clock"), NULL, 1, 1,
                    _("Format: [W | B] [+]T[+I] | ++I | M/T [M/T [...] [SD/T]] [+I]\n"
                      "T = time (hms), I = increment, M = moves per, SD = sudden death\ne.g., 30m or 4m+12s or 35/90m SD/30m"),
                    NULL, NULL, 0, in, INPUT_HIST_CLOCK, NULL, -1);
}

void do_play_toggle_human()
{
    struct userdata_s *d = gp->data;

    TOGGLE_FLAG(d->flags, CF_HUMAN);

    if (!TEST_FLAG(d->flags, CF_HUMAN) && pgn_history_total(gp->hp))
    {
        if (init_chess_engine(gp))
            return;
    }

    CLEAR_FLAG(d->flags, CF_ENGINE_LOOP);

    if (d->engine)
        d->engine->status = ENGINE_READY;
}

void do_play_toggle_engine()
{
    struct userdata_s *d = gp->data;

    TOGGLE_FLAG(d->flags, CF_ENGINE_LOOP);
    CLEAR_FLAG(d->flags, CF_HUMAN);

    if (d->engine && TEST_FLAG(d->flags, CF_ENGINE_LOOP))
    {
        char *fen = pgn_game_to_fen(gp, d->b);

        pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
        add_engine_command(gp, ENGINE_READY, "setboard %s\n", fen);
        free(fen);
    }
}

/*
 * This will send a command to the engine skipping the command queue.
 */
void do_play_send_command()
{
    struct userdata_s *d = gp->data;
    struct input_data_s *in;

    if (!d->engine || d->engine->status == ENGINE_OFFLINE)
    {
        if (init_chess_engine(gp))
            return;
    }

    in = Calloc(1, sizeof(struct input_data_s));
    in->efunc = do_engine_command_finalize;
    construct_input(_("Engine Command"), NULL, 1, 1, NULL, NULL, NULL, 0, in,
                    INPUT_HIST_ENGINE, NULL, -1);
}

/*
void do_play_switch_turn()
{
    struct userdata_s *d = gp->data;

    pgn_switch_side(gp);
    pgn_switch_turn(gp);

    if (!TEST_FLAG(d->flags, CF_HUMAN))
    add_engine_command(gp, -1,
        (gp->side == WHITE) ? "white\n" : "black\n");

    update_status_window(gp);
}
*/
void do_play_toggle_eh_mode()
{
    struct userdata_s *d = gp->data;

    if (!TEST_FLAG(d->flags, CF_HUMAN))
    {
        if (!gp->hindex)
        {
            pgn_switch_side(gp, TRUE);
            d->play_mode = (d->play_mode) ? PLAY_HE : PLAY_EH;
            if (gp->side == BLACK)
                update_status_notify(gp, _("Press 'g' to start the game"));

            d->rotate = !d->rotate;
        }
        else
            message(NULL, ANY_KEY_STR,
                    _("You may only switch sides at the start of the \n"
                      "game. Press ^K or ^N to begin a new game."));
    }
}

void do_play_undo()
{
    struct userdata_s *d = gp->data;

    if (!pgn_history_total(gp->hp))
        return;

    if (keycount)
    {
        if (gp->hindex - keycount < 0)
            gp->hindex = 0;
        else
        {
            if (d->go_move)
                gp->hindex -= (keycount * 2) - 1;
            else
                gp->hindex -= keycount * 2;
        }
    }
    else
    {
        if (gp->hindex - 2 < 0)
            gp->hindex = 0;
        else
        {
            if (d->go_move)
                gp->hindex -= 1;
            else
                gp->hindex -= 2;
        }
    }

    pgn_history_free(gp->hp, gp->hindex);
    gp->hindex = pgn_history_total(gp->hp);
    pgn_board_update(gp, d->b, gp->hindex);

    if (d->engine && d->engine->status == ENGINE_READY)
    {
        char *fen = pgn_game_to_fen(gp, d->b);

        add_engine_command(gp, ENGINE_READY, "setboard %s\n", fen);
        free(fen);
        d->engine->status = ENGINE_READY;
    }

    update_history_window(gp);

    if (d->go_move)
    {
        pgn_switch_side(gp, FALSE);
        d->go_move--;
    }

    d->pm_undo = TRUE;
}

void do_play_toggle_pause()
{
    struct userdata_s *d = gp->data;

    if (!TEST_FLAG(d->flags, CF_HUMAN) && gp->turn != gp->side)
    {
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

    if (fm_loaded_file && gp->side != gp->turn)
    {
        pgn_switch_side(gp, FALSE);
        add_engine_command(gp, ENGINE_THINKING, "black\n");
    }

    add_engine_command(gp, ENGINE_THINKING, "go\n");

    // Completa la función para que permita seguir jugando al usarla.
    // Complete the function to allow continue playing when using.
    if (gp->side == gp->turn)
        pgn_switch_side(gp, FALSE);

    d->go_move++;
}

void do_play_config_command()
{
    int x, w;

    if (config.keys)
    {
        for (x = 0; config.keys[x]; x++)
        {
            if (config.keys[x]->c == input_c)
            {
                switch (config.keys[x]->type)
                {
                case KEY_DEFAULT:
                    add_engine_command(gp, -1, "%ls\n", config.keys[x]->str);
                    break;
                case KEY_SET:
                    if (!keycount)
                        break;

                    add_engine_command(gp, -1,
                                       "%ls %i\n", config.keys[x]->str,
                                       keycount);
                    keycount = 0;
                    break;
                case KEY_REPEAT:
                    if (!keycount)
                        break;

                    for (w = 0; w < keycount; w++)
                        add_engine_command(gp, -1, "%ls\n", config.keys[x]->str);
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
    pgn_reset_valid_moves(d->b);
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

    if (d->rotate)
    {
        rotate_position(&d->sp.row, &d->sp.col);
        rotate_position(&d->sp.srow, &d->sp.scol);
    }

    move_to_engine(gp);

    // Completa la función para que permita seguir jugando cuando se carga un
    // archivo pgn (con juego no terminado) que inicie con turno del lado
    // negro.
    // Complete the function to allow continue playing when loading a file
    // pgn (with unfinished game) you start to turn black side.
    if (gp->side != gp->turn)
        pgn_switch_side(gp, FALSE);

    if (d->rotate && d->sp.icon)
        rotate_position(&d->sp.srow, &d->sp.scol);

    d->go_move = 0;
    fm_loaded_file = FALSE;
    d->pm_undo = FALSE;
}

/*
 * Map a local click on the board table to rank/file (1..8) via cell rects.
 */
static int
board_click_to_square(int lx, int ly, int *rank, int *file)
{
    vk_grid_t *grid;
    int gcol, grow;
    int x, y, w, h;
    int ox, oy;

    if (!board_table || !gp || !gp->data)
        return 0;

    ox = board_table_ox();
    oy = board_table_oy();
    /* Clicks on coord gutters are not squares. */
    lx -= ox;
    ly -= oy;
    if (lx < 0 || ly < 0)
        return 0;

    grid = VK_GRID(board_table);
    for (grow = 0; grow < 8; grow++)
    {
        for (gcol = 0; gcol < 8; gcol++)
        {
            if (vk_grid_get_cell_rect(grid, gcol, grow, &x, &y, &w, &h) != 0)
                continue;
            if (lx >= x && lx < x + w && ly >= y && ly < y + h)
            {
                board_display_to_chess(gp->data, gcol, grow, rank, file);
                return 1;
            }
        }
    }
    return 0;
}

int cboard_board_mouse(int x, int y, mmask_t bstate)
{
    struct userdata_s *d;
    int bx, by, bw, bh, lx, ly;
    int rank, file;

    if (!board_vk || !gp || !gp->data)
        return 0;

    if (!(bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)))
        return 0;

    vk_widget_get_position((vk_widget_t *) board_vk, &bx, &by);
    vk_widget_get_metrics((vk_widget_t *) board_vk, &bw, &bh);
    if (x < bx || y < by || x >= bx + bw || y >= by + bh)
        return 0;

    lx = x - bx;
    ly = y - by;
    if (!board_click_to_square(lx, ly, &rank, &file))
        return 1; /* grid/border/coords — absorb, no move */

    d = gp->data;
    /* Move cursor (grid subfocus) to the clicked square. */
    d->c_row = rank;
    d->c_col = file;

    if (d->mode == MODE_PLAY)
    {
        /*
       * Same as keyboard: first click on a side-to-move piece selects it
       * (yellow). Second click on a destination commits. Empty/wrong-side
       * squares only move the cursor (green).
       */
        if (d->sp.icon)
            do_play_commit();
        else
            do_play_select();
    }

    update_all(gp);
    return 1;
}

void do_play_select()
{
    struct userdata_s *d = gp->data;

    if (!TEST_FLAG(d->flags, CF_HUMAN) && (!d->engine ||
                                           d->engine->status ==
                                               ENGINE_OFFLINE))
    {
        if (init_chess_engine(gp))
            return;
    }

    if (d->engine && d->engine->status == ENGINE_THINKING)
        return;

    if (d->sp.icon)
        do_play_cancel_selected();

    if (d->rotate)
        rotate_position(&d->c_row, &d->c_col);

    d->sp.icon = d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].icon;

    if (pgn_piece_to_int(d->sp.icon) == OPEN_SQUARE)
    {
        d->sp.icon = 0;
        return;
    }

    if (((islower(d->sp.icon) && gp->turn != BLACK) || (isupper(d->sp.icon) && gp->turn != WHITE)))
    {
        struct key_s **k;
        char *str = Malloc(512);

        for (k = play_keys; *k; k++)
        {
            if ((*k)->f == do_play_toggle_eh_mode)
                break;
        }

        snprintf(str, 512,
                 _("It is not your turn to move. You may switch playing sides by pressing \"%lc\"."),
                 *k ? (*k)->c : '?');
        message(NULL, ANY_KEY_STR, "%s", str);
        free(str);
        d->sp.icon = 0;
        return;
#if 0
      if (pgn_history_total (gp->hp))
    {
      message (NULL, ANY_KEY_STR, "%s",
           _("It is not your turn to move. You can switch sides "));
      d->sp.icon = 0;
      return;
    }
      else
    {
      if (pgn_tag_find (gp->tag, "FEN") != E_PGN_ERR)
        return;

      add_engine_command (gp, ENGINE_READY, "black\n");
      pgn_switch_turn (gp);

      if (gp->side != BLACK)
        pgn_switch_side (gp);
    }
#endif
    }

    d->sp.srow = d->c_row;
    d->sp.scol = d->c_col;

    if (config.validmoves)
        pgn_find_valid_moves(gp, d->b, d->sp.scol, d->sp.srow);

    if (d->rotate)
    {
        rotate_position(&d->c_row, &d->c_col);
        rotate_position(&d->sp.srow, &d->sp.scol);
    }

    CLEAR_FLAG(d->flags, CF_NEW);
    start_clock(gp);
}

static void
build_help_line_once(wchar_t *buf, wchar_t **pp, struct key_s *k, int t,
                     int nlen)
{
    wchar_t *p = *pp, *wc;
    int n;

    if (k->key)
        n = wcslen(k->key);
    else
        n = 1;

    while (n++ <= nlen)
        *p++ = ' ';

    *p = 0;

    if (k->key)
    {
        wcsncat(buf, k->key, t - 1);
        p = buf + wcslen(buf);
    }
    else
        *p++ = k->c;

    *p++ = ' ';
    *p++ = '-';
    *p++ = ' ';
    *p = 0;

    if (k->d)
        wcsncat(buf, k->d, t - 1);

    if (k->r)
    {
        wc = str_to_wchar("*");
        wcsncat(buf, wc, t - 1);
        free(wc);
    }

    wc = str_to_wchar("\n");
    wcscat(buf, wc);
    free(wc);
    *pp = buf + wcslen(buf);
}

static void
calc_help_len(struct key_s *k, int *t, int *nlen, int *len)
{
    if (!k->d)
        return;

    if (k->key)
    {
        if (wcslen(k->key) > *nlen)
        {
            *nlen = wcslen(k->key);
            *t += *nlen;
        }
        else
            (*t)++;
    }
    else
        (*t)++;

    if (k->d)
    {
        if (wcslen(k->d) > *len)
            *len = wcslen(k->d);
    }

    *t += *len;
    *t += k->r;
}

static wchar_t *
build_help(struct key_s **keys)
{
    int i, m = 0, nlen = 1, len, t;
    wchar_t *buf = NULL;
    wchar_t *p;
    wchar_t *more_help = str_to_wchar(_("more help"));
    const wchar_t *more_help_key = key_lookup(global_keys, do_global_help);
    struct key_s k;
    int mode = MODE_ANY;

    if (!keys)
    {
        free(more_help);
        return NULL;
    }

    for (i = t = len = 0; keys[i]; i++)
        calc_help_len(keys[i], &t, &nlen, &len);

    if (macros)
    {
        if (keys == play_keys)
            mode = MODE_PLAY;
        else if (keys == history_keys)
            mode = MODE_HISTORY;
        else if (keys == edit_keys)
            mode = MODE_EDIT;
        else
            mode = MODE_ANY;

        for (m = 0; macros[m]; m++)
        {
            if (macros[m]->mode == mode)
            {
                k.d = macros[m]->desc;
                k.r = k.c = 0;
                k.key = str_to_wchar(fancy_key_name(macros[m]->c));
                calc_help_len(&k, &t, &nlen, &len);
                free(k.key);
            }
        }
    }

    k.d = more_help;
    k.r = k.c = 0;
    k.key = (wchar_t *) more_help_key;
    calc_help_len(&k, &t, &nlen, &len);

    t += 4 + i + 1 + m + 1; // +1 for more_help
    buf = Malloc((t + 1) * sizeof(wchar_t));
    p = buf;

    for (i = 0; keys[i]; i++)
    {
        if (!keys[i]->d)
            continue;

        build_help_line_once(buf, &p, keys[i], t, nlen);
    }

    if (macros)
    {
        for (m = 0; macros[m]; m++)
        {
            if (macros[m]->mode == mode)
            {
                k.d = macros[m]->desc;
                k.r = k.c = 0;
                k.key = str_to_wchar(fancy_key_name(macros[m]->c));
                build_help_line_once(buf, &p, &k, t, nlen);
                free(k.key);
            }
        }
    }

    k.d = more_help;
    k.r = k.c = 0;
    k.key = (wchar_t *) more_help_key;
    build_help_line_once(buf, &p, &k, t, nlen);
    free(more_help);
    return buf;
}

void do_global_help()
{
    struct userdata_s *d = gp->data;
    wchar_t *buf;

    if (!d->global_help)
    {
        switch (d->mode)
        {
        case MODE_PLAY:
            do_play_help();
            return;
        case MODE_EDIT:
            do_edit_help();
            return;
        case MODE_HISTORY:
            do_history_help();
            return;
        default:
            break;
        }
    }

    d->global_help = 0;
    buf = build_help(global_keys);
    construct_message(_("Global Game Keys (* = can take a repeat count)"),
                      ANY_KEY_SCROLL_STR, 0, 0, NULL, NULL, buf, do_more_help,
                      0, 1, NULL, "%ls", buf);
}

void do_main_help(WIN *win)
{
    struct userdata_s *d = gp->data;

    switch (win->c)
    {
    case 'p':
        do_play_help();
        break;
    case 'h':
        do_history_help();
        break;
    case 'e':
        do_edit_help();
        break;
    case 'g':
        d->global_help = 1;
        do_global_help();
        break;
    default:
        break;
    }
}

static void
do_more_help(WIN *win)
{
    int i;

    for (i = 0; global_keys[i]; i++)
    {
        if (global_keys[i]->f == do_global_help && global_keys[i]->c == win->c)
            construct_message(_("Command Key Index"),
                              _("p/h/e/g or any other key to quit"), 0, 0,
                              NULL, NULL, NULL, do_main_help, 0, 0, NULL, "%s",
                              _(" p - play mode keys\n"
                                " h - history mode keys\n"
                                " e - board edit mode keys\n"
                                " g - global game keys"));
    }
}

static void
do_play_help()
{
    wchar_t *buf = build_help(play_keys);

    construct_message(_("Play Mode Keys (* = can take a repeat count)"),
                      ANY_KEY_SCROLL_STR, 0, 0, NULL, NULL,
                      buf, do_more_help, 0, 1, NULL, "%ls", buf);
}

void do_play_history_mode()
{
    struct userdata_s *d = gp->data;

    if (!pgn_history_total(gp->hp) ||
        (d->engine && d->engine->status == ENGINE_THINKING))
        return;

    d->mode = MODE_HISTORY;
    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
}

void do_play_edit_mode()
{
    struct userdata_s *d = gp->data;

    if (pgn_history_total(gp->hp))
        return;

    pgn_board_init_fen(gp, d->b, NULL);
    config.details++;
    d->mode = MODE_EDIT;
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

    if (pgn_piece_to_int(d->sp.icon) == OPEN_SQUARE)
    {
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
}

void do_edit_toggle_castle()
{
    struct userdata_s *d = gp->data;

    castling_state(gp, d->b, RANKTOBOARD(d->c_row),
                   FILETOBOARD(d->c_col),
                   d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].icon,
                   1);
}

void do_edit_insert()
{
    struct userdata_s *d = gp->data;

    construct_message(_("Insert Piece"),
                      _("P=pawn, R=rook, N=knight, B=bishop, Q=queen, K=king"),
                      0, 0, NULL, NULL, d->b, do_edit_insert_finalize, 0, 0,
                      NULL, "%s",
                      _("Type the piece letter to insert. Lowercase for a black piece, uppercase for a white piece."));
}

void do_edit_enpassant()
{
    struct userdata_s *d = gp->data;

    if (d->c_row == 6 || d->c_row == 3)
    {
        int n = d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].enpassant;

        pgn_reset_enpassant(d->b);
        if (!n)
            d->b[RANKTOBOARD(d->c_row)][FILETOBOARD(d->c_col)].enpassant = 1;
    }
}

static void
do_edit_help()
{
    wchar_t *buf = build_help(edit_keys);

    construct_message(_("Edit Mode Keys (* = can take a repeat count)"),
                      ANY_KEY_SCROLL_STR, 0, 0, NULL, NULL, buf, do_more_help,
                      0, 1, NULL, "%ls", buf);
}

void do_edit_exit()
{
    struct userdata_s *d = gp->data;
    char *fen = pgn_game_to_fen(gp, d->b);

    config.details--;
    pgn_tag_add(&gp->tag, (char *) "FEN", fen);
    free(fen);
    pgn_tag_add(&gp->tag, (char *) "SetUp", (char *) "1");
    pgn_tag_sort(gp->tag);
    pgn_board_update(gp, d->b, gp->hindex);
    d->mode = MODE_PLAY;
}

void really_do_annotate_finalize(struct input_data_s *in, struct userdata_s *d)
{
    HISTORY *h = in->data;
    int len;

    if (!in->str)
    {
        if (h->comment)
        {
            free(h->comment);
            h->comment = NULL;
        }
    }
    else
    {
        len = strlen(in->str);
        h->comment = Realloc(h->comment, len + 1);
        strncpy(h->comment, in->str, len);
        h->comment[len] = 0;
    }

    free(in->str);
    free(in);
    SET_FLAG(d->flags, CF_MODIFIED);
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

    if (init || !firstrun)
    {
        if (!firstrun)
            regfree(&r);

        if ((ret = regcomp(&r, moveexp, REG_EXTENDED | REG_NOSUB)) != 0)
        {
            regerror(ret, &r, errbuf, sizeof(errbuf));
            cmessage(_("Error Compiling Regular Expression"), ANY_KEY_STR,
                     "%s", errbuf);
            return;
        }

        firstrun = 1;
    }

    if ((n = find_move_exp(gp, r,
                           (which == -1) ? 0 : 1,
                           (keycount) ? keycount : 1)) == -1)
        return;

    gp->hindex = n;
    pgn_board_update(gp, d->b, gp->hindex);
}

void do_find_move_exp(WIN *win)
{
    struct input_data_s *in = win->data;
    int *n = in->data;
    int which = *n;

    if (in->str)
    {
        strncpy(moveexp, in->str, sizeof(moveexp) - 1);
        moveexp[sizeof(moveexp) - 1] = 0;
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
}

void do_move_jump(WIN *win)
{
    struct input_data_s *in = win->data;

    if (!in->str || !isinteger(in->str))
    {
        if (in->str)
            free(in->str);

        free(in);
        return;
    }

    do_move_jump_finalize(atoi(in->str));
    free(in->str);
    free(in);
}

struct history_menu_s
{
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

    for (i = 0; h[i]; i++)
    {
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
        for (n = 0; hmenu[n]; n++)
            ;
    else
        depth = 0;

    for (i = 0; i < t; i++)
    {
        hmenu = Realloc(hmenu, (n + 2) * sizeof(struct history_menu_s *));
        hmenu[n] = Malloc(sizeof(struct history_menu_s));
        snprintf(buf, sizeof(buf), "%c%s%s", (turn == WHITE) ? 'W' : 'B',
                 hp[i]->move, (hp[i]->comment || hp[i]->nag[0]) ? " !" : "");
        hmenu[n]->line = strdup(buf);
        hmenu[n]->hindex = i;
        hmenu[n]->indent = 0;
        hmenu[n]->ravlevel = depth;
        hmenu[n]->move = (n && depth > hmenu[n - 1]->ravlevel) ? m++ : m;
        n++;
        hmenu[n] = NULL;

#if 0
      if (hp[i]->rav)
    {
      depth++;
      get_history_data (hp[i]->rav, &hmenu, m, turn);
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

struct menu_item_s **
get_history_items(WIN *win)
{
    struct menu_input_s *m = win->data;
    GAME g = m->data;
    struct userdata_s *d = g->data;
    struct history_menu_s **hm = d->data;
    struct menu_item_s **items = m->items;
    int i;

    if (!hm)
    {
        get_history_data(g->history, &hm, 0,
                         TEST_FLAG(g->flags, GF_BLACK_OPENING));
        m->selected = g->hindex - 1;

        if (m->selected < 0)
            m->selected = 0;

        m->draw_exit_func = history_draw_update;
    }

    d->data = hm;

    if (items)
    {
        for (i = 0; items[i]; i++)
            free(items[i]);

        free(items);
        items = NULL;
    }

    for (i = 0; hm[i]; i++)
    {
        items = Realloc(items, (i + 2) * sizeof(struct menu_item_s *));
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

    for (i = 0; hm[i]; i++)
    {
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

    for (t = 0; hm[t]; t++)
        ;

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

    for (t = 0; hm[t]; t++)
        ;

    if (m->selected - 1 < 0)
        n = t - 1;
    else
        n = hm[m->selected - 1]->hindex;

    n++;
    g->hindex = n;
}

void history_menu_help(struct menu_input_s *m)
{
    message(_("History Menu Help"), ANY_KEY_STR, "%s",
            _("    UP/DOWN - previous/next menu item\n"
              "   HOME/END - first/last menu item\n"
              "  PGDN/PGUP - next/previous page\n"
              "  a-zA-Z0-9 - jump to item\n"
              "     CTRL-a - annotate the selected move\n"
              "      ENTER - view annotation\n"
              "     CTRL-d - toggle board details\n"
              "   ESCAPE/M - return to move history"));
}

void do_annotate_move(HISTORY *hp)
{
    char buf[COLS - 4];
    struct input_data_s *in;

    snprintf(buf, sizeof(buf), "%s \"%s\"", _("Editing Annotation for"),
             hp->move);
    in = Calloc(1, sizeof(struct input_data_s));
    in->data = hp;
    in->efunc = do_annotate_finalize;
    construct_input(buf, hp->comment, MAX_PGN_LINE_LEN / INPUT_WIDTH, 0,
                    _("Type CTRL-t to edit NAG"), edit_nag, NULL,
                    CTRL_KEY('T'), in, -1, NULL, -1);
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
    get_history_data(g->history, &hm, 0,
                     TEST_FLAG(g->flags, GF_BLACK_OPENING));
    d->data = hm;
    pushkey = REFRESH_MENU;
}

void history_menu_annotate(struct menu_input_s *m)
{
    GAME g = m->data;
    char buf[COLS - 4];
    struct input_data_s *in;
    HISTORY *hp = g->history[m->selected]; // FIXME RAV

    snprintf(buf, sizeof(buf), "%s \"%s\"", _("Editing Annotation for"),
             hp->move);
    in = Calloc(1, sizeof(struct input_data_s));
    in->data = hp;
    in->moredata = m->data;
    in->efunc = history_menu_annotate_finalize;
    construct_input(buf, hp->comment, MAX_PGN_LINE_LEN / INPUT_WIDTH, 0,
                    _("Type CTRL-t to edit NAG"), edit_nag, NULL,
                    CTRL_KEY('T'), in, -1, NULL, -1);
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
    attr_t attrs;
    short pair;
    int total;

    for (total = 0; hm[total]; total++)
        ;
    wattr_get(win->w, &attrs, &pair, NULL);
    wattroff(win->w, COLOR_PAIR(pair));
    mvwaddch(win->w, m->print_line, 1,
             *p == 'W' ? *p | mix_cp(CP_BOARD_WHITE, CP_HISTORY_WINDOW,
                                     ATTRS(CP_BOARD_WHITE),
                                     A_FG_B_BG)
                       : *p | mix_cp(CP_BOARD_BLACK,
                                     CP_HISTORY_WINDOW,
                                     ATTRS(CP_BOARD_BLACK),
                                     A_FG_B_BG));
    p++;

    if (h->hindex == 0 && line == 0)
        waddch(win->w, ACS_ULCORNER | CP_HISTORY_MENU_LG);
    else if ((!hm[h->hindex + (win->rows - 5) + 1] && line == win->rows - 5) ||
             (m->top + line == total - 1))
        waddch(win->w, ACS_LLCORNER | CP_HISTORY_MENU_LG);
    else if (hm[m->top + 1]->ravlevel != h->ravlevel || !h->ravlevel)
        waddch(win->w, ACS_LTEE | CP_HISTORY_MENU_LG);
    else
        waddch(win->w, ACS_VLINE | CP_HISTORY_MENU_LG);

    wattron(win->w, COLOR_PAIR(pair) | attrs);

    for (i = 2; *p; p++, i++)
        waddch(win->w, (*p == '!') ? *p | A_BOLD : *p);

    while (i++ < win->cols - 2)
        waddch(win->w, ' ');
}

void history_menu(GAME g)
{
    struct menu_key_s **keys = NULL;

    add_menu_key(&keys, KEY_ESCAPE, history_menu_quit);
    add_menu_key(&keys, 'M', history_menu_quit);
    add_menu_key(&keys, KEY_UP, history_menu_prev);
    add_menu_key(&keys, KEY_DOWN, history_menu_next);
    add_menu_help_key(&keys, history_menu_help);
    add_menu_key(&keys, CTRL_KEY('a'), history_menu_annotate);
    add_menu_key(&keys, CTRL_KEY('d'), history_menu_details);
    add_menu_key(&keys, '\n', history_menu_view_annotation);
    construct_menu(MEGA_BOARD ? LINES - HISTORY_HEIGHT_MB : LINES, TAG_WIDTH,
                   0, config.boardleft ? BOARD_WIDTH : 0,
                   _("Move History Tree"), 1, get_history_items, keys, g,
                   history_menu_print, history_menu_exit, history_menu_resize);
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

void do_history_rotate_board()
{
    struct userdata_s *d = gp->data;
    d->rotate = !d->rotate;
}

void do_history_jump_next()
{
    struct userdata_s *d = gp->data;

    pgn_history_next(gp, d->b, (keycount > 0) ? config.jumpcount * keycount * movestep : config.jumpcount * movestep);
}

void do_history_jump_prev()
{
    struct userdata_s *d = gp->data;

    pgn_history_prev(gp, d->b, (keycount) ? config.jumpcount * keycount * movestep : config.jumpcount * movestep);
}

void do_history_prev()
{
    struct userdata_s *d = gp->data;

    pgn_history_prev(gp, d->b, (keycount) ? keycount * movestep : movestep);
}

void do_history_next()
{
    struct userdata_s *d = gp->data;

    pgn_history_next(gp, d->b, (keycount) ? keycount * movestep : movestep);
}

void do_history_mode_finalize(struct userdata_s *d)
{
    pushkey = 0;
    d->mode = MODE_PLAY;
}

void do_history_mode_confirm(WIN *win)
{
    struct userdata_s *d = gp->data;
    wchar_t str[] = {win->c, 0};

    if (!wcscmp(str, resume_wchar))
    {
        pgn_history_free(gp->hp, gp->hindex);
        pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
    }
#if 0
case 'C':
case 'c':
  if (pgn_history_rav_new (gp, d->b, gp->hindex) != E_PGN_OK)
    return;

  break;
#endif
    else
        return;

    if (!TEST_FLAG(d->flags, CF_HUMAN))
    {
        char *fen = pgn_game_to_fen(gp, d->b);

        add_engine_command(gp, ENGINE_READY, "setboard %s\n", fen);
        free(fen);
    }

    do_history_mode_finalize(d);
}

void do_history_toggle()
{
    struct userdata_s *d = gp->data;

    // FIXME Resuming from previous history could append to a RAV.
    if (gp->hindex != pgn_history_total(gp->hp))
    {
        if (!pushkey)
            construct_message(NULL, _("What would you like to do?"), 0, 1,
                              NULL, NULL, NULL, do_history_mode_confirm, 0, 0,
                              NULL, _("The current move is not the final move of this round. Press \"%ls\" to resume a game from the current move and discard future moves or any other key to cancel."),
                              resume_wchar);
        return;
    }
    else
    {
        if (TEST_FLAG(gp->flags, GF_GAMEOVER))
            return;
    }

    if (gp->side != gp->turn)
    {
        d->play_mode = PLAY_EH;
    }
    else
    {
        d->play_mode = PLAY_HE;
        d->rotate = FALSE;
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

static void
do_history_help()
{
    wchar_t *buf = build_help(history_keys);

    construct_message(_("History Mode Keys (* = can take a repeat count)"),
                      ANY_KEY_SCROLL_STR, 0, 0, NULL, NULL,
                      buf, do_more_help, 0, 1, NULL, "%ls", buf);
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

    if (!*moveexp || which == 0)
    {
        construct_input(_("Find Move Text Expression"), NULL, 1, 0, NULL, NULL,
                        NULL, 0, in, INPUT_HIST_MOVE_EXP, NULL, -1);
        return;
    }

    free(p);
    free(in);
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

    if (!keycount)
    {
        in = Calloc(1, sizeof(struct input_data_s));
        in->efunc = do_move_jump;

        construct_input(_("Jump to Move Number"), NULL, 1, 1, NULL,
                        NULL, NULL, 0, in, -1, NULL, 0);
        return;
    }

    do_move_jump_finalize(keycount);
}

static void
free_userdata_once(GAME g)
{
    struct userdata_s *d = g->data;

    if (!d)
        return;

    if (d->engine)
        free_engine(g);

#ifdef WITH_LIBPERL
    if (d->perlfen)
        free(d->perlfen);

    if (d->oldfen)
        free(d->oldfen);
#endif

    free(d);
    g->data = NULL;
}

static void
free_userdata()
{
    int i;

    for (i = 0; i < gtotal; i++)
    {
        free_userdata_once(game[i]);
        game[i]->data = NULL;
    }
}

void update_loading_window(int n)
{
    char buf[16];

    if (!loading_vk)
    {
        loading_vk =
            cboard_ui_widget_new(3, COLS / 2, CALCPOSY(3),
                                 CALCPOSX(COLS / 2));
        loadingw = cboard_ui_widget_canvas(loading_vk);
        wbkgd(loadingw, CP_MESSAGE_WINDOW);
    }

    cboard_ui_widget_raise(loading_vk);
    wmove(loadingw, 0, 0);
    wclrtobot(loadingw);
    wattron(loadingw, CP_MESSAGE_BORDER);
    box(loadingw, ACS_VLINE, ACS_HLINE);
    wattroff(loadingw, CP_MESSAGE_BORDER);
    mvwprintw(loadingw, 1, CENTER_INT((COLS / 2), 11 + strlen(itoa(gtotal, buf))),
              _("Loading... %i%% (%i games)"), n, gtotal);
    cboard_ui_refresh();
}

static void
init_userdata_once(GAME g, int n)
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

    if (*start > *end)
    {
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
    d->rotate = FALSE;
    d->go_move = 0;
}

void do_new_game_from_scratch(WIN *win)
{
    wchar_t str[] = {win->c, 0};

    if (wcscmp(str, yes_wchar))
        return;

    stop_clock();
    free_userdata();
    pgn_parse(NULL);
    gp = game[gindex];
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
    if (d->mode != MODE_EDIT)
        pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
}

void do_game_delete_confirm(WIN *win)
{
    int *n;
    wchar_t str[] = {win->c, 0};

    if (wcscmp(str, yes_wchar))
    {
        free(win->data);
        return;
    }

    n = (int *) win->data;
    do_game_delete_finalize(*n);
    free(win->data);
}

void do_game_delete()
{
    char *tmp = NULL;
    int i, n;
    struct userdata_s *d;
    int *p;

    if (gtotal < 2)
    {
        cmessage(NULL, ANY_KEY_STR, "%s", _("Cannot delete last game."));
        return;
    }

    tmp = NULL;

    for (i = n = 0; i < gtotal; i++)
    {
        d = game[i]->data;

        if (TEST_FLAG(d->flags, CF_DELETE))
            n++;
    }

    if (!n)
        tmp = _("Delete the current game?");
    else
    {
        if (n == gtotal)
        {
            cmessage(NULL, ANY_KEY_STR, "%s", _("Cannot delete last game."));
            return;
        }

        tmp = _("Delete all games marked for deletion?");
    }

    if (config.deleteprompt)
    {
        p = Malloc(sizeof(int));
        *p = n;
        construct_message(NULL, _("[ Yes or No ]"), 1, 1, NULL, NULL, p,
                          do_game_delete_confirm, 0, 0, NULL, "%s", tmp);
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
    {
        update_status_notify(gp, "%s", _("No matches found"));
        return;
    }

    gindex = n;
    d = gp->data;

    if (pgn_history_total(gp->hp))
        d->mode = MODE_HISTORY;

    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
}

void do_find_game_exp(WIN *win)
{
    struct input_data_s *in = win->data;
    int *n = in->data;
    int c = *n;

    if (in->str)
    {
        strncpy(gameexp, in->str, sizeof(gameexp));
        gameexp[sizeof(gameexp) - 1] = 0;

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
    gp = game[gindex];
    d = gp->data;
    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
    update_status_notify(gp, NULL);
}

void do_game_jump(WIN *win)
{
    struct input_data_s *in = win->data;

    if (!in->str || !isinteger(in->str))
    {
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
    struct input_data_s *in = win->data;
    char *tmp = in->str;
    struct userdata_s *d;
    PGN_FILE *pgn = NULL;
    int n;

    if (!in->str)
    {
        free(in);
        return;
    }

    if ((tmp = pathfix(tmp)) == NULL)
        goto done;

    n = pgn_open(tmp, "r", &pgn);

    if (n == E_PGN_ERR)
    {
        cmessage(ERROR_STR, ANY_KEY_STR, "%s\n%s", tmp, strerror(errno));
        goto done;
    }
    else if (n == E_PGN_INVALID)
    {
        cmessage(ERROR_STR, ANY_KEY_STR, "%s\n%s", tmp,
                 _("Not a regular file"));
        goto done;
    }

    free_userdata();

    if (pgn_parse(pgn) == E_PGN_ERR)
    {
        cboard_ui_widget_destroy(loading_vk);
        loading_vk = NULL;
        loadingw = NULL;
        init_userdata();
        goto done;
    }

    cboard_ui_widget_destroy(loading_vk);
    loading_vk = NULL;
    loadingw = NULL;
    init_userdata();
    strncpy(loadfile, tmp, sizeof(loadfile));
    loadfile[sizeof(loadfile) - 1] = 0;
    gp = game[gindex];
    d = gp->data;

    if (pgn_history_total(gp->hp))
        d->mode = MODE_HISTORY;

    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));

    fm_loaded_file = TRUE;
    d->rotate = FALSE;

done:
    pgn_close(pgn);

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

    if (pgn_is_compressed(tmp) == E_PGN_ERR)
    {
        p = tmp + strlen(tmp) - 1;

        if (*p != 'n' || *(p - 1) != 'g' || *(p - 2) != 'p' || *(p - 3) != '.')
        {
            snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
            tmp = tfile;
        }
    }

    /*
   * When in edit mode, update the FEN tag.
   */
    if (n == -1)
    {
        for (i = 0; i < gtotal; i++)
        {
            d = game[i]->data;

            if (d->mode == MODE_EDIT)
            {
                char *fen = pgn_game_to_fen(game[i], d->b);

                pgn_tag_add(&game[i]->tag, (char *) "FEN", fen);
                free(fen);
            }
        }
    }
    else
    {
        d = game[n]->data;

        if (d->mode == MODE_EDIT)
        {
            char *fen = pgn_game_to_fen(game[n], d->b);

            pgn_tag_add(&game[n]->tag, (char *) "FEN", fen);
            free(fen);
        }
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

    construct_input(_("Save Game Filename"), loadfile, 1, 1,
                    _("Type TAB for file browser"), file_browser, NULL, '\t',
                    in, INPUT_HIST_FILE, NULL, -1);
}

void do_game_save_multi_confirm(WIN *win)
{
    int i;
    wchar_t str[] = {win->c, 0};

    if (!wcscmp(str, current_wchar))
        i = gindex;
    else if (!wcscmp(str, all_wchar))
        i = -1;
    else
    {
        update_status_notify(gp, "%s", _("Save game aborted."));
        return;
    }

    do_get_game_save_input(i);
}

void do_global_about()
{
    cmessage(_("ABOUT"), ANY_KEY_STR,
             _("%s\nUsing %s with %i colors and %i color pairs\n%s\n%s"),
             PACKAGE_STRING, curses_version(), COLORS, COLOR_PAIRS,
             COPYRIGHT, CBOARD_URL);
}

void global_game_next_prev(int which)
{
    struct userdata_s *d;

    game_next_prev(gp, (which == 1) ? 1 : 0, (keycount) ? keycount : 1);
    d = gp->data;

    if (delete_count)
    {
        if (which == 1)
        {
            markend = markstart + delete_count;
            delete_count = 0;
        }
        else
        {
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

    if (!*gameexp || which == 0)
    {
        construct_input(_("Find Game by Tag Expression"), NULL, 1, 0,
                        _("[name expression:]value expression"), NULL, NULL, 0,
                        in, INPUT_HIST_GAME_EXP, NULL, -1);
        return;
    }

    free(p);
    free(in);
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
    if (gtotal < 2)
        return;

    if (!keycount)
    {
        struct input_data_s *in;

        in = Calloc(1, sizeof(struct input_data_s));
        in->efunc = do_game_jump;
        construct_input(_("Jump to Game Number"), NULL, 1, 1, NULL, NULL, NULL,
                        0, in, -1, NULL, 0);
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

    if (keycount && delete_count == 0)
    {
        markstart = gindex;
        delete_count = keycount;
        update_status_notify(gp, "%s (delete)", status.notify);
        return;
    }

    if (markstart >= 0 && markend >= 0)
    {
        for (i = markstart; i < markend; i++)
        {
            if (toggle_delete_flag(i))
            {
                return;
            }
        }

        gindex = (delete_count < 0) ? markstart : i - 1;
    }
    else
    {
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
    construct_input(_("Load Filename"), NULL, 1, 1,
                    _("Type TAB for file browser"), file_browser, NULL, '\t',
                    in, INPUT_HIST_FILE, NULL, -1);
}

void do_global_save_game()
{
    if (gtotal > 1)
    {
        construct_message(NULL, _("What would you like to do?"), 0, 1,
                          NULL, NULL, NULL, do_game_save_multi_confirm, 0, 0,
                          NULL, _("There is more than one game loaded. Press \"%ls\" to save the current game, \"%ls\" to save all games or any other key to cancel."),
                          current_wchar, all_wchar);
        return;
    }

    do_get_game_save_input(-1);
}

void do_global_new_game()
{
    do_new_game();
}

void copy_game_common(int fen)
{
    int g = gindex;
    int i, n;
    struct userdata_s *d = gp->data;
    char *fentag = fen ? pgn_game_to_fen(gp, d->b) : NULL;

    do_global_new_game();
    d = gp->data;
    n = pgn_tag_total(game[g]->tag);

    for (i = 0; i < n; i++)
        pgn_tag_add(&gp->tag, game[g]->tag[i]->name, game[g]->tag[i]->value);

    if (fentag)
    {
        pgn_tag_add(&gp->tag, (char *) "SetUp", (char *) "1");
        pgn_tag_add(&gp->tag, (char *) "FEN", fentag);
        free(fentag);
        pgn_tag_sort(gp->tag);
    }
    else
    {
        pgn_board_init_fen(gp, d->b, NULL);
        n = pgn_history_total(game[g]->history);

        // FIXME RAV
        for (i = 0; i < n; i++)
        {
            char *frfr = NULL;
            char *move = strdup(game[g]->history[i]->move);

            if (pgn_parse_move(gp, d->b, &move, &frfr) != E_PGN_OK)
            {
                free(move);
                SET_FLAG(gp->flags, GF_PERROR);
                return;
            }

            pgn_history_add(gp, d->b, move);
            free(move);
            free(frfr);
            pgn_switch_turn(gp);
        }
    }

    pgn_board_update(gp, d->b, pgn_history_total(gp->hp));
}

void do_global_copy_game()
{
    copy_game_common(0);
}

void do_global_copy_game_fen()
{
    copy_game_common(1);
}

void do_global_new_all()
{
    construct_message(NULL, _("[ Yes or No ]"), 1, 1, NULL, NULL, NULL,
                      do_new_game_from_scratch, 0, 0, NULL, "%s",
                      _("Really start a new game from scratch?"));
}

void do_quit(WIN *win)
{
    wchar_t str[] = {win->c, 0};
    int n = wcscmp(str, yes_wchar);

    if (n)
        return;

    quit = 1;
}

void do_global_quit()
{
    if (config.exitdialogbox)
        construct_confirm(_("Quit"), _("Want to Quit?"), do_quit);
    else
        quit = 1;
}

void do_play_toggle_valid_moves()
{
    config.validmoves = config.validmoves ? 0 : 1;
    if (!config.validmoves && gp && gp->data)
    {
        struct userdata_s *d = gp->data;

        pgn_reset_valid_moves(d->b);
    }
    update_status_notify(gp,
                         config.validmoves ? _("Valid moves: on")
                                           : _("Valid moves: off"));
    update_all(gp);
}

void do_global_toggle_engine_window()
{
    if (!engine_vk)
    {
        engine_vk = cboard_ui_widget_new(LINES, COLS, 0, 0);
        enginew = cboard_ui_widget_canvas(engine_vk);
        window_draw_title(enginew, _("Engine IO Window"), COLS,
                          CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);
        cboard_ui_widget_hide(engine_vk);
    }

    if (cboard_ui_widget_hidden(engine_vk))
    {
        update_engine_window(gp);
        cboard_ui_widget_raise(engine_vk);
    }
    else
    {
        cboard_ui_widget_hide(engine_vk);
    }
}

void do_global_toggle_board_details()
{
    do_board_details();
}

void do_play_toggle_strict_castling()
{
    do_toggle_strict_castling();
}

// Global and other keys.
static int
globalkeys()
{
    struct userdata_s *d = gp->data;
    int i;

    /*
   * These cannot be modified and other game mode keys cannot conflict with
   * these.
   */
    switch (input_c)
    {
    case KEY_ESCAPE:
        d->sp.icon = d->sp.srow = d->sp.scol = 0;
        markend = markstart = 0;

        if (keycount)
        {
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

        update_status_notify(gp, _("Repeat %i"), keycount);
        return -1;
    case KEY_UP:
        if (d->mode == MODE_HISTORY)
            return 0;

        if (keycount)
            d->c_row += keycount;
        else
            d->c_row++;

        if (d->c_row > 8)
            d->c_row = 1;

        return 1;
    case KEY_DOWN:
        if (d->mode == MODE_HISTORY)
            return 0;

        if (keycount)
        {
            d->c_row -= keycount;
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

        if (keycount)
            d->c_col -= keycount;
        else
            d->c_col--;

        if (d->c_col < 1)
            d->c_col = 8;

        return 1;
    case KEY_RIGHT:
        if (d->mode == MODE_HISTORY)
            return 0;

        if (keycount)
            d->c_col += keycount;
        else
            d->c_col++;

        if (d->c_col > 8)
            d->c_col = 1;

        return 1;
    case KEY_RESIZE:
        return 1;
    case 0:
    default:
        for (i = 0; global_keys[i]; i++)
        {
            if (input_c == global_keys[i]->c && global_keys[i]->f)
            {
                (*global_keys[i]->f)();
                return 1;
            }
        }
        break;
    }

    return 0;
}

#ifdef WITH_LIBPERL
static void
perl_error(const char *fmt, ...)
{
    va_list ap;
    char *buf;

    va_start(ap, fmt);
    vasprintf(&buf, fmt, ap);
    va_end(ap);

    message(ERROR_STR, ANY_KEY_STR, "%s", buf);
    free(buf);
}

static void
do_perl_finalize(WIN *win)
{
    struct input_data_s *in = win->data;
    GAME g = in->data;
    struct userdata_s *d = g->data;
    char *filename;
    char *result = NULL;
    char *arg = NULL;
    int n;

    asprintf(&filename, "%s/perl.pl", config.datadir);

    if (!in->str)
        goto done;

    if (perl_init_file(filename, perl_error))
        goto done;

    arg = pgn_game_to_fen(g, d->b);

    if (perl_call_sub(trim(in->str), arg, &result))
        goto done;

    d->perlfen = pgn_game_to_fen(g, d->b);
    d->perlflags = g->flags;

    if (pgn_board_init_fen(g, d->b, result) != E_PGN_OK)
    {
        message(ERROR_STR, ANY_KEY_STR, "%s", _("FEN parse error."));
        pgn_board_init_fen(g, d->b, d->perlfen);
        g->flags = d->perlflags;
        free(d->perlfen);
        d->perlfen = NULL;
        goto done;
    }

    SET_FLAG(d->flags, CF_PERL);
    n = pgn_tag_find(g->tag, "FEN");

    if (n != E_PGN_ERR)
        d->oldfen = strdup(g->tag[n]->value);

    pgn_tag_add(&g->tag, (char *) "FEN", result);
    update_status_notify(g, "%s", ANY_KEY_STR);
    update_all(g);

done:
    free(result);
    free(arg);
    free(in->str);
    free(in);
    free(filename);
}

void do_global_perl()
{
    struct input_data_s *in;

    in = Calloc(1, sizeof(struct input_data_s));
    in->data = gp;
    in->efunc = do_perl_finalize;
    construct_input(_("PERL Subroutine Filter"), NULL, 1, 0, NULL, NULL, NULL,
                    0, in, INPUT_HIST_PERL, NULL, -1);
}
#endif

/*
 * A macro may contain a key that belongs to another macro so macro_match will
 * need to be updated to the new index of the matching macro.
 */
static void
find_macro(struct userdata_s *d)
{
    int i;

    /*
   * Macros can't contain macros when in a window.
   */
    if (window_depth() > 0)
        return;

again:
    for (i = 0; macros[i]; i++)
    {
        if ((macros[i]->mode == -1 || macros[i]->mode == d->mode) &&
            input_c == macros[i]->c)
        {
            input_c = macros[i]->keys[macros[i]->n++];

            if (!macro_depth_n && macro_match > -1)
            {
                macro_depth = Realloc(macro_depth, (macro_depth_n + 1) * sizeof(int));
                macro_depth[macro_depth_n++] = macro_match;
            }

            macro_depth = Realloc(macro_depth, (macro_depth_n + 1) * sizeof(int));
            macro_depth[macro_depth_n++] = i;
            macro_match = i;
            goto again;
        }
    }
}

/*
 * Resets the position in each macro to the first key.
 */
static void
reset_macros()
{
    int i;
    struct userdata_s *d = gp->data;

again:
    if (macro_depth_n > 0)
    {
        macro_depth_n--;
        macro_match = macro_depth[macro_depth_n];

        if (macros[macro_match]->n >= macros[macro_match]->total)
            goto again;

        input_c = macros[macro_match]->keys[macros[macro_match]->n++];
        find_macro(d);
        return;
    }

    for (i = 0; macros[i]; i++)
        macros[i]->n = 0;

    free(macro_depth);
    macro_depth = NULL;
    macro_depth_n = 0;
    macro_match = -1;
}

void game_loop()
{
    struct userdata_s *d;

    macro_match = -1;
    gindex = gtotal - 1;
    gp = game[gindex];
    d = gp->data;

    if (pgn_history_total(gp->hp))
        d->mode = MODE_HISTORY;
    else
    {
        d->mode = MODE_PLAY;
        d->play_mode = PLAY_HE;
    }

    d->rotate = FALSE;
    d->go_move = 0;
    d->pm_undo = FALSE;
    d->pm_frfr[0] = '\0';

    if (d->mode == MODE_HISTORY)
        pgn_board_update(gp, d->b, pgn_history_total(gp->hp));

    update_status_notify(gp, _("Type %ls for help"),
                         key_lookup(global_keys, do_global_help));
    movestep = 2;
    flushinp();
    update_all(gp);
    wtimeout(boardw, WINDOW_TIMEOUT);

    while (!quit)
    {
        int n = 0, i;
        char fdbuf[8192] = {0};
        int len;
        struct timeval tv = {0, 0};
        fd_set rfds, wfds;
        WIN *win = NULL;
        WINDOW *wp = NULL;

        FD_ZERO(&rfds);
        FD_ZERO(&wfds);

        for (i = 0; i < gtotal; i++)
        {
            d = game[i]->data;

            if (d->engine && d->engine->pid != -1)
            {
                if (d->engine->fd[ENGINE_IN_FD] > 2)
                {
                    if (d->engine->fd[ENGINE_IN_FD] > n)
                        n = d->engine->fd[ENGINE_IN_FD];

                    FD_SET(d->engine->fd[ENGINE_IN_FD], &rfds);
                }

                if (d->engine->fd[ENGINE_OUT_FD] > 2)
                {
                    if (d->engine->fd[ENGINE_OUT_FD] > n)
                        n = d->engine->fd[ENGINE_OUT_FD];

                    FD_SET(d->engine->fd[ENGINE_OUT_FD], &wfds);
                }
            }
        }

        if (n)
        {
            if ((n = select(n + 1, &rfds, &wfds, NULL, &tv)) > 0)
            {
                for (i = 0; i < gtotal; i++)
                {
                    d = game[i]->data;

                    if (d->engine && d->engine->pid != -1)
                    {
                        if (FD_ISSET(d->engine->fd[ENGINE_IN_FD], &rfds))
                        {
                            len = read(d->engine->fd[ENGINE_IN_FD], fdbuf,
                                       sizeof(fdbuf));

                            if (len > 0)
                            {
                                if (d->engine->iobuf)
                                    d->engine->iobuf =
                                        Realloc(d->engine->iobuf,
                                                d->engine->len + len + 1);
                                else
                                    d->engine->iobuf = Calloc(1, len + 1);

                                memcpy(&(d->engine->iobuf[d->engine->len]),
                                       &fdbuf, len);
                                d->engine->len += len;
                                d->engine->iobuf[d->engine->len] = 0;

                                /*
                   * The fdbuf is full or no newline
                   * was found. So we'll append the next
                   * read() to this games buffer.
                   */
                                if (d->engine->iobuf[d->engine->len - 1] !=
                                    '\n')
                                    continue;

                                parse_engine_output(game[i], d->engine->iobuf);
                                free(d->engine->iobuf);
                                d->engine->iobuf = NULL;
                                d->engine->len = 0;
                            }
                            else if (len == 0
                                     || (len == -1 && errno != EAGAIN))
                            {
                                /*
                                 * EOF (engine exited) or hard I/O error.
                                 * free_engine tears down FDs/pid so a later
                                 * restart can surface the same failure again.
                                 */
                                if (len == 0)
                                    cmessage(ERROR_STR, ANY_KEY_STR, "%s",
                                             _("Engine closed the connection"));
                                else
                                    cmessage(ERROR_STR, ANY_KEY_STR,
                                             "Engine read(): %s",
                                             strerror(errno));
                                free_engine(game[i]);
                                break;
                            }
                        }

                        if (FD_ISSET(d->engine->fd[ENGINE_OUT_FD], &wfds))
                        {
                            if (d->engine->queue)
                                send_engine_command(game[i]);
                        }
                    }
                }
            }
            else
            {
                if (n == -1)
                    cmessage(ERROR_STR, ANY_KEY_STR, "select(): %s",
                             strerror(errno));
                /* timeout */
            }
        }

        gp = game[gindex];
        d = gp->data;

        /*
       * Input via vk_kmio_fetch (keyboard + SGR mouse).  Timeout still
       * comes from wtimeout(stdscr).  Never read from a VDK canvas
       * (auto-wrefresh would paint over the composite).
       *
       * Geometry changes are handled only on KEY_RESIZE (VDK/VWM style).
       * Do not poll LINES/COLS here — that ran a second resize cascade and
       * raced ncurses resizeterm inside wrefresh.
       */
        wp = stdscr;
        wtimeout(stdscr, WINDOW_TIMEOUT);

        if (pushkey)
        {
            input_c = pushkey;
            pushkey = 0;
        }
        else if (macros && macro_match >= 0)
        {
            if (macros[macro_match]->n >= macros[macro_match]->total)
            {
                reset_macros();
                goto refresh;
            }
            input_c = macros[macro_match]->keys[macros[macro_match]->n++];
            find_macro(d);
        }
        else
        {
            MEVENT mev;
            int pev;

            pev = cboard_ui_poll_event(&input_c, &mev);
            if (pev == 0)
                continue;
            if (pev == 2)
            {
                if (cboard_mouse_handle(&mev))
                {
                    if (macro_match == -1)
                        keycount = 0;
                    /* Modal may have closed via pushkey; drain next loop. */
                    if (pushkey)
                        continue;
                    goto refresh;
                }
                continue;
            }
            if (input_c == KEY_RESIZE)
            {
                /*
           * One cascade only: vk_screen_resize + chrome layout +
           * window_resize_all + update_all.  Do not also dispatch
           * KEY_RESIZE into the modal key handler (rfuncs already ran).
           */
                do_window_resize();
                continue;
            }
        }

        pushkey = 0;

        /*
         * Top modal sinks keyboard until dismissed (same rule as mouse).
         * Re-sample after the poll so a mouse-closed dialog is not used.
         */
        win = window_top();
        if (win)
        {
            win->c = input_c;

            if ((*win->func)(win) == 0)
            {
                window_exit_func *ef = win->efunc;

                if (ef)
                    (*ef)(win);

                window_destroy(win);
                update_all(gp);
            }

            continue;
        }

        /* F10 menubar (and keys while the bar/dropdown is active). */
        if (cboard_menubar_key(input_c))
        {
            if (macro_match == -1)
                keycount = 0;
            goto refresh;
        }

        if (!keycount && status.notify)
            update_status_notify(gp, NULL);

#ifdef WITH_LIBPERL
        if (TEST_FLAG(d->flags, CF_PERL))
        {
            CLEAR_FLAG(d->flags, CF_PERL);
            pgn_board_init_fen(gp, d->b, d->perlfen);
            gp->flags = d->perlflags;
            free(d->perlfen);
            pgn_tag_add(&gp->tag, (char *) "FEN", d->oldfen);
            free(d->oldfen);
            d->perlfen = d->oldfen = NULL;
            update_all(gp);
            continue;
        }
#endif

        if (macros && macro_match < 0)
            find_macro(d);

        if ((n = globalkeys()) == 1)
        {
            if (macro_match == -1)
                keycount = 0;

            goto refresh;
        }
        else if (n == -1)
            goto refresh;

        switch (d->mode)
        {
        case MODE_EDIT:
            for (i = 0; edit_keys[i]; i++)
            {
                if (input_c == edit_keys[i]->c)
                {
                    (*edit_keys[i]->f)();
                    break;
                }
            }
            break;
        case MODE_PLAY:
            for (i = 0; play_keys[i]; i++)
            {
                if (input_c == play_keys[i]->c)
                {
                    (*play_keys[i]->f)();
                    goto done;
                }
            }

            do_play_config_command();
            break;
        case MODE_HISTORY:
            for (i = 0; history_keys[i]; i++)
            {
                if (input_c == history_keys[i]->c)
                {
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

    refresh:
        update_all(gp);
    }
}

void usage(const char *pn, int ret)
{
    fprintf((ret) ? stderr : stdout, "%s%s",
#ifdef DEBUG
            _("Usage: cboard [-hvCD] [-u [N]] [-p [-VtRSE] <file>]\n"
              "  -D  Dump libchess debugging info to \"libchess.debug\" (stderr)\n"),
#else
            _("Usage: cboard [-hvC] [-u [N]] [-p [-VtRSE] <file>]\n"),
#endif
            _("  -p  Load PGN file.\n"
              "  -V  Validate a game file.\n"
              "  -S  Validate and output a PGN formatted game.\n"
              "  -R  Like -S but write a reduced PGN formatted game.\n"
              "  -t  Also write custom PGN tags from config file.\n"
              "  -E  Stop processing on file parsing error (overrides config).\n"
              "  -C  Enable strict castling (overrides config).\n"
              "  -u  Enable/disable UTF-8 pieces (1=enable, 0=disable, overrides config).\n"
              "  -v  Version information.\n"
              "  -h  This help text.\n"));

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

    if (config.keys)
    {
        for (i = 0; config.keys[i]; i++)
        {
            free(config.keys[i]->str);
            free(config.keys[i]);
        }

        free(config.keys);
    }

    if (config.einit)
    {
        for (i = 0; config.einit[i]; i++)
            free(config.einit[i]);

        free(config.einit);
    }

    if (config.tag)
        pgn_tag_free(config.tag);

    free(config.datadir);

    if (curses_initialized)
    {
        cboard_menubar_shutdown();
        if (board_table)
        {
            vk_table_destroy(board_table);
            board_table = NULL;
        }
        if (board_vk)
        {
            cboard_ui_widget_destroy(board_vk);
            board_vk = NULL;
            boardw = NULL;
        }
        cboard_ui_window_destroy(history_vk);
        cboard_ui_window_destroy(status_vk);
        cboard_ui_window_destroy(tag_vk);
        board_vk = history_vk = status_vk = tag_vk = NULL;
        boardw = historyw = statusw = tagw = NULL;

        if (engine_vk)
        {
            cboard_ui_widget_destroy(engine_vk);
            engine_vk = NULL;
            enginew = NULL;
        }

        if (loading_vk)
        {
            cboard_ui_widget_destroy(loading_vk);
            loading_vk = NULL;
            loadingw = NULL;
        }

        cboard_ui_shutdown();
    }

#ifdef WITH_LIBPERL
    perl_cleanup();
#endif
}

static void
signal_save_pgn(int sig)
{
    char *buf;
    time_t now;
    char *p = config.savedirectory ? config.savedirectory : config.datadir;

    time(&now);
    if (asprintf(&buf, "%s/signal-%i-%li.pgn", p, sig, now) < 0)
    {
        quit = 1;
        return;
    }

    if (do_game_write(buf, "w", 0, gtotal))
    {
        cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s", p, strerror(errno));
        update_status_notify(gp, "%s", _("Save game failed."));
    }

    free(buf);
    quit = 1;
}

void catch_signal(int which, siginfo_t *info, void *ctx)
{
    (void) info;
    (void) ctx;

    switch (which)
    {
    case SIGALRM:
        update_clocks();
        break;
    case SIGPIPE:
        if (which == SIGPIPE && quit)
            break;

        if (which == SIGPIPE)
            cmessage(NULL, ANY_KEY_STR, "%s", _("Broken pipe. Quitting."));

        cleanup_all();
        exit(EXIT_FAILURE);
        break;
    case SIGSTOP:
        savetty();
        break;
    case SIGCONT:
        resetty();
        /* Same cascade as KEY_RESIZE (re-arm kmio + layout + one refresh). */
        do_window_resize();
        break;
    case SIGINT:
        quit = 1;
        break;
    case SIGTERM:
        signal_save_pgn(which);
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
    else
    {
        fprintf(stderr, _("Loading... %i%% (%i games)%c"), n, gtotal, '\r');
        fflush(stderr);
    }
}

static void
set_defaults()
{
    set_config_defaults();
    set_default_keys();
    filetype = FILE_NONE;
    pgn_config_set(PGN_PROGRESS, 1024);
    pgn_config_set(PGN_PROGRESS_FUNC, loading_progress);
}

int main(int argc, char *argv[])
{
    int opt;
    struct stat st;
    int ret = EXIT_SUCCESS;
    int validate_only = 0, validate_and_write = 0;
    int write_custom_tags = 0;
    int i = 0;
    PGN_FILE *pgn;
    int utf8_pieces = -1;
    struct sigaction sigact;

    setlocale(LC_ALL, "");
    bindtextdomain("cboard", LOCALE_DIR);
    textdomain("cboard");

    /* Solaris 5.9 */
#ifndef HAVE_PROGNAME
    __progname = argv[0];
#endif

    if ((config.pwd = getpwuid(getuid())) == NULL)
        err(EXIT_FAILURE, "getpwuid()");

    if (asprintf(&config.datadir, "%s/.cboard", config.pwd->pw_dir) < 0 || asprintf(&config.ccfile, "%s/cc.data", config.datadir) < 0 || asprintf(&config.nagfile, "%s/nag.data", config.datadir) < 0 || asprintf(&config.configfile, "%s/config", config.datadir) < 0)
        err(EXIT_FAILURE, "asprintf");

    if (stat(config.datadir, &st) == -1)
    {
        if (errno == ENOENT)
        {
            if (mkdir(config.datadir, 0755) == -1)
                err(EXIT_FAILURE, "%s", config.datadir);
        }
        else
            err(EXIT_FAILURE, "%s", config.datadir);

        if (stat(config.datadir, &st) == -1)
            err(EXIT_FAILURE, "%s", config.datadir);
    }

    if (!S_ISDIR(st.st_mode))
        errx(EXIT_FAILURE, "%s: %s", config.datadir, _("Not a directory."));

    set_defaults();

#ifdef DEBUG
    while ((opt = getopt(argc, argv, "DCEVtSRhp:vu::")) != -1)
    {
#else
    while ((opt = getopt(argc, argv, "ECVtSRhp:vu::")) != -1)
    {
#endif
        switch (opt)
        {
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
            printf("%s (%s)\n%s\n%s\n", PACKAGE_STRING, curses_version(),
                   COPYRIGHT, CBOARD_URL);
            exit(EXIT_SUCCESS);
        case 'p':
            filetype = FILE_PGN;
            strncpy(loadfile, optarg, sizeof(loadfile));
            loadfile[sizeof(loadfile) - 1] = 0;
            break;
        case 'u':
            utf8_pieces = optarg ? atoi(optarg) : 1;
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

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_flags = SA_SIGINFO;
    sigact.sa_sigaction = catch_signal;
    sigaction(SIGPIPE, &sigact, NULL);
    sigaction(SIGCONT, &sigact, NULL);
    sigaction(SIGSTOP, &sigact, NULL);
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGALRM, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
    signal(SIGCHLD, SIG_IGN);

    srandom(getpid());

    switch (filetype)
    {
    case FILE_PGN:
        if (pgn_open(loadfile, "r", &pgn) != E_PGN_OK)
            err(EXIT_FAILURE, "%s", loadfile);

        ret = pgn_parse(pgn);
        pgn_close(pgn);
        break;
    case FILE_FEN:
        //ret = parse_fen_file(loadfile);
        break;
    case FILE_EPD: // Not implemented.
    case FILE_NONE:
    default:
        // No file specified. Empty game.
        ret = pgn_parse(NULL);
        gp = game[gindex];
        add_custom_tags(&gp->tag);
        break;
    }

    if (validate_only || validate_and_write)
    {
        if (validate_and_write)
        {
            if (pgn_open("-", "r", &pgn) != E_PGN_OK)
                err(EXIT_FAILURE, "pgn_open()");

            for (i = 0; i < gtotal; i++)
            {
                if (write_custom_tags)
                    add_custom_tags(&game[i]->tag);

                pgn_write(pgn, game[i]);
            }

            pgn_close(pgn);

            fm_loaded_file = TRUE;
        }

        cleanup_all();
        exit(ret);
    }
    else if (ret == E_PGN_ERR)
        exit(ret);

    if (utf8_pieces != -1)
        config.utf8_pieces = utf8_pieces;

    init_wchar_pieces();
    yes_wchar = str_to_wchar(_("y"));
    all_wchar = str_to_wchar(_("a"));
    overwrite_wchar = str_to_wchar(_("o"));
    resume_wchar = str_to_wchar(_("r"));
    current_wchar = str_to_wchar(_("c"));
    append_wchar = str_to_wchar(_("a"));
    translatable_tag_names[0] = _("Event");
    translatable_tag_names[1] = _("Site");
    translatable_tag_names[2] = _("Date");
    translatable_tag_names[3] = _("Round");
    translatable_tag_names[4] = _("White");
    translatable_tag_names[5] = _("Black");
    translatable_tag_names[6] = _("Result");
    init_userdata();

    /*
   * This fixes window resizing in an xterm.
   */
    if (getenv("DISPLAY") != NULL)
    {
        putenv((char *) "LINES=");
        putenv((char *) "COLUMNS=");
    }

    cboard_ui_init();
    curses_initialized = 1;

    if (LINES < 24 || COLS < 74)
    {
        cboard_ui_shutdown();
        curses_initialized = 0;
        errx(EXIT_FAILURE, _("Need at least an 74x24 terminal."));
    }

    /* Color pairs come from vdk_color_init() in cboard_ui_init(). */

    cboard_menubar_init();

    {
        int style = config.linegraphics ? VK_BORDER_SINGLE : VK_BORDER_NONE;
        int bx = config.boardleft ? 0 : COLS - BOARD_WIDTH;
        int tw = BOARD_WIDTH - BOARD_RANK_GUTTER;
        int th = BOARD_HEIGHT - BOARD_FILE_GUTTER;

        if (tw < 10)
            tw = 10;
        if (th < 10)
            th = 10;

        /* Host holds gutters; table is inset and drawn onto the host. */
        board_vk = cboard_ui_widget_new(BOARD_HEIGHT, BOARD_WIDTH, UI_TOP, bx);
        if (!board_vk)
            errx(EXIT_FAILURE, "%s", "Could not create board host.");
        board_table = vk_table_create(tw, th, 8, 8, style);
        if (!board_table)
            errx(EXIT_FAILURE, "%s", "Could not create board grid.");
        vk_table_set_border_colors(board_table,
                                   config.color[CONF_BGRAPHICS].fg,
                                   config.color[CONF_BGRAPHICS].bg);
        vk_widget_set_colors(VK_WIDGET(board_table),
                             config.color[CONF_BDWINDOW].fg,
                             config.color[CONF_BDWINDOW].bg);
        board_layout_table();
        boardw = cboard_ui_widget_canvas(board_vk);
    }
    history_vk =
        cboard_ui_frame_new(HISTORY_HEIGHT, HISTORY_WIDTH,
                            LINES - HISTORY_HEIGHT, COLS - HISTORY_WIDTH,
                            _("Move History"),
                            config.color[CONF_HWINDOW].fg,
                            config.color[CONF_HWINDOW].bg,
                            config.color[CONF_HBORDER].fg,
                            config.color[CONF_HBORDER].bg);
    historyw = cboard_ui_frame_canvas(history_vk);
    status_vk =
        cboard_ui_frame_new(STATUS_HEIGHT, STATUS_WIDTH, UI_TOP, 0,
                            _("Game Status"),
                            config.color[CONF_SWINDOW].fg,
                            config.color[CONF_SWINDOW].bg,
                            config.color[CONF_SBORDER].fg,
                            config.color[CONF_SBORDER].bg);
    statusw = cboard_ui_frame_canvas(status_vk);
    tag_vk =
        cboard_ui_frame_new(TAG_HEIGHT, TAG_WIDTH,
                            UI_TOP + STATUS_HEIGHT + 1, 0,
                            _("Roster Tags"),
                            config.color[CONF_TWINDOW].fg,
                            config.color[CONF_TWINDOW].bg,
                            config.color[CONF_TBORDER].fg,
                            config.color[CONF_TBORDER].bg);
    tagw = cboard_ui_frame_canvas(tag_vk);
    keypad(boardw, TRUE);
    if (tagw)
        leaveok(tagw, TRUE);
    if (statusw)
        leaveok(statusw, TRUE);
    if (historyw)
        leaveok(historyw, TRUE);
    draw_window_decor();
    cboard_menubar_refresh();
    game_loop();
    cleanup_all();
    free(w_pawn_wchar);
    free(w_rook_wchar);
    free(w_bishop_wchar);
    free(w_knight_wchar);
    free(w_queen_wchar);
    free(w_king_wchar);
    free(b_pawn_wchar);
    free(b_rook_wchar);
    free(b_bishop_wchar);
    free(b_knight_wchar);
    free(b_queen_wchar);
    free(b_king_wchar);
    free(empty_wchar);
    free(enpassant_wchar);
    free(yes_wchar);
    free(all_wchar);
    free(overwrite_wchar);
    free(resume_wchar);
    free(current_wchar);
    free(append_wchar);
    free(status.notify);
    exit(EXIT_SUCCESS);
}
