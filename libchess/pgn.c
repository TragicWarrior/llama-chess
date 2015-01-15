/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2013 Ben Kibbey <bjk@luxsci.net>

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
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <err.h>
#include <time.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "chess.h"
#include "common.h"

#ifdef DEBUG
#include "debug.h"
#endif

static BOARD pgn_board;
static int nulltags;
static int tag_section;
static int pgn_ret;
static int pgn_write_turn;
static int pgn_mpl;
static int pgn_lastc;
static int ravlevel;
static long pgn_fsize;
static int pgn_rav;
static RAV *pgn_rav_p;

#ifdef __linux__
extern char *strptime(const char *, const char *, struct tm *);
#endif

static int Fgetc(FILE *fp)
{
    int c;

    if ((c = fgetc(fp)) != EOF) {
	if (pgn_config.progress && pgn_config.pfunc) {
	    if (!(ftell(fp) % pgn_config.progress))
		pgn_config.pfunc(pgn_fsize, ftell(fp));
	}
    }

    return c;
}

static int Ungetc(int c, FILE *fp)
{
    return ungetc(c, fp);
}

char *pgn_version()
{
    return "libchess " PACKAGE_VERSION;
}

static char *trim(char *str)
{
    int i = 0;

    if (!str)
	return NULL;

    while (isspace(*str))
	str++;

    for (i = strlen(str) - 1; isspace(str[i]); i--)
	str[i] = 0;

    return str;
}

static char *itoa(long n, char *buf)
{
    sprintf(buf, "%li", n);
    return buf;
}

/*
 * Clears the valid move flag for all positions on board 'b'. Returns nothing.
 */
void pgn_reset_valid_moves(BOARD b)
{
    int row, col;

#ifdef DEBUG
    PGN_DUMP("%s:%d: resetting valid moves\n", __FILE__, __LINE__);
#endif

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++)
	    b[row][col].valid = 0;
    }
}

/*
 * Toggles g->turn. Returns nothing.
 */
void pgn_switch_turn(GAME g)
{
    g->turn = (g->turn == WHITE) ? BLACK : WHITE;
}

/*
 * Toggles g->side and switches the White and Black roster tags. Returns
 * nothing.
 */
void pgn_switch_side(GAME g, int t)
{
    if (t) {
      char *w = g->tag[4]->value;

      g->tag[4]->value = g->tag[5]->value;
      g->tag[5]->value = w;
    }

    g->side = !g->side;
}

/*
 * Creates a FEN tag from the current game 'g', history move (g->hindex) and
 * board 'b'. Returns a FEN tag.
 */
char *pgn_game_to_fen(GAME g, BOARD b)
{
    char tmp[16];
    int row, col;
    int i;
    char buf[MAX_PGN_LINE_LEN] = {0}, *p;
    int oldturn = g->turn;
    char enpassant[3] = {0}, *e;
    int castle = 0;

#ifdef DEBUG
    PGN_DUMP("%s:%d: creating FEN tag\n", __FILE__, __LINE__);
#endif

    for (i = pgn_history_total(g->hp); i >= g->hindex - 1; i--)
	pgn_switch_turn(g);

    p = buf;

    for (row = 0; row < 8; row++) {
	int count = 0;

	for (col = 0; col < 8; col++) {
	    if (b[row][col].enpassant) {
		b[row][col].icon = pgn_int_to_piece(WHITE, OPEN_SQUARE);
		e = enpassant;
		*e++ = 'a' + col;
		*e++ = ('0' + 8) - row;
		*e = 0;
	    }

	    if (pgn_piece_to_int(b[row][col].icon) == OPEN_SQUARE) {
		count++;
		continue;
	    }

	    if (count) {
		*p++ = '0' + count;
		count = 0;
	    }

	    *p++ = b[row][col].icon;
	    *p = 0;
	}

	if (count) {
	    *p++ = '0' + count;
	    count = 0;
	}

	*p++ = '/';
    }

    --p;
    *p++ = ' ';
    *p++ = (g->turn == WHITE) ? 'w' : 'b';
    *p++ = ' ';

    if (TEST_FLAG(g->flags, GF_WK_CASTLE) && pgn_piece_to_int(b[7][7].icon) ==
	    ROOK && isupper(b[7][7].icon) && pgn_piece_to_int(b[7][4].icon) ==
		KING && isupper(b[7][4].icon)) {
	*p++ = 'K';
	castle = 1;
    }

    if (TEST_FLAG(g->flags, GF_WQ_CASTLE) && pgn_piece_to_int(b[7][0].icon) ==
	    ROOK && isupper(b[7][0].icon) && pgn_piece_to_int(b[7][4].icon) ==
		KING && isupper(b[7][4].icon)) {
	*p++ = 'Q';
	castle = 1;
    }

    if (TEST_FLAG(g->flags, GF_BK_CASTLE) && pgn_piece_to_int(b[0][7].icon) ==
	    ROOK && islower(b[0][7].icon) && pgn_piece_to_int(b[0][4].icon) ==
		KING && islower(b[0][4].icon)) {
	*p++ = 'k';
	castle = 1;
    }

    if (TEST_FLAG(g->flags, GF_BQ_CASTLE) && pgn_piece_to_int(b[0][0].icon) ==
	    ROOK && islower(b[0][0].icon) && pgn_piece_to_int(b[0][4].icon) ==
		KING && islower(b[0][4].icon)) {
	*p++ = 'q';
	castle = 1;
    }

    if (!castle)
	*p++ = '-';

    *p++ = ' ';

    if (enpassant[0]) {
	e = enpassant;
	*p++ = *e++;
	*p++ = *e++;
    }
    else
	*p++ = '-';

    *p++ = ' ';

    // Halfmove clock.
    *p = 0;
    strcat(p, itoa(g->ply, tmp));
    p = buf + strlen(buf);
    *p++ = ' ';

    // Fullmove number.
    i = (g->hindex + 1) / 2;
    *p = 0;
    strcat(p, itoa((g->hindex / 2) + (g->hindex % 2), tmp));

    g->turn = oldturn;
    return buf[0] ? strdup (buf) : NULL;
}

/*
 * Returns the total number of moves in 'h' or 0 if there are none.
 */
int pgn_history_total(HISTORY **h)
{
    int i;

    if (!h)
	return 0;

    for (i = 0; h[i]; i++);
    return i;
}

/*
 * Deallocates all of the history data from position 'start' in the array 'h'.
 * Returns nothing.
 */
void pgn_history_free(HISTORY **h, int start)
{
    int i;

#ifdef DEBUG
    PGN_DUMP("%s:%d: freeing history\n", __FILE__, __LINE__);
#endif

    if (!h || start > pgn_history_total(h))
	return;

    if (start < 0)
	start = 0;

    for (i = start; h[i]; i++) {
	if (h[i]->rav) {
	    pgn_history_free(h[i]->rav, 0);
	    free(h[i]->rav);
	}

	free(h[i]->comment);
	free(h[i]->move);
	free(h[i]->fen);
	free(h[i]);
    }

    h[start] = NULL;
}

/*
 * Returns the history ply 'n' from 'h'. If 'n' is out of range then NULL is
 * returned.
 */
HISTORY *pgn_history_by_n(HISTORY **h, int n)
{
    if (n < 0 || n > pgn_history_total(h) - 1)
	return NULL;

    return h[n];
}

/*
 * Appends move 'm' to game 'g' history pointer and creates a FEN tag for the
 * current game state using board 'b'. The FEN tag makes things faster than
 * validating the entire move history by validating only the current move to
 * the previous moves FEN tag. The history pointer may be a in a RAV so
 * g->rav.hp is also updated to the new (realloc()'ed) pointer. If not in a
 * RAV then g->history will be updated. Returns E_PGN_ERR if realloc() failed
 * or E_PGN_OK on success.
 */
pgn_error_t pgn_history_add(GAME g, BOARD b, const char *m)
{
    int t = pgn_history_total(g->hp);
    int o;
    HISTORY **h = NULL, *tmp;
    int ri = (g->ravlevel) ? g->rav[g->ravlevel - 1].hindex : 0;

#ifdef DEBUG
    PGN_DUMP("%s:%d: adding '%s' to move history\n", __FILE__, __LINE__, m);
#endif

    if (g->ravlevel)
	o = g->rav[g->ravlevel - 1].hp[ri-1]->rav - g->hp;
    else
	o = g->history - g->hp;

    if ((h = realloc(g->hp, (t + 2) * sizeof(HISTORY *))) == NULL)
	return E_PGN_ERR;

    g->hp = h;

    if (g->ravlevel)
	g->rav[g->ravlevel - 1].hp[ri-1]->rav = g->hp + o;
    else
	g->history = g->hp + o;

    if ((g->hp[t] = calloc(1, sizeof(HISTORY))) == NULL)
	return E_PGN_ERR;

    if ((g->hp[t]->move = strdup(m)) == NULL) {
	free(g->hp[t]);
	g->hp[t] = NULL;
	return E_PGN_ERR;
    }

    tmp = g->hp[t];
    t++;
    g->hp[t] = NULL;
    g->hindex = pgn_history_total(g->hp);
    pgn_switch_turn(g);
    tmp->fen = pgn_game_to_fen(g, b);
    pgn_switch_turn(g);
    return E_PGN_OK;
}

/*
 * Resets the game 'g' using board 'b' up to history move (g->hindex) 'n'.
 * Returns E_PGN_OK on success or E_PGN_PARSE if there was a FEN tag but the
 * parsing of it failed. Or returns E_PGN_INVALID if somehow the move failed
 * validation while resetting.
 */
pgn_error_t pgn_board_update(GAME g, BOARD b, int n)
{
    BOARD tb;
    int ret = E_PGN_OK;
    int p_error = TEST_FLAG(g->flags, GF_PERROR);
    int black_opening = TEST_FLAG(g->flags, GF_BLACK_OPENING);

#ifdef DEBUG
    PGN_DUMP("%s:%d: updating board\n", __FILE__, __LINE__);
#endif

    if (!g->ravlevel && TEST_FLAG(g->flags, GF_BLACK_OPENING))
	g->turn = BLACK;
    else
	g->turn = WHITE;

    g->flags = 0;
    SET_FLAG(g->flags, GF_WK_CASTLE|GF_WQ_CASTLE|GF_WQ_CASTLE|
	    GF_BK_CASTLE|GF_BQ_CASTLE);
    pgn_board_init(tb);

    if (g->ravlevel)
	pgn_board_init_fen(g, tb, g->rav[g->ravlevel - 1].fen);
    else if (pgn_tag_find(g->tag, "FEN") != -1 &&
	    pgn_board_init_fen(g, tb, NULL))
	return E_PGN_PARSE;

    if (n) {
	HISTORY *h = pgn_history_by_n(g->hp, n-1);

	if (h) {
	    ret = pgn_board_init_fen(g, tb, h->fen);
	    if (ret == E_PGN_OK) {
		h = pgn_history_by_n(g->hp, n);
		if (h) {
		    char *frfr = NULL;

		    ret = pgn_parse_move(g, tb, &h->move, &frfr);
		    if (ret == E_PGN_OK) {
			h = pgn_history_by_n(g->hp, n-1);
			ret = pgn_board_init_fen(g, tb, h->fen);
		    }

		    free(frfr);
		}
	    }
	}
    }

    if (ret == E_PGN_OK)
	memcpy(b, tb, sizeof(BOARD));

    if (p_error)
	SET_FLAG(g->flags, GF_PERROR);

    if (black_opening)
	SET_FLAG(g->flags, GF_BLACK_OPENING);

    return ret;
}

/*
 * Updates the game 'g' using board 'b' to the next 'n'th history move.
 * Returns nothing.
 */
void pgn_history_prev(GAME g, BOARD b, int n)
{
    if (g->hindex - n < 0) {
	if (n <= 2)
	    g->hindex = pgn_history_total(g->hp);
	else
	    g->hindex = 0;
    }
    else
	g->hindex -= n;

    pgn_board_update(g, b, g->hindex);
}

/*
 * Updates the game 'g' using board 'b' to the previous 'n'th history move.
 * Returns nothing.
 */
void pgn_history_next(GAME g, BOARD b, int n)
{
    if (g->hindex + n > pgn_history_total(g->hp)) {
	if (n <= 2)
	    g->hindex = 0;
	else
	    g->hindex = pgn_history_total(g->hp);
    }
    else
	g->hindex += n;

    pgn_board_update(g, b, g->hindex);
}

/*
 * Converts the character piece 'p' to an integer. Returns the integer
 * associated with 'p' or E_PGN_ERR if 'p' is invalid.
 */
int pgn_piece_to_int(int p)
{
    if (p == '.')
	return OPEN_SQUARE;

    p = tolower(p);

    switch (p) {
	case 'p':
	    return PAWN;
	case 'r':
	    return ROOK;
	case 'n':
	    return KNIGHT;
	case 'b':
	    return BISHOP;
	case 'q':
	    return QUEEN;
	case 'k':
	    return KING;
	default:
	    break;
    }

#ifdef DEBUG
    PGN_DUMP("%s:%d: invalid piece '%c'\n", __FILE__, __LINE__, p);
#endif
    return E_PGN_ERR;
}

/*
 * Converts the integer piece 'n' to a character whose turn is 'turn'. WHITE
 * piece are uppercase and BLACK pieces are lowercase. Returns the character
 * associated with 'n' or E_PGN_ERR if 'n' is an invalid piece.
 */
pgn_error_t pgn_int_to_piece(char turn, int n)
{
    int p = 0;

    switch (n) {
	case PAWN:
	    p = 'p';
	    break;
	case ROOK:
	    p = 'r';
	    break;
	case KNIGHT:
	    p = 'n';
	    break;
	case BISHOP:
	    p = 'b';
	    break;
	case QUEEN:
	    p = 'q';
	    break;
	case KING:
	    p = 'k';
	    break;
	case OPEN_SQUARE:
	    p = '.';
	    break;
	default:
#ifdef DEBUG
	    PGN_DUMP("%s:%d: unknown piece integer %i\n", __FILE__,
		    __LINE__, n);
#endif
	    return E_PGN_ERR;
	    break;
    }

    return (turn == WHITE) ? toupper(p) : p;
}

/*
 * Finds a tag 'name' in the structure array 't'. Returns the location in the
 * array of the found tag or E_PGN_ERR if the tag could not be found.
 */
pgn_error_t pgn_tag_find(TAG **t, const char *name)
{
    int i;

    for (i = 0; t[i]; i++) {
	if (strcasecmp(t[i]->name, name) == 0)
	    return i;
    }

    return E_PGN_ERR;
}

static pgn_error_t remove_tag(TAG ***array, const char *tag)
{
    TAG **tags = *array;
    int n = pgn_tag_find(tags, tag);
    int i, t;

    if (n == E_PGN_ERR)
	return E_PGN_ERR;

    for (i = t = 0; tags[i]; i++) {
	if (i == n) {
	    free(tags[i]->name);
	    free(tags[i]->value);
	    free(tags[i]);
	    continue;
	}

	tags[t++] = tags[i];
    }

    tags = realloc(*array, (t + 1) * sizeof(TAG *));
    tags[t] = NULL;
    *array = tags;
#ifdef DEBUG
    PGN_DUMP("%s:%d: removed tag: name='%s'\n", __FILE__, __LINE__, tag);
#endif
    return E_PGN_OK;
}

static int tag_compare(const void *a, const void *b)
{
    TAG * const *ta = a;
    TAG * const *tb = b;

    return strcmp((*ta)->name, (*tb)->name);
}

/*
 * Sorts a tag array. The first seven tags are in order of the PGN standard so
 * don't sort'em. Returns nothing.
 */
void pgn_tag_sort(TAG **tags)
{
    if (pgn_tag_total(tags) <= 7)
	return;

    qsort(tags + 7, pgn_tag_total(tags) - 7, sizeof(TAG *), tag_compare);
}

/*
 * Returns the total number of tags in 't' or 0 if 't' is NULL.
 */
int pgn_tag_total(TAG **tags)
{
    int i = 0;

    if (!tags)
	return 0;

    while (tags[i])
	i++;

    return i;
}

/*
 * Adds a tag 'name' with value 'value' to the pointer to array of TAG
 * pointers 'dst'. If a duplicate tag 'name' was found then the existing tag
 * is updated to the new 'value'. If 'value' is NULL, the tag is removed.
 * Returns E_PGN_ERR if there was a memory allocation error or E_PGN_OK on
 * success.
 */
pgn_error_t pgn_tag_add(TAG ***dst, char *name, char *value)
{
    int i;
    TAG **tdata = *dst;
    TAG **a = NULL;
    int t = pgn_tag_total(tdata);

#ifdef DEBUG
    PGN_DUMP("%s:%d: adding tag\n", __FILE__, __LINE__);
#endif

    if (!name)
	return E_PGN_ERR;

    name = trim(name);

    if (value)
	value = trim(value);

    // Find an existing tag with 'name'.
    for (i = 0; i < t; i++) {
	char *tmp = NULL;

	if (strcasecmp(tdata[i]->name, name) == 0) {
	    if (value) {
		if ((tmp = strdup(value)) == NULL)
		    return E_PGN_ERR;
	    }
	    else {
		remove_tag(dst, name);
		return E_PGN_OK;
	    }

	    free(tdata[i]->value);
	    tdata[i]->value = tmp;
	    *dst = tdata;
	    return E_PGN_OK;
	}
    }

    if ((a = realloc(tdata, (t + 2) * sizeof(TAG *))) == NULL)
	return E_PGN_ERR;

    tdata = a;

    if ((tdata[t] = malloc(sizeof(TAG))) == NULL)
	return E_PGN_ERR;

    if ((tdata[t]->name = strdup(name)) == NULL) {
	tdata[t] = NULL;
	return E_PGN_ERR;
    }

    if (value) {
	if ((tdata[t]->value = strdup(value)) == NULL) {
	    free(tdata[t]->name);
	    tdata[t] = NULL;
	    return E_PGN_ERR;
	}
    }
    else
	tdata[t]->value = NULL;

    tdata[t]->name[0] = toupper(tdata[t]->name[0]);
    tdata[++t] = NULL;
    *dst = tdata;

#ifdef DEBUG
    PGN_DUMP("%s:%d: added tag: name='%s' value='%s'\n", __FILE__, __LINE__,
	    name, (value) ? value : "null");
#endif

    return E_PGN_OK;
}

static char *remove_tag_escapes(const char *str)
{
    int i, n;
    int len = strlen(str);
    char buf[MAX_PGN_LINE_LEN] = {0};

    for (i = n = 0; i < len; i++, n++) {
	switch (str[i]) {
	    case '\\':
		i++;
	    default:
		break;
	}

	buf[n] = str[i];
    }

    buf[n] = '\0';
    return buf[0] ? strdup (buf) : NULL;
}

/*
 * Resets or initializes a new game board 'b'. Returns nothing.
 */
void pgn_board_init(BOARD b)
{
    int row, col;

#ifdef DEBUG
    PGN_DUMP("%s:%d: initializing board\n", __FILE__, __LINE__);
#endif

    memset(b, 0, sizeof(BOARD));

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++) {
	    int c = '.';

	    switch (row) {
		case 0:
		case 7:
		    switch (col) {
			case 0:
			case 7:
			    c = 'r';
			    break;
			case 1:
			case 6:
			    c = 'n';
			    break;
			case 2:
			case 5:
			    c = 'b';
			    break;
			case 3:
			    c = 'q';
			    break;
			case 4:
			    c = 'k';
			    break;
		    }
		    break;
		case 1:
		case 6:
		    c = 'p';
		    break;
	    }

	    b[row][col].icon = (row < 2) ? c : toupper(c);
	}
    }
}

/*
 * Adds the standard PGN roster tags to game 'g'.
 */
static void set_default_tags(GAME g)
{
    time_t now;
    char tbuf[11] = {0};
    struct tm *tp;
    struct passwd *pw = getpwuid(getuid());
    char host[64] = { 0 };

    time(&now);
    tp = localtime(&now);
    strftime(tbuf, sizeof(tbuf), PGN_TIME_FORMAT, tp);

    /* The standard seven tag roster (in order of appearance). */
    if (pgn_tag_add(&g->tag, "Event", "?") != E_PGN_OK)
	warn("pgn_tag_add()");

    gethostname (host, sizeof(host)-1);
    if (pgn_tag_add(&g->tag, "Site", host) != E_PGN_OK)
	warn("pgn_tag_add()");

    if (pgn_tag_add(&g->tag, "Date", tbuf) != E_PGN_OK)
	warn("pgn_tag_add()");

    if (pgn_tag_add(&g->tag, "Round", "-") != E_PGN_OK)
	warn("pgn_tag_add()");

    if (pgn_tag_add(&g->tag, "White", pw->pw_gecos) != E_PGN_OK)
	warn("pgn_tag_add()");

    if (pgn_tag_add(&g->tag, "Black", "?") != E_PGN_OK)
	warn("pgn_tag_add()");

    if (pgn_tag_add(&g->tag, "Result", "*") != E_PGN_OK)
	warn("pgn_tag_add()");
}

/*
 * Frees a TAG array. Returns nothing.
 */
void pgn_tag_free(TAG **tags)
{
    int i;
    int t = pgn_tag_total(tags);

#ifdef DEBUG
    PGN_DUMP("%s:%d: freeing tags\n", __FILE__, __LINE__);
#endif

    if (!tags)
	return;

    for (i = 0; i < t; i++) {
	free(tags[i]->name);
	free(tags[i]->value);
	free(tags[i]);
    }

    free(tags);
}

/*
 * Frees a single game 'g'. Returns nothing.
 */
void pgn_free(GAME g)
{
#ifdef DEBUG
    PGN_DUMP("%s:%d: freeing game\n", __FILE__, __LINE__);
#endif
    pgn_history_free(g->history, 0);
    free(g->history);
    pgn_tag_free(g->tag);

    if (g->rav) {
	for (g->ravlevel--; g->ravlevel >= 0; g->ravlevel--)
	    free(g->rav[g->ravlevel].fen);

	free(g->rav);
    }

    free(g);
}

/*
 * Frees all games in the global 'game' array. Returns nothing.
 */
void pgn_free_all()
{
    int i;

#ifdef DEBUG
    PGN_DUMP("%s:%d: freeing game data\n", __FILE__, __LINE__);
#endif

    for (i = 0; i < gtotal; i++) {
	pgn_free(game[i]);
    }

    if (game)
	free(game);

    game = NULL;
}

static void reset_game_data()
{
#ifdef DEBUG
    PGN_DUMP("%s:%d: resetting game data\n", __FILE__, __LINE__);
#endif
    pgn_free_all();
    gtotal = gindex = 0;
}

static void skip_leading_space(FILE *fp)
{
    int c;

    while ((c = Fgetc(fp)) != EOF && !feof(fp)) {
	if (!isspace(c))
	    break;
    }

    Ungetc(c, fp);
}

/*
 * PGN move text section.
 */
static int move_text(GAME g, FILE *fp)
{
    char m[MAX_SAN_MOVE_LEN + 1] = {0}, *p;
    int c;
    int count;
    char *frfr = NULL;

    g->oflags = g->flags;

    while((c = Fgetc(fp)) != EOF) {
	if (isdigit(c) || isspace(c) || c == '.')
	    continue;

	break;
    }

    Ungetc(c, fp);

    if (fscanf(fp, " %[a-hPpRrNnBbQqKk1-9#+=Ox-]%n", m, &count) != 1)
	return 1;

    m[MAX_SAN_MOVE_LEN] = 0;
    p = m;

    if (pgn_parse_move(g, pgn_board, &p, &frfr)) {
	pgn_switch_turn(g);
	return 1;
    }

    free(frfr);
#ifdef DEBUG
    PGN_DUMP("%s\n%s", p, debug_board(pgn_board));
#endif

    pgn_history_add(g, pgn_board, p);
    pgn_switch_turn(g);
    return 0;
}

/*
 * PGN nag text.
 */
static void nag_text(GAME g, FILE *fp)
{
    int c, i, t;
    char nags[5], *n = nags;
    int nag = 0;

    while ((c = Fgetc(fp)) != EOF && !isspace(c)) {
	if (c == '$') {
	    while ((c = Fgetc(fp)) != EOF && isdigit(c))
		*n++ = c;

	    Ungetc(c, fp);
	    break;
	}

	if (c == '!') {
	    if ((c = Fgetc(fp)) == '!')
		nag = 3;
	    else if (c == '?')
		nag = 5;
	    else {
		Ungetc(c, fp);
		nag = 1;
	    }

	    break;
	}
	else if (c == '?') {
	    if ((c = Fgetc(fp)) == '?')
		nag = 4;
	    else if (c == '!')
		nag = 6;
	    else {
		Ungetc(c, fp);
		nag = 2;
	    }

	    break;
	}
	else if (c == '~')
	    nag = 13;
	else if (c == '=') {
	    if ((c = Fgetc(fp)) == '+')
		nag = 15;
	    else {
		Ungetc(c, fp);
		nag = 10;
	    }

	    break;
	}
	else if (c == '+') {
	    if ((t = Fgetc(fp)) == '=')
		nag = 14;
	    else if (t == '-')
		nag = 18;
	    else if (t == '/') {
		if ((i = Fgetc(fp)) == '-')
		    nag = 16;
		else
		    Ungetc(i, fp);

		break;
	    }
	    else
		Ungetc(t, fp);

	    break;
	}
	else if (c == '-') {
	    if ((t = Fgetc(fp)) == '+')
		nag = 18;
	    else if (t == '/') {
		if ((i = Fgetc(fp)) == '+')
		    nag = 17;
		else
		    Ungetc(i, fp);

		break;
	    }
	    else
		Ungetc(t, fp);

	    break;
	}
    }

    *n = '\0';

    if (!nag)
	nag = (nags[0]) ? atoi(nags) : 0;

    if (!nag || nag < 0 || nag > 255)
	return;

    // FIXME -1 is because move_text() increments g->hindex. The NAG
    // annoatation isnt guaranteed to be after the move text in import format.
    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (g->hp[g->hindex - 1]->nag[i])
	    continue;

	g->hp[g->hindex - 1]->nag[i] = nag;
	break;
    }

    skip_leading_space(fp);
}

/*
 * PGN move annotation.
 */
static int annotation_text(GAME g, FILE *fp, int terminator)
{
    int c, lastchar = 0;
    int len = 0;
    int hindex = pgn_history_total(g->hp) - 1;
    char buf[MAX_PGN_LINE_LEN], *a = buf;

    skip_leading_space(fp);

    while ((c = Fgetc(fp)) != EOF && c != terminator) {
	if (c == '\n')
	    c = ' ';

	if (isspace(c) && isspace(lastchar))
	    continue;

	if (len + 1 == sizeof(buf))
	    continue;

	*a++ = lastchar = c;
	len++;
    }

    *a = '\0';

    if (hindex < 0)
	hindex = 0;

    /*
     * This annotation is before any move text or NAg-> Allocate a new move.
     */
    if (!g->hp[hindex]) {
	if ((g->hp[hindex] = calloc(1, sizeof(HISTORY))) == NULL)
	    return E_PGN_ERR;
    }

    if ((g->hp[hindex]->comment = strdup(buf)) == NULL)
	return E_PGN_ERR;

    return E_PGN_OK;
}

/*
 * PGN roster tag->
 */
static int tag_text(GAME g, FILE *fp)
{
    char name[LINE_MAX] = {0}, *n = name;
    char value[LINE_MAX] = {0}, *v = value;
    int c, i = 0;
    int lastchar = 0;
    char *tmp;

    skip_leading_space(fp);

    /* The tag name is up until the first whitespace. */
    while ((c = Fgetc(fp)) != EOF && !isspace(c))
	*n++ = c;

    *n = '\0';
    *name = toupper(*name);
    skip_leading_space(fp);

    /* The value is until the first closing bracket. */
    while ((c = Fgetc(fp)) != EOF && c != ']') {
	if (i++ == '\0' && c == '\"')
	    continue;

	if (c == '\n' || c == '\t')
	    c = ' ';

	if (c == ' ' && lastchar == ' ')
	    continue;

	lastchar = *v++ = c;
    }

    *v = '\0';

    while (isspace(*--v))
	*v = '\0';

    if (*v == '\"')
	*v = '\0';

    if (value[0] == '\0') {
	if (strcmp(name, "Result") == 0)
	    value[0] = '*';
	else
	    value[0] = '?';

	value[1] = '\0';
    }

    tmp = remove_tag_escapes (value);
    strncpy(value, tmp, sizeof(value)-1);
    free (tmp);

    /*
     * See eog_text() for an explanation.
     */
    if (strcmp(name, "Result") == 0) {
	if (strcmp(value, "1/2-1/2") == 0) {
	    if (pgn_tag_add(&g->tag, name, value) != E_PGN_OK)
		warn("pgn_tag_add()");
	}
    }
    else {
	if (pgn_tag_add(&g->tag, name, value) != E_PGN_OK)
	    warn("pgn_tag_add()");
    }

    return 0;
}

/*
 * PGN end-of-game marker.
 */
static int eog_text(GAME g, FILE *fp)
{
    int c, i = 0;
    char buf[8], *p = buf;

    while ((c = Fgetc(fp)) != EOF && !isspace(c) && i++ < sizeof(buf))
	*p++ = c;

    if (isspace(c))
	Ungetc(c, fp);

    *p = 0;

    if (strcmp(buf, "1-0") != 0 && strcmp(buf, "0-1") != 0 &&
	    strcmp(buf, "1/2-1/2") != 0 && strcmp(buf, "*") != 0)
	return 1;

    /*
     * The eog marker in the move text may not match the actual game result.
     * We'll leave it up to the move validator to determine the result unless
     * the move validator cannot determine who won and the move text says it's
     * a draw.
     */
    if (g->tag[6]->value[0] == '*' && strcmp("1/2-1/2", buf) == 0) {
	if (pgn_tag_add(&g->tag, "Result", buf) != E_PGN_OK)
	    warn("pgn_tag_add()");
    }

    return 0;
}

/*
 * Parse RAV text and keep track of g->hp. The 'o' argument is the board state
 * before the current move (.hindex) was parsed.
 */
static int read_file(FILE *);
static int rav_text(GAME g, FILE *fp, int which, BOARD o)
{
    struct game_s tg;
    RAV *r;

    // Begin RAV for the current move.
    if (which == '(') {
	if (!g->hindex)
	    return 1;

	pgn_rav++; // For detecting parse errors.

	/*
	 * Save the current game state for this RAV depth/level.
	 */
	if ((r = realloc(g->rav, (g->ravlevel + 1) * sizeof(RAV))) == NULL) {
	    warn("realloc()");
	    return 1;
	}

	g->rav = pgn_rav_p = r;

	if ((g->rav[g->ravlevel].fen = pgn_game_to_fen(g, pgn_board))
		== NULL) {
	    warn("strdup()");
	    return 1;
	}

	g->rav[g->ravlevel].hp = g->hp;
	g->rav[g->ravlevel].hindex = g->hindex;
	memcpy(&tg, &(*g), sizeof(struct game_s));
	g->flags = g->oflags;
	memcpy(pgn_board, o, sizeof(BOARD));

	if ((g->hp[g->hindex - 1]->rav = calloc(1, sizeof(HISTORY *))) == NULL) {
	    warn("calloc()");
	    return 1;
	}

	/*
	 * pgn_history_add() will now append to the new history pointer that
	 * is g->hp[previous_move]->rav.
	 */
	g->hp = g->hp[g->hindex - 1]->rav;

	/*
	 * Reset. Will be restored later from 'tg' which is a local variable
	 * so recursion is possible.
	 */
	g->hindex = 0;
	g->ravlevel++;

	/*
	 * Undo move_text()'s switch.
	 */
	pgn_switch_turn(g);

	/*
	 * Now continue as normal as if there were no RAV. Moves will be
	 * parsed and appended to the new history pointer.
	 */
	if (read_file(fp))
	    return 1;

	/*
	 * read_file() has returned. This means that a RAV has ended by this
	 * function returning -1 (see below). So we restore the game state
	 * (tg) that was saved before calling read_file().
	 */
	pgn_board_init_fen(&tg, pgn_board, g->rav[tg.ravlevel].fen);
	free(g->rav[tg.ravlevel].fen);
	memcpy(&(*g), &tg, sizeof(struct game_s));
	g->rav = pgn_rav_p;
    }
    /*
     * The end of a RAV. This makes the read_file() that called this function
     * return (see above and the switch statement in read_file()).
     */
    else if (which == ')') {
	pgn_rav--; // For detecting parse errors.
	return (pgn_rav < 0) ? 1 : -1;
    }

    return 0;
}

/*
 * FIXME
 * See pgn_board_init_fen(). Returns E_PGN_PARSE on parse error. 0 may be
 * returned on success when there is no move count in the FEN tag otherwise
 * the move count is returned.
 */
static int parse_fen_line(BOARD b, unsigned *flags, char *turn, char *ply,
	char *str)
{
    char *tmp;
    char line[LINE_MAX] = {0}, *s;
    int row = 8, col = 1;

#ifdef DEBUG
    PGN_DUMP("%s:%d: FEN line is '%s'\n", __FILE__, __LINE__, str);
#endif
    strncpy(line, str, sizeof(line)-1);
    s = line;
    pgn_reset_enpassant(b);

    while ((tmp = strsep(&s, "/")) != NULL) {
	int n;

	if (!VALIDFILE(row))
	    return E_PGN_PARSE;

	while (*tmp) {
	    if (*tmp == ' ')
		goto other;

	    if (isdigit(*tmp)) {
		n = *tmp - '0';

		if (!VALIDFILE(n))
		    return E_PGN_PARSE;

		for (; n; --n, col++)
		    b[RANKTOBOARD(row)][FILETOBOARD(col)].icon =
			pgn_int_to_piece(WHITE, OPEN_SQUARE);
	    }
	    else if (pgn_piece_to_int(*tmp) != -1)
		b[RANKTOBOARD(row)][FILETOBOARD(col++)].icon = *tmp;
	    else
		return E_PGN_PARSE;

	    tmp++;
	}

	row--;
	col = 1;
    }

other:
    tmp++;

    switch (*tmp++) {
	case 'b':
	    *turn = BLACK;
	    break;
	case 'w':
	    *turn = WHITE;
	    break;
	default:
	    return E_PGN_PARSE;
    }

    tmp++;

    while (*tmp && *tmp != ' ') {
	switch (*tmp++) {
	    case 'K':
		SET_FLAG(*flags, GF_WK_CASTLE);
		break;
	    case 'Q':
		SET_FLAG(*flags, GF_WQ_CASTLE);
		break;
	    case 'k':
		SET_FLAG(*flags, GF_BK_CASTLE);
		break;
	    case 'q':
		SET_FLAG(*flags, GF_BQ_CASTLE);
		break;
	    case '-':
		break;
	    default:
		return E_PGN_PARSE;
	}
    }

    tmp++;

    // En passant.
    if (*tmp != '-') {
	if (!VALIDCOL(*tmp))
	    return E_PGN_PARSE;

	col = *tmp++ - 'a';

	if (!VALIDROW(*tmp))
	    return E_PGN_PARSE;

	row = 8 - atoi(tmp++);
	b[row][col].enpassant = 1;
	SET_FLAG(*flags, GF_ENPASSANT);
    }
    else
	tmp++;

    if (*tmp)
	tmp++;

    if (*tmp)
	*ply = atoi(tmp);

    while (*tmp && isdigit(*tmp))
	tmp++;

    if (*tmp)
	tmp++;

    return E_PGN_OK;
}

/*
 * It initializes the board (b) to the FEN tag (if found) and sets the
 * castling and enpassant info for the game 'g'. If 'fen' is set it should be
 * a fen tag and will be parsed rather than the game 'g'.tag FEN tag-> Returns
 * E_PGN_OK on success or if there was both a FEN and SetUp tag with the SetUp
 * tag set to 0. Returns E_PGN_PARSE if there was a FEN parse error, E_PGN_ERR
 * if there was no FEN tag or there was a SetUp tag with a value of 0. Returns
 * E_PGN_OK on success.
 */
pgn_error_t pgn_board_init_fen(GAME g, BOARD b, char *fen)
{
    int n = -1, i = -1;
    BOARD tmpboard;
    unsigned flags = 0;
    char turn = g->turn;
    char ply = 0;

#ifdef DEBUG
    PGN_DUMP("%s:%d: initializing board from FEN\n", __FILE__, __LINE__);
#endif
    pgn_board_init(tmpboard);

    if (!fen) {
	n = pgn_tag_find(g->tag, "Setup");
	i = pgn_tag_find(g->tag, "FEN");
    }

    /*
     * If the FEN tag exists and there is no SetUp tag go ahead and parse it.
     * If there is a SetUp tag only parse the FEN tag if the value is 1.
     */
    if ((n >= 0 && i >= 0 && atoi(g->tag[n]->value) == 1)
	    || (i >= 0 && n == -1) || fen) {
	if ((n = parse_fen_line(tmpboard, &flags, &turn, &ply,
			(fen) ? fen : g->tag[i]->value)) != E_PGN_OK)
	    return E_PGN_PARSE;
	else {
	    memcpy(b, tmpboard, sizeof(BOARD));
	    CLEAR_FLAG(g->flags, GF_WK_CASTLE);
	    CLEAR_FLAG(g->flags, GF_WQ_CASTLE);
	    CLEAR_FLAG(g->flags, GF_BK_CASTLE);
	    CLEAR_FLAG(g->flags, GF_BQ_CASTLE);
	    g->flags |= flags;
	    g->turn = turn;
	    g->ply = ply;
	}
    }
    else
	return (i >= 0 && n >= 0) ? E_PGN_OK : E_PGN_ERR;

    return E_PGN_OK;
}

/*
 * Allocates a new game and increments 'gtotal'. 'gindex' is then set to the
 * new game. Returns E_PGN_ERR if there was a memory allocation error or
 * E_PGN_OK on success.
 */
pgn_error_t pgn_new_game()
{
    GAME *g;
    GAME newg;
    int t = gtotal + 1;

#ifdef DEBUG
    PGN_DUMP("%s:%d: allocating new game\n", __FILE__, __LINE__);
#endif
    gindex = t - 1;

    if ((g = realloc(game, t * sizeof(GAME))) == NULL) {
	warn("realloc()");
	return E_PGN_ERR;
    }

    game = g;

    if ((newg = calloc(1, sizeof(struct game_s))) == NULL) {
	warn("calloc()");
	return E_PGN_ERR;
    }

    game[gindex] = newg;

    if ((game[gindex]->hp = calloc(1, sizeof(HISTORY *))) == NULL) {
	free(game[gindex]);
	warn("calloc()");
	return E_PGN_ERR;
    }

    game[gindex]->hp[0] = NULL;
    game[gindex]->history = game[gindex]->hp;
    game[gindex]->side = game[gindex]->turn = WHITE;
    SET_FLAG(game[gindex]->flags, GF_WK_CASTLE|GF_WQ_CASTLE|GF_WQ_CASTLE|
	    GF_BK_CASTLE|GF_BQ_CASTLE);
    pgn_board_init(pgn_board);
    set_default_tags(game[gindex]);
    gtotal = t;
    return E_PGN_OK;
}

static int read_file(FILE *fp)
{
#ifdef DEBUG
    char buf[LINE_MAX] = {0}, *p = buf;
#endif
    int c = 0;
    int parse_error = 0;
    BOARD old;

    while (1) {
	int nextchar = 0;
	int lastchar = c;
	int n;

	/*
	 * A parse error may have occured at EOF.
	 */
	if (parse_error) {
	    pgn_ret = E_PGN_PARSE;

	    if (!game)
		pgn_new_game();

	    SET_FLAG(game[gindex]->flags, GF_PERROR);
	}

	if ((c = Fgetc(fp)) == EOF) {
	    if (feof(fp))
		break;

	    if (ferror(fp)) {
		clearerr(fp);
		continue;
	    }
	}

	if (!isascii(c)) {
	    parse_error = 1;
	    continue;
	}

	if (c == '\015')
	    continue;

	nextchar = Fgetc(fp);
	Ungetc(nextchar, fp);

	/*
	 * If there was a move text parsing error, keep reading until the end
	 * of the current game discarding the data.
	 */
	if (parse_error) {
	    pgn_ret = E_PGN_PARSE;

	    if (game[gindex]->ravlevel)
		return 1;

	    if (pgn_config.stop)
		return E_PGN_PARSE;

	    if (c == '\n' && (nextchar == '\n' || nextchar == '\015')) {
		parse_error = 0;
		nulltags = 1;
		tag_section = 0;
	    }
	    else
		continue;
	}

	// New game reached.
	if (c == '\n' && (nextchar == '\n' || nextchar == '\015')) {
	    if (tag_section)
		continue;

	    nulltags = 1;
	    tag_section = 0;
	    continue;
	}

	/*
	 * PGN: Application comment. The '%' must be on the first column of
	 * the line. The comment continues until the end of the current line.
	 */
	if (c == '%') {
	    if (lastchar == '\n' || lastchar == 0) {
		while ((c = Fgetc(fp)) != EOF && c != '\n');
		continue;
	    }

	    // Not sure what to do here.
	}

	if (isspace(c))
	    continue;

	// PGN: Reserved.
	if (c == '<' || c == '>')
	    continue;

	/*
	 * PGN: Recurrsive Annotated Variation. Read rav_text() for more
	 * info.
	 */
	if (c == '(' || c == ')') {
	    switch (rav_text(game[gindex], fp, c, old)) {
		case -1:
		    /*
		     * This is the end of the current RAV. This function has
		     * been called from rav_text(). Returning from this point
		     * will put us back in rav_text().
		     */
		    if (game[gindex]->ravlevel > 0)
			return pgn_ret;

		    /*
		     * We're back at the root move. Continue as normal.
		     */
		    break;
		case 1:
		    parse_error = 1;
		    continue;
		default:
		    /*
		     * Continue processing-> Probably the root move.
		     */
		    break;
	    }

	    if (!game[gindex]->ravlevel && pgn_rav)
		parse_error = 1;

	    continue;
	}

	// PGN: Numeric Annotation Glyph.
	if (c == '$' || c == '!' || c == '?' || c == '+' || c == '-' ||
		c == '~' || c == '=') {
	    Ungetc(c, fp);
	    nag_text(game[gindex], fp);
	    continue;
	}

	/*
	 * PGN: Annotation. The ';' comment continues until the end of the
	 * current line. The '{' type comment continues until a '}' is
	 * reached.
	 */
	if (c == '{' || c == ';') {
	    annotation_text(game[gindex], fp, (c == '{') ? '}' : '\n');
	    continue;
	}

	// PGN: Roster tag->
	if (c == '[') {
	    // First roster tag found. Initialize the data structures.
	    if (!tag_section) {
		nulltags = 0;
		tag_section = 1;

		if (gtotal && pgn_history_total(game[gindex]->hp))
		    game[gindex]->hindex = pgn_history_total(game[gindex]->hp) - 1;

		if (pgn_new_game() != E_PGN_OK) {
		    pgn_ret = E_PGN_ERR;
		    break;
		}

		memcpy(old, pgn_board, sizeof(BOARD));
	    }

	    if (tag_text(game[gindex], fp))
		parse_error = 1; // FEN tag parse error.

	    continue;
	}

	// PGN: End-of-game markers.
	if ((isdigit(c) && (nextchar == '-' || nextchar == '/')) || c == '*') {
	    Ungetc(c, fp);

	    if (eog_text(game[gindex], fp)) {
		parse_error = 1;
		continue;
	    }

	    nulltags = 1;
	    tag_section = 0;

	    if (!game[gindex]->done_fen_tag) {
		if (pgn_tag_find(game[gindex]->tag, "FEN") != -1 &&
			pgn_board_init_fen(game[gindex], pgn_board, NULL)) {
		    parse_error = 1;
		    continue;
		}

		game[gindex]->pgn_fen_tag = pgn_tag_find(game[gindex]->tag, "FEN");
		game[gindex]->done_fen_tag = 1;
	    }

	    continue;
	}

	// PGN: Move text.
	if ((isdigit(c) && c != '0') || VALIDCOL(c) || c == 'N' || c == 'K'
		    || c == 'Q' || c == 'B' || c == 'R' || c == 'P' ||
		    c == 'O') {
	    Ungetc(c, fp);

	    // PGN: If a FEN tag exists, initialize the board to the value.
	    if (tag_section) {
		if (pgn_tag_find(game[gindex]->tag, "FEN") != E_PGN_ERR &&
			(n = pgn_board_init_fen(game[gindex], pgn_board,
						NULL)) == E_PGN_PARSE) {
		    parse_error = 1;
		    continue;
		}

		game[gindex]->done_fen_tag = 1;
		game[gindex]->pgn_fen_tag = pgn_tag_find(game[gindex]->tag, "FEN");
		tag_section = 0;
	    }

	    /*
	     * PGN: Import format doesn't require a roster tag section. We've
	     * arrived to the move text section without any tags so we
	     * initialize a new game which set's the default tags and any tags
	     * from the configuration file.
	     */
	    if (nulltags) {
		if (gtotal)
		    game[gindex]->hindex = pgn_history_total(game[gindex]->hp) - 1;

		if (pgn_new_game() != E_PGN_OK) {
		    pgn_ret = E_PGN_ERR;
		    break;
		}

		memcpy(old, pgn_board, sizeof(BOARD));
		nulltags = 0;
	    }

	    memcpy(old, pgn_board, sizeof(BOARD));

	    if (move_text(game[gindex], fp)) {
		if (pgn_tag_add(&game[gindex]->tag, "Result", "*") ==
			E_PGN_ERR) {
		    warn("pgn_tag_add()");
		    pgn_ret = E_PGN_ERR;
		}

		SET_FLAG(game[gindex]->flags, GF_PERROR);
		parse_error = 1;
	    }

	    continue;
	}

#ifdef DEBUG
	*p++ = c;

	PGN_DUMP("%s:%d: unparsed: '%s'\n", __FILE__, __LINE__, buf);

	if (strlen(buf) + 1 == sizeof(buf))
	    bzero(buf, sizeof(buf));
#endif

	continue;
    }

    return pgn_ret;
}

/*
 * Parses a PGN_FILE which was opened with pgn_open(). If 'pgn' is NULL then a
 * single empty game will be allocated. If there is a parsing error
 * E_PGN_PARSE is returned, if there was a memory allocation error E_PGN_ERR
 * is returned, otherwise E_PGN_OK is returned and the global 'gindex' is set
 * to the last parsed game in the file and the global 'gtotal' is set to the
 * total number of games in the file. The file should be closed with
 * pgn_close() after processing.
 */
pgn_error_t pgn_parse(PGN_FILE *pgn)
{
    int i;

    if (!pgn) {
	reset_game_data();
	pgn_ret = pgn_new_game();
	goto done;
    }

    reset_game_data();
    nulltags = 1;
    fseek(pgn->fp, 0, SEEK_END);
    pgn_fsize = ftell(pgn->fp);
    fseek(pgn->fp, 0, SEEK_SET);
#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN parsing->..\n", __FILE__, __LINE__);
#endif
    parsing_file = 1;
    pgn_ret = read_file(pgn->fp);
    parsing_file = 0;

#ifdef DEBUG
    PGN_DUMP("%s:%d: END parsing->..\n", __FILE__, __LINE__);
#endif

    if (gtotal < 1)
	pgn_new_game();

done:
    gtotal = gindex + 1;

    for (i = 0; i < gtotal; i++) {
	game[i]->history = game[i]->hp;
	game[i]->hindex = pgn_history_total(game[i]->hp);
    }

    return pgn_ret;
}

/*
 * Escape '"' and '\' in tag values.
 */
static char *pgn_tag_add_escapes(const char *str)
{
    int i, n;
    int len = strlen(str);
    char buf[MAX_PGN_LINE_LEN] = {0};

    for (i = n = 0; i < len; i++, n++) {
	switch (str[i]) {
	    case '\\':
	    case '\"':
		buf[n++] = '\\';
		break;
	    default:
		break;
	}

	buf[n] = str[i];
    }

    buf[n] = '\0';
    return buf[0] ? strdup (buf) : NULL;
}

static void Fputc(int c, FILE *fp, int *len)
{
    int i = *len;

    if (c != '\n' && i + 1 > 80)
	Fputc('\n', fp, &i);

    if (pgn_lastc == '\n' && c == ' ') {
	*len = pgn_mpl = 0;
	return;
    }

    if (fputc(c, fp) == EOF)
	warn("PGN Save");
    else {
	if (c == '\n')
	    i = pgn_mpl = 0;
	else
	    i++;
    }

    *len = i;
    pgn_lastc = c;
}

static void putstring(FILE *fp, char *str, int *len)
{
    char *p;

    for (p = str; *p; p++) {
	int n = 0;

	while (*p && *p != ' ')
	    n++, p++;

	if (n + *len > 80)
	    Fputc('\n', fp, len);

	p -= n;
	Fputc(*p, fp, len);
    }
}

/*
 * See pgn_write() for more info.
 */
static void write_comments_and_nag(FILE *fp, HISTORY *h, int *len)
{
    int i;
    char tmp[16];

#ifdef DEBUG
    PGN_DUMP("%s:%d: writing comments and nag\n", __FILE__, __LINE__);
#endif

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (h->nag[i]) {
	    Fputc(' ', fp, len);
	    Fputc('$', fp, len);
	    putstring(fp, itoa(h->nag[i], tmp), len);
	}
    }

    if (h->comment) {
	Fputc('\n', fp, len);
	putstring(fp, " {", len);
	putstring(fp, h->comment, len);
	Fputc('}', fp, len);
    }
}

static void write_move_text(FILE *fp, HISTORY *h, int *len)
{
    Fputc(' ', fp, len);
    putstring(fp, h->move, len);

    if (!pgn_config.reduced)
	write_comments_and_nag(fp, h, len);
}

static void write_all_move_text(FILE *fp, HISTORY **h, int m, int *len)
{
    int i;
    HISTORY **hp = NULL;
    char tmp[16];

    for (i = 0; h[i]; i++) {
	if (pgn_write_turn == WHITE) {
	    if (pgn_config.mpl && pgn_mpl == pgn_config.mpl) {
		pgn_mpl = 0;
		Fputc('\n', fp, len);
	    }

	    if (m > 1 && i > 0)
		Fputc(' ', fp, len);

	    if (strlen(itoa(m, tmp)) + 1 + *len > 80)
		Fputc('\n', fp, len);

	    putstring(fp, itoa(m, tmp), len);
	    Fputc('.', fp, len);
	    pgn_mpl++;
	}

	write_move_text(fp, h[i], len);

	if (!pgn_config.reduced && h[i]->rav) {
	    int oldm = m;
	    int oldturn = pgn_write_turn;

	    ravlevel++;
	    putstring(fp, " (", len);

	    /*
	     * If it's WHITE's turn the move number will be added above after
	     * the call to write_all_move_text() below.
	     */
	    if (pgn_write_turn == BLACK) {
	      putstring(fp, itoa(m, tmp), len);
		putstring(fp, "...", len);
	    }

	    hp = h[i]->rav;
	    write_all_move_text(fp, hp, m, len);
	    m = oldm;
	    pgn_write_turn = oldturn;
	    putstring(fp, ")", len);
	    ravlevel--;

	    if (ravlevel && h[i + 1])
		Fputc(' ', fp, len);

	    if (h[i + 1] && !ravlevel)
		Fputc(' ', fp, len);

	    if (pgn_write_turn == WHITE && h[i + 1]) {
	        putstring(fp, itoa(m, tmp), len);
		putstring(fp, "...", len);
	    }
	}

	if (pgn_write_turn == BLACK)
	    m++;

	pgn_write_turn = (pgn_write_turn == WHITE) ? BLACK : WHITE;
    }
}

static char *compression_cmd(const char *filename, int expand)
{
    char command[PATH_MAX];
    int len = strlen(filename);

    if (filename[len - 4] == '.' && filename[len - 3] == 'z' &&
	    filename[len - 2] == 'i' && filename[len - 1] == 'p' &&
	    filename[len] == '\0') {
	if (expand)
	    snprintf(command, sizeof(command), "unzip -p %s 2>/dev/null",
		    filename);
	else
	    snprintf(command, sizeof(command), "zip -9 >%s 2>/dev/null",
		    filename);

	return strdup (command);
    }
    else if (filename[len - 3] == '.' && filename[len - 2] == 'g' &&
	    filename[len - 1] == 'z' && filename[len] == '\0') {
	if (expand)
	    snprintf(command, sizeof(command), "gzip -dc %s", filename);
	else
	    snprintf(command, sizeof(command), "gzip -c 1>%s", filename);

	return strdup (command);
    }
    else if (filename[len - 2] == '.' && filename[len - 1] == 'Z' &&
	    filename[len] == '\0') {
	if (expand)
	    snprintf(command, sizeof(command), "uncompress -c %s", filename);
	else
	    snprintf(command, sizeof(command), "compress -c 1>%s", filename);

	return strdup (command);
    }
    else if ((filename[len - 4] == '.' && filename[len - 3] == 'b' &&
	    filename[len - 2] == 'z' && filename[len - 1] == '2' &&
	    filename[len] == '\0') || (filename[len - 3] == '.' &&
		filename[len - 2] == 'b' && filename[len - 1] == 'z' &&
		filename[len] == '\0')) {
	if (expand)
	    snprintf(command, sizeof(command), "bzip2 -dc %s", filename);
	else
	    snprintf(command, sizeof(command), "bzip2 -zc 1>%s", filename);

	return strdup (command);
    }

    return NULL;
}

static int copy_file(FILE *fp, const char *dst)
{
    FILE *ofp;
    char line[LINE_MAX];
    char *cmd = compression_cmd(dst, 0);

    if ((ofp = popen(cmd, "w")) == NULL) {
	free (cmd);
	return 1;
    }

    free (cmd);
    fseek(fp, 0, SEEK_SET);

    while ((fgets(line, sizeof(line), fp)) != NULL)
	fprintf(ofp, "%s", line);

    pclose(ofp);
    return 0;
}

/*
 * Closes and free's a PGN file handle.
 */
pgn_error_t pgn_close(PGN_FILE *pgn)
{
    if (!pgn)
	return E_PGN_INVALID;

    if (pgn->pipe) {
	/*
	 * Appending to a compressed file.
	 */
	if (pgn->tmpfile) {
	    if (copy_file(pgn->fp, pgn->filename))
		return E_PGN_ERR;

	    fclose(pgn->fp);
	    unlink(pgn->tmpfile);
	    free(pgn->tmpfile);
	}
	else
	    pclose(pgn->fp);
    }
    else
	fclose(pgn->fp);

    free(pgn->filename);
    free(pgn);
    return E_PGN_OK;
}

/*
 * Opens a file 'filename' with the given 'mode'. 'mode' should be "r" for
 * reading, "w" for writing (will truncate if the file exists) or "a" for
 * appending to an existing file or creating a new one. Returns E_PGN_OK on
 * success and sets 'result' to a file handle for use will the other file
 * functions or E_PGN_ERR if there is an error opening the file in which case
 * errno will be set to the error or E_PGN_INVALID if 'mode' is an invalid
 * mode or if 'filename' is not a regular file.
 */
pgn_error_t pgn_open(const char *filename, const char *mode, PGN_FILE **result)
{
    FILE *fp = NULL, *tfp = NULL;
    char buf[PATH_MAX], *p;
    char *cmd = NULL;
    PGN_FILE *pgn;
    int m;
    int append = 0;
    struct stat st;
    int ret = E_PGN_ERR;


#ifdef DEBUG
    PGN_DUMP("%s:%d: BEGIN opening %s\n", __FILE__, __LINE__, filename);
#endif

    if (!filename || !mode)
	return E_PGN_INVALID;

    if (strcmp(mode, "r") == 0)
	m = 1;
    else if (strcmp(mode, "w") == 0)
	m = 0;
    else if (strcmp(mode, "a") == 0) {
	m = 0;
	append = 1;
    }
    else {
	return E_PGN_INVALID;
    }

    pgn = calloc(1, sizeof(PGN_FILE));

    if (!pgn)
	goto fail;

    if (strcmp(filename, "-") != 0) {
	if (m && access(filename, R_OK) == -1)
	    goto fail;

	if (stat(filename, &st) == -1 && !m && errno != ENOENT)
	    goto fail;

	if (m && !S_ISREG(st.st_mode)) {
	    ret = E_PGN_INVALID;
	    goto fail;
	}

	if ((cmd = compression_cmd(filename, m)) != NULL) {
            char tmp[21];
            int fd;

#ifdef HAVE_MKSTEMP
            snprintf (tmp, sizeof(tmp), "cboard.XXXXXX");
#else
            if (tmpnam(tmp) == NULL)
                goto fail;
#endif

	    pgn->pipe = 1;

	    if (append && access(filename, R_OK) == 0) {
		free (cmd);
		cmd = compression_cmd(filename, 1);

		if ((fp = popen(cmd, "r")) == NULL)
		    goto fail;

#ifdef HAVE_MKSTEMP
                fd = mkstemp (tmp);
                if (fd == -1)
                    goto fail;
#else
		if ((fd = open(tmp, O_RDWR|O_EXCL|O_CREAT, 0600)) == -1)
		    goto fail;
#endif
		if ((tfp = fdopen(fd, "a+")) == NULL)
		    goto fail;

		while ((p = fgets(buf, sizeof(buf), fp)) != NULL)
		    fprintf(tfp, "%s", p);

		pclose(fp);
		pgn->fp = tfp;
		pgn->tmpfile = strdup(tmp);
		goto done;
	    }

	    if ((fp = popen(cmd, m ? "r" : "w")) == NULL)
		goto fail;

	    if (m) {
#ifdef HAVE_MKSTEMP
                fd = mkstemp (tmp);
                if (fd == -1)
                    goto fail;
#else
		if ((fd = open(tmp, O_RDWR|O_EXCL|O_CREAT|O_TRUNC, 0600)) == -1)
		    goto fail;
#endif
		if ((tfp = fdopen(fd, "w+")) == NULL)
		    goto fail;

		while ((p = fgets(buf, sizeof(buf), fp)) != NULL)
		    fprintf(tfp, "%s", p);

		pclose(fp);
		pgn->fp = tfp;
		pgn->tmpfile = strdup(tmp);
	    }
	    else
		pgn->fp = fp;
	}
	else {
	    if ((fp = fopen(filename, mode)) == NULL)
		goto fail;

	    pgn->fp = fp;
	}
    }
    else
	pgn->fp = stdout;

done:
    if (*filename != '/') {
	if (getcwd(buf, sizeof(buf)) == NULL) {
	    if (pgn->tmpfile)
		free(pgn->tmpfile);

	    goto fail;
	}

	asprintf(&p, "%s/%s", buf, filename);
	pgn->filename = p;
    }
    else
	pgn->filename = strdup(filename);

    free (cmd);
    *result = pgn;
    return E_PGN_OK;

fail:
    if (fp)
	fclose(fp);

    free (cmd);
    free(pgn);
    return ret;
}

/*
 * Returns E_PGN_OK if 'filename' is a recognized compressed filetype or
 * E_PGN_ERR if not.
 */
pgn_error_t pgn_is_compressed(const char *filename)
{
    char *s = compression_cmd(filename, 0);
    pgn_error_t e = s ? E_PGN_OK : E_PGN_ERR;

    free (s);
    return e;
}

/*
 * Gets the value of config flag 'f'. The next argument should be a pointer of
 * the config type which is set to the value of 'f'. Returns E_PGN_ERR if 'f'
 * is an invalid flag or E_PGN_OK on success.
 */
pgn_error_t pgn_config_get(pgn_config_flag f, ...)
{
    va_list ap;
    int *intval;
    long *longval;
    pgn_progress *progress;

    va_start(ap, f);

    switch (f) {
	case PGN_STRICT_CASTLING:
	    intval = va_arg(ap, int *);
	    *intval = pgn_config.strict_castling;
	    va_end(ap);
	    return E_PGN_OK;
	case PGN_REDUCED:
	    intval = va_arg(ap, int *);
	    *intval = pgn_config.reduced;
	    va_end(ap);
	    return E_PGN_OK;
	case PGN_MPL:
	    intval = va_arg(ap, int *);
	    *intval = pgn_config.mpl;
	    va_end(ap);
	    return E_PGN_OK;
	case PGN_STOP_ON_ERROR:
	    intval = va_arg(ap, int *);
	    *intval = pgn_config.stop;
	    va_end(ap);
	    return E_PGN_OK;
	case PGN_PROGRESS:
	    longval = va_arg(ap, long *);
	    *longval = pgn_config.stop;
	    va_end(ap);
	    return E_PGN_OK;
	case PGN_PROGRESS_FUNC:
	    progress = va_arg(ap, pgn_progress*);
	    *progress = pgn_config.pfunc;
	    va_end(ap);
	    return E_PGN_OK;
#ifdef DEBUG
	case PGN_DEBUG:
	    intval = va_arg(ap, int *);
	    *intval = dumptofile;
	    va_end(ap);
	    return E_PGN_OK;
#endif
	default:
	    break;
    }

    return E_PGN_ERR;
}

/*
 * Sets config flag 'f' to the next argument. Returns E_PGN_OK on success or
 * E_PGN_ERR if 'f' is an invalid flag or E_PGN_INVALID if 'val' is an invalid
 * flag value.
 */
pgn_error_t pgn_config_set(pgn_config_flag f, ...)
{
    va_list ap;
    int n;
    int ret = E_PGN_OK;

    va_start(ap, f);

    switch (f) {
	case PGN_REDUCED:
	    n = va_arg(ap, int);

	    if (n != 1 && n != 0) {
		ret = E_PGN_INVALID;
		break;
	    }

	    pgn_config.reduced = n;
	    break;
	case PGN_MPL:
	    n = va_arg(ap, int);

	    if (n < 0) {
		ret = E_PGN_INVALID;
		break;
	    }

	    pgn_config.mpl = n;
	    break;
	case PGN_STOP_ON_ERROR:
	    n = va_arg(ap, int);

	    if (n != 1 && n != 0) {
		ret = E_PGN_INVALID;
		break;
	    }

	    pgn_config.stop = n;
	    break;
	case PGN_PROGRESS:
	    n = va_arg(ap, long);
	    pgn_config.progress = n;
	    break;
	case PGN_PROGRESS_FUNC:
	    pgn_config.pfunc = va_arg(ap, pgn_progress);
	    break;
	case PGN_STRICT_CASTLING:
	    n = va_arg(ap, int);
	    pgn_config.strict_castling = n;
	    break;
#ifdef DEBUG
	case PGN_DEBUG:
	    n = va_arg(ap, int);
	    dumptofile = (n > 0) ? 1 : 0;
	    break;
#endif
	default:
	    ret = E_PGN_ERR;
	    break;
    }

    va_end(ap);
    return ret;
}

/*
 * Writes a PGN formatted game 'g' to a file which was opened with pgn_open().
 * See 'pgn_config_flag' for output options. Returns E_PGN_ERR if there was a
 * memory allocation or write error and E_PGN_OK on success. It is important
 * to use pgn_close() afterwards if the file is a recognized compressed file
 * type otherwise the created temporary file wont be copied to the destination
 * filename.
 */
pgn_error_t pgn_write(PGN_FILE *pgn, GAME g)
{
    int i;
    int len = 0;

    if (!pgn)
	return E_PGN_ERR;

    pgn_write_turn = (TEST_FLAG(g->flags, GF_BLACK_OPENING)) ? BLACK : WHITE;
    pgn_tag_sort(g->tag);

#ifdef DEBUG
    PGN_DUMP("%s:%d: writing tag section\n", __FILE__, __LINE__);
#endif

    for (i = 0; g->tag[i]; i++) {
	struct tm tp;
	char tbuf[11] = {0};
	char *tmp;
	char *escaped;

	if (pgn_config.reduced && i == 7)
	    break;

	if (strcmp(g->tag[i]->name, "Date") == 0) {
	    memset(&tp, 0, sizeof(struct tm));

	    if (strptime(g->tag[i]->value, PGN_TIME_FORMAT, &tp) != NULL) {
		len = strftime(tbuf, sizeof(tbuf), PGN_TIME_FORMAT, &tp) + 1;

		if ((tmp = strdup(tbuf)) == NULL)
		    return E_PGN_ERR;

		free(g->tag[i]->value);
		g->tag[i]->value = tmp;
	    }
	}
	else if (strcmp(g->tag[i]->name, "Event") == 0) {
	    if (g->tag[i]->value[0] == '\0') {
		if ((tmp = strdup("?")) == NULL)
		    return E_PGN_ERR;

		free(g->tag[i]->value);
		g->tag[i]->value = tmp;
	    }
	}
	else if (strcmp(g->tag[i]->name, "Site") == 0) {
	    if (g->tag[i]->value[0] == '\0') {
		if ((tmp = strdup("?")) == NULL)
		    return E_PGN_ERR;

		free(g->tag[i]->value);
		g->tag[i]->value = tmp;
	    }
	}
	else if (strcmp(g->tag[i]->name, "Round") == 0) {
	    if (g->tag[i]->value[0] == '\0') {
		if ((tmp = strdup("?")) == NULL)
		    return E_PGN_ERR;

		free(g->tag[i]->value);
		g->tag[i]->value = tmp;
	    }
	}
	else if (strcmp(g->tag[i]->name, "Result") == 0) {
	    if (g->tag[i]->value[0] == '\0') {
		if ((tmp = strdup("*")) == NULL)
		    return E_PGN_ERR;

		free(g->tag[i]->value);
		g->tag[i]->value = tmp;
	    }
	}
	else if (strcmp(g->tag[i]->name, "Black") == 0) {
	    if (g->tag[i]->value[0] == '\0') {
		if ((tmp = strdup("?")) == NULL)
		    return E_PGN_ERR;

		free(g->tag[i]->value);
		g->tag[i]->value = tmp;
	    }
	}
	else if (strcmp(g->tag[i]->name, "White") == 0) {
	    if (g->tag[i]->value[0] == '\0') {
		if ((tmp = strdup("?")) == NULL)
		    return E_PGN_ERR;

		free(g->tag[i]->value);
		g->tag[i]->value = tmp;
	    }
	}

	escaped = pgn_tag_add_escapes(g->tag[i]->value);
	fprintf(pgn->fp, "[%s \"%s\"]\n", g->tag[i]->name,
		(g->tag[i]->value && g->tag[i]->value[0]) ? escaped : "");
	free (escaped);
    }

#ifdef DEBUG
    PGN_DUMP("%s:%d: writing move section\n", __FILE__, __LINE__);
#endif
    Fputc('\n', pgn->fp, &len);
    g->hp = g->history;
    ravlevel = pgn_mpl = 0;

    if (pgn_history_total(g->hp) && pgn_write_turn == BLACK)
	putstring(pgn->fp, "1...", &len);

    write_all_move_text(pgn->fp, g->hp, 1, &len);

    Fputc(' ', pgn->fp, &len);
    putstring(pgn->fp, g->tag[6]->value, &len);
    putstring(pgn->fp, "\n\n", &len);

    if (!pgn_config.reduced)
	CLEAR_FLAG(g->flags, GF_PERROR);

    return E_PGN_OK;
}

/*
 * Clears the enpassant flag for all positions on board 'b'. Returns nothing.
 */
void pgn_reset_enpassant(BOARD b)
{
    int r, c;

#ifdef DEBUG
    PGN_DUMP("%s:%d: resetting enpassant\n", __FILE__, __LINE__);
#endif

    for (r = 0; r < 8; r++) {
	for (c = 0; c < 8; c++)
	    b[r][c].enpassant = 0;
    }
}
