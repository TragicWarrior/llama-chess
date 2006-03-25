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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "chess.h"
#include "pgn.h"
#include "misc.h"

#ifdef DEBUG
#include "debug.h"
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

/*
 * Creates a FEN tag from the current game 'g' and board 'b'. Returns a FEN
 * tag.
 */
char *pgn_game_to_fen(GAME g, BOARD b)
{
    int row, col;
    int i;
    static char buf[MAX_PGN_LINE_LEN], *p;
    int oldturn = g.turn;
    char enpassant[3] = {0}, *e;
    int castle = 0;

    for (i = history_total(g.hp); i >= g.hindex - 1; i--)
	pgn_switch_turn(&g.turn);

    p = buf;

    for (row = 0; row < 8; row++) {
	int count = 0;

	for (col = 0; col < 8; col++) {
	    if (b[row][col].enpassant == 1) {
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
    *p++ = (g.side == WHITE) ? 'w' : 'b';
    *p++ = ' ';

    if (TEST_FLAG(g.flags, GF_WK_CASTLE) && pgn_piece_to_int(b[7][7].icon) ==
	    ROOK && isupper(b[7][7].icon) && pgn_piece_to_int(b[7][4].icon) ==
		KING && isupper(b[7][4].icon)) {
	*p++ = 'K';
	castle = 1;
    }

    if (TEST_FLAG(g.flags, GF_WQ_CASTLE) && pgn_piece_to_int(b[7][0].icon) ==
	    ROOK && isupper(b[7][0].icon) && pgn_piece_to_int(b[7][4].icon) ==
		KING && isupper(b[7][4].icon)) {
	*p++ = 'Q';
	castle = 1;
    }

    if (TEST_FLAG(g.flags, GF_BK_CASTLE) && pgn_piece_to_int(b[0][7].icon) ==
	    ROOK && islower(b[0][7].icon) && pgn_piece_to_int(b[0][4].icon) ==
		KING && islower(b[0][4].icon)) {
	*p++ = 'k';
	castle = 1;
    }

    if (TEST_FLAG(g.flags, GF_BQ_CASTLE) && pgn_piece_to_int(b[0][0].icon) ==
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

    /*
    if (g.ply >= 10) {
	*p++ = '0' + (g.ply / 10);
	*p++ = '0' + g.ply % 10;
    }
    else
	*p++ = '0' + g.ply;
	*/

    *p++ = '0';
    *p++ = ' ';

    i = (g.hindex + 1) / 2;

    if (i >= 100) {
	*p++ = '0' + (i / 100);
	*p++ = '0' + (i / 100) / 10;
	*p++ = '0' + (i / 100) % 10;
    }
    else if (i >= 10) {
	*p++ = '0' + (i / 10);
	*p++ = '0' + i % 10;
    }
    else
	*p++ = '0' + i;

    *p = '\0';

    g.turn = oldturn;
    return buf;
}

/*
 * Returns the total number of moves in 'h' or 0 if none.
 */
int history_total(HISTORY **h)
{
    int i;

    if (!h)
	return 0;

    for (i = 0; h[i]; i++);
    return i;
}

/* 
 * Deallocates the all the data for 'h' from position 'start' in the array.
 */
void history_free(HISTORY **h, int start)
{
    int i;

    if (!h)
	return;

    for (i = start; h[i]; i++) {
	if (h[i]->comment)
	    free(h[i]->comment);

	if (h[i]->rav) {
	    history_free(h[i]->rav, 0);
	    free(h[i]->rav);
	}

	if (h[i]->move)
	    free(h[i]->move);
    }

    free(h);
}

/*
 * Returns the history ply 'n' from 'h'. If 'n' is out of range then NULL is
 * returned.
 */
HISTORY *history_by_n(HISTORY **h, int n)
{
    if (n < 0 || n > history_total(h) - 1)
	return NULL;

    return h[n];
}

/*
 * Appends move 'm' to 'h' and increments 'n'.
 */
void history_add(HISTORY ***h, unsigned char *n, const char *m)
{
    HISTORY **new = *h;
    int t = history_total(new);

    new = Realloc(new, (t + 2) * sizeof(HISTORY *));
    new[t] = Calloc(1, sizeof(HISTORY));
    new[t]->move = strdup(m);
    new[t]->n = *n = t;
    new[++t] = NULL;
    *h = new;
}

/*
 * Resets the game 'g' using board 'b' up to history move 'n'.
 */
int history_update_board(GAME *g, BOARD b, int n)
{
    int i = 0;
    BOARD tb;
    int ret = 0;

    if (TEST_FLAG((*g).flags, GF_BLACK_OPENING))
	(*g).turn = BLACK;
    else
	(*g).turn = WHITE;

#if 0
    if (TEST_FLAG((*g).flags, GF_PERROR))
	SET_FLAG(flags, GF_PERROR);

    if (TEST_FLAG((*g).flags, GF_MODIFIED))
	SET_FLAG(flags, GF_MODIFIED);

    if (TEST_FLAG((*g).flags, GF_DELETE))
	SET_FLAG(flags, GF_DELETE);

    if (TEST_FLAG((*g).flags, GF_GAMEOVER))
	SET_FLAG(flags, GF_GAMEOVER);
    
    (*g).flags = flags;
#endif
    pgn_init_board(tb);

    if (pgn_init_fen_board(g, tb, NULL))
	return 1;

    for (i = 0; i < n; i++) {
	HISTORY *h;

	if ((h = history_by_n((*g).hp, i)) == NULL)
	    break;
	
	if (pgn_validate_move(g, tb, h->move)) {
	    ret = 1;
	    break;
	}

	pgn_switch_turn(&(*g).turn);
    }

    if (ret == 0)
	memcpy(b, tb, sizeof(BOARD));

    return ret;
}

/*
 * Updates the game 'g' using board 'b' to the next 'n'th history move. The
 * 's' parameter is either 2 for a wholestep or 1 for a halfstep.
 */
void history_previous(GAME *g, BOARD b, int n, int s)
{
    if (s < 1 || s > 2)
	return;

    if ((*g).hindex - n < 0) {
	if ((n == 2 && s == 2) || (n == 1 && s == 1))
	    (*g).hindex = history_total((*g).hp);
	else
	    (*g).hindex = 0;
    }
    else
	(*g).hindex -= n;

    history_update_board(g, b, (*g).hindex);
}

/*
 * Updates the game 'g' using board 'b' to the previous 'n'th history move.
 * 's' parameter is either 2 for a wholestep or 1 for a halfstep.
 */
void history_next(GAME *g, BOARD b, int n, int s)
{
    if ((*g).hindex + n > history_total((*g).hp)) {
	if ((n == 2 && s == 2) || (n == 1 && s == 1))
	    (*g).hindex = 0;
	else
	    (*g).hindex = history_total((*g).hp);
    }
    else
	(*g).hindex += n;

    history_update_board(g, b, game[gindex].hindex);
}
/*
 * Converts the character piece 'p' to an integer.
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

    return -1;
}

/*
 * Converts the integer piece 'n' to a character.
 */
int pgn_int_to_piece(char turn, int n)
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
	    break;
    }

    return (turn == WHITE) ? toupper(p) : p;
}

/*
 * Finds a tag 'name' in the structure array 't'. Returns the location in the
 * array of the found tag or -1 on failure.
 */
int pgn_find_tag(TAG *t, int total, const char *name)
{
    int i;

    for (i = 0; i < total; i++) {
	if (strcasecmp(t[i].name, name) == 0)
	    return i;
    }

    return -1;
}

static int tag_compare(const void *s1, const void *s2)
{
    const TAG *ss1 = s1;
    const TAG *ss2 = s2;

    return strcmp(ss1->name, ss2->name);
}

/*
 * Sorts the tag array in game 'g'. The first seven tags are in order of the
 * PGN standard so don't sort'em.
 */
void pgn_sort_tags(GAME g)
{
    TAG *t = g.tag + 7;

    qsort(t, g.tindex - 7, sizeof(TAG), tag_compare);
}

// FIXME ???
#if 0
static int end_of_game(GAME g, const char *str)
{
    int i;
    int len;

    for (i = 0; i < NARRAY(fancy_results); i++) {
	if (strstr(str, fancy_results[i].pgn) != NULL) {
	    len = strlen(fancy_results[i].pgn) + 1;
	    g.tag[TAG_RESULT].value = Realloc(g.tag[TAG_RESULT].value, len);
	    strncpy(g.tag[TAG_RESULT].value, fancy_results[i].pgn, len);
	    return 1;
	}
    }

    return 0;
}
#endif

/*
 * Adds a tag 'name' with value 'value' to the pointer to array 'dst'. The 'n'
 * parameter is incremented to the new total of array 'dst'. If a duplicate
 * tag 'name' was found then the existing tag is updated to the new 'value'.
 * Returns 1 if a duplicate tag was found or 0 otherwise.
 */
int pgn_add_tag(TAG **dst, unsigned char *n, char *name, char *value)
{
    int i, idx = *n;
    TAG *tdata = *dst;
    int len = 0;

    name = trim(name);
    value = trim(value);

    for (i = 0; i < idx; i++) {
	if (strcasecmp(tdata[i].name, name) == 0) {
	    len = (value) ? strlen(value) + 1 : 1;
	    tdata[i].value = Realloc(tdata[i].value, len);
	    strncpy(tdata[i].value, (value) ? value : "", len);
	    *dst = tdata;
	    return 1;
	}
    }

    tdata = Realloc(tdata, (idx + 2) * sizeof(TAG));

    len = strlen(name) + 1;
    tdata[idx].name = Malloc(len);
    strncpy(tdata[idx].name, name, len);

    if (value) {
	len = strlen(value) + 1;
	tdata[idx].value = Malloc(len);
	strncpy(tdata[idx].value, value, len);
    }

    memset(&tdata[++idx], '\0', sizeof(TAG));
    *n = idx;
    *dst = tdata;
    return 0;
}

static char *remove_tag_escapes(const char *str)
{
    int i, n;
    int len = strlen(str);
    static char buf[MAX_PGN_LINE_LEN] = {0};

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
    return buf;
}

/*
 * Initializes a new game board.
 */
void pgn_init_board(BOARD b)
{
    int row, col;

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
static void set_default_tags(GAME *g)
{
    time_t now;
    char tbuf[12] = {0};
    struct tm *tp;
    struct passwd *pw = getpwuid(getuid());

    time(&now);
    tp = localtime(&now);
    strftime(tbuf, sizeof(tbuf), PGN_TIME_FORMAT, tp);
    tbuf[11] = 0;

    /* The standard seven tag roster (in order of appearance). */
    pgn_add_tag(&(*g).tag, &(*g).tindex, "Event", "?");
    pgn_add_tag(&(*g).tag, &(*g).tindex, "Site", "?");
    pgn_add_tag(&(*g).tag, &(*g).tindex, "Date", tbuf);
    pgn_add_tag(&(*g).tag, &(*g).tindex, "Round", "-");
    pgn_add_tag(&(*g).tag, &(*g).tindex, "White", pw->pw_gecos);
    pgn_add_tag(&(*g).tag, &(*g).tindex, "Black", "?");
    pgn_add_tag(&(*g).tag, &(*g).tindex, "Result", "*");
}

void pgn_tag_free(TAG *data, int n)
{
    int i;

    for (i = 0; i < n; i++) {
	free(data[i].name);
	free(data[i].value);
    }

    free(data);
}

void pgn_free(GAME g)
{
    history_free(g.history, 0);
    pgn_tag_free(g.tag, g.tindex);
    memset(&g, 0, sizeof(GAME));
}

void pgn_free_all()
{
    int i;

    for (i = 0; i < gtotal; i++)
	pgn_free(game[i]);

    if (game)
	free(game);
    game = NULL;
}

static void reset_game_data()
{
    pgn_free_all();
    gtotal = gindex = 0;
    pgn_init_board(pgnboard);
}

static void skip_leading_space(FILE *fp)
{
    int c;

    while ((c = fgetc(fp)) != EOF && !feof(fp)) {
	if (!isspace(c))
	    break;
    }

    ungetc(c, fp);
}

/*
 * PGN move text section.
 */
static int move_text(GAME *g, FILE *fp)
{
    char m[MAX_SAN_MOVE_LEN + 1] = {0};
    int c;
    int count;
    int dots = 0;
    int digit = 0;

    while((c = fgetc(fp)) != EOF) {
	if (isspace(c))
	    continue;

	if (isdigit(c)) {
	    digit = 1;
	    continue;
	}
	
	if (c == '.') {
	    dots++;
	    continue;
	}

	break;
    }

    if (digit) {
	if (dots > 1) {
	    (*g).turn = BLACK;

	    if ((*g).hindex == 0)
		SET_FLAG((*g).flags, GF_BLACK_OPENING);
	}
	else {
	    (*g).turn = WHITE;

	    if ((*g).hindex == 0)
		CLEAR_FLAG((*g).flags, GF_BLACK_OPENING);
	}
    }
    else {
	if ((*g).hindex > 0)
	    pgn_switch_turn(&(*g).turn);
    }

    ungetc(c, fp);

    if (fscanf(fp, " %[a-hPRNBQK1-9#+=Ox-]%n", m, &count) != 1)
	return 1;

    // In case the file is in a2a4 format, convert this move to SAN format.
    if (pgn_a2a4tosan(g, pgnboard, m) == NULL)
	return 1;

    if (pgn_validate_move(g, pgnboard, m)) {
	// Black opening move?
	if ((*g).hindex == 0) {
	    pgn_switch_turn(&(*g).turn);

	    if (pgn_validate_move(g, pgnboard, m)) {
		// Nope. Parse error.
		pgn_switch_turn(&(*g).turn);
		return 1;
	    }

	    SET_FLAG((*g).flags, GF_BLACK_OPENING);
	}
	else {
	    // Parse error (not an opening move).
	    pgn_switch_turn(&(*g).turn);
	    return 1;
	}
    }

#ifdef DEBUG
    DUMP("%s\n", m);
    dump_board(0, pgnboard);
#endif

    history_add(&(*g).hp, &(*g).hindex, m);
    return 0;
}

/*
 * PGN nag text.
 */
static void nag_text(GAME *g, FILE *fp)
{
    int c, i, t;
    char nags[5], *n = nags;
    int nag = 0;

    while ((c = fgetc(fp)) != EOF && !isspace(c)) {
	if (c == '$') {
	    while ((c = fgetc(fp)) != EOF && isdigit(c))
		*n++ = c;

	    break;
	}

	if (c == '!') {
	    if ((c = fgetc(fp)) == '!')
		nag = 3;
	    else if (c == '?')
		nag = 5;
	    else {
		ungetc(c, fp);
		nag = 1;
	    }

	    break;
	}
	else if (c == '?') {
	    if ((c = fgetc(fp)) == '?')
		nag = 4;
	    else if (c == '!')
		nag = 6;
	    else {
		ungetc(c, fp);
		nag = 2;
	    }

	    break;
	}
	else if (c == '~')
	    nag = 13;
	else if (c == '=') {
	    if ((c = fgetc(fp)) == '+')
		nag = 15;
	    else {
		ungetc(c, fp);
		nag = 10;
	    }

	    break;
	}
	else if (c == '+') {
	    if ((t = fgetc(fp)) == '=')
		nag = 14;
	    else if (t == '-')
		nag = 18;
	    else if (t == '/') {
		if ((i = fgetc(fp)) == '-')
		    nag = 16;
		else
		    ungetc(i, fp);

		break;
	    }
	    else
		ungetc(t, fp);

	    break;
	}
	else if (c == '-') {
	    if ((t = fgetc(fp)) == '+')
		nag = 18;
	    else if (t == '/') {
		if ((i = fgetc(fp)) == '+')
		    nag = 17;
		else
		    ungetc(i, fp);

		break;
	    }
	    else
		ungetc(t, fp);

	    break;
	}
    }

    *n = '\0';

    if (!nag)
	nag = (nags[0]) ? atoi(nags) : 0;

    if (!nag || nag < 0 || nag > 255)
	return;

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if ((*g).hp[(*g).hindex - 1]->nag[i])
	    continue;

	(*g).hp[(*g).hindex - 1]->nag[i] = nag;
	break;
    }

    skip_leading_space(fp);
}

/*
 * PGN move annotation.
 */
static void annotation_text(GAME *g, FILE *fp, int terminator)
{
    int c, lastchar = 0;
    int len = 0;
    int hindex = history_total((*g).hp) - 1;
    char buf[MAX_PGN_LINE_LEN], *a = buf;

    skip_leading_space(fp);

    while ((c = fgetc(fp)) != EOF && c != terminator) {
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
    (*g).hp[hindex]->comment = Realloc((*g).hp[hindex]->comment, ++len);
    strncpy((*g).hp[hindex]->comment, buf, len);
}

/*
 * PGN roster tag.
 */
static void tag_text(GAME *g, FILE *fp)
{
    char name[LINE_MAX], *n = name;
    char value[LINE_MAX], *v = value;
    int c, i = 0;
    int quoted_string = 0;
    int lastchar = 0;

    skip_leading_space(fp);

    /* The tag name is up until the first whitespace. */
    while ((c = fgetc(fp)) != EOF && !isspace(c))
	*n++ = c;

    *n = '\0';
    *name = toupper(*name);
    skip_leading_space(fp);

    /* The value is until the first closing bracket. */
    while ((c = fgetc(fp)) != EOF && c != ']') {
	if (i++ == '\0' && c == '\"') {
	    quoted_string = 1;
	    continue;
	}

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

    strncpy(value, remove_tag_escapes(value), sizeof(value));
    pgn_add_tag(&(*g).tag, &(*g).tindex, name, value);
    skip_leading_space(fp);
}

/*
 * PGN end-of-game marker.
 */
static int eog_text(GAME *g, FILE *fp)
{
    int c, i = 0;
    char buf[8], *p = buf;

    while ((c = fgetc(fp)) != EOF && !isspace(c) && i++ < sizeof(buf))
	*p++ = c;

    *p = 0;
    (*g).tag[TAG_RESULT].value = Realloc((*g).tag[TAG_RESULT].value, 
					 strlen(buf) + 1);
    strcpy((*g).tag[TAG_RESULT].value, buf);
    skip_leading_space(fp);
    return 1;
}

/*
 * This function updates a games move history pointer to a new history
 * instance for the current move.
 */
static int read_file(FILE *);
static int rav_text(GAME *g, FILE *fp, int which)
{
    int hindex = 0;
    char *fen = NULL;

    if (which == '(') {
	if (!ravlevel)
	    /* 
	     * hindex holds the current root move number. We have to know
	     * where to begin in the originial move history (not the pointer
	     * .hp) when ravlevel is deincremented (end of this rav). 
	     */
	    hindex = (*g).hindex;

	ravlevel++;
	(*g).hp[hindex]->rav = Calloc(1, sizeof(HISTORY));
	(*g).hp = (*g).hp[hindex]->rav;
	(*g).hindex = 0;
	fen = strdup(pgn_game_to_fen(*g, pgnboard));

	if (read_file(fp)) {
	    fprintf(stderr, "ACK\n");
	    return 1;
	}

	(*g).hp = (*g).history;
	(*g).hindex = hindex;
	ravlevel--;
	pgn_init_fen_board(g, pgnboard, fen);
	free(fen);
    }
    else if (which == ')')
	return -1;

    return 0;
}

/*
 * See pgn_init_fen_board(). Returns -1 on parse error. 0 may be returned on
 * success when there is no move count in the FEN tag otherwise the move count
 * is returned.
 */
static int parse_fen_line(BOARD b, unsigned *flags, char *turn, char *str)
{
    char *tmp;
    char line[LINE_MAX], *s;
    int row = 8, col = 1;
    int moven;

    strncpy(line, str, sizeof(line));
    s = line;
    board_reset_enpassant(b);

    while ((tmp = strsep(&s, "/")) != NULL) {
	int n;

	if (!VALIDFILE(row))
	    return -1;

	while (*tmp) {
	    if (*tmp == ' ')
		goto other;

	    if (isdigit(*tmp)) {
		n = *tmp - '0';

		if (!VALIDFILE(n))
		    return -1;

		for (; n; --n, col++)
		    b[ROWTOBOARD(row)][COLTOBOARD(col)].icon =
			pgn_int_to_piece(WHITE, OPEN_SQUARE);
	    } 
	    else if (pgn_piece_to_int(*tmp) != -1)
		b[ROWTOBOARD(row)][COLTOBOARD(col++)].icon = *tmp;
	    else
		return -1;

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
	    return -1;
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
	    default:
		return -1;
	}
    }

    // En passant.
    if (*++tmp != '-') {
	if (!VALIDCOL(*tmp))
	    return -1;

	col = *tmp++ - 'a';

	if (!VALIDROW(*tmp))
	    return -1;

	row = 8 - atoi(tmp++);
	b[row][col].enpassant = 1;
    }

    while (*tmp)
	tmp++;

    while (*tmp != ' ')
	tmp--;

    moven = atoi(tmp);
    return moven;
}

/*
 * This is called at the EOG marker and the beginning of the move text
 * section. So at least a move or EOG marker has to exist. It initializes the
 * board (b) to the FEN tag (if found) and sets the castling and enpassant
 * info for the game 'g'. If 'fen' is set it should be a fen tag and will be
 * parsed rather than the game 'g' FEN tag. Returns 0 on success or if there
 * was no FEN tag and 1 if there was a FEN parse error.
 */
int pgn_init_fen_board(GAME *g, BOARD b, char *fen)
{
    int n = -1, i = -1;
    BOARD tmpboard;
    unsigned flags = 0;
    char turn = (*g).turn;

    pgn_init_board(tmpboard);

    if (!fen) {
	n = pgn_find_tag((*g).tag, (*g).tindex, "Setup");
	i = pgn_find_tag((*g).tag, (*g).tindex, "FEN");
    }

    /*
     * If the FEN tag exists and there is no SetUp tag go ahead and parse it.
     * If there is a SetUp tag only parse the FEN tag if the value is 1.
     */
    if ((n >= 0 && i >= 0 && atoi((*g).tag[n].value) == 1) 
	    || (i >= 0 && n == -1) || fen) {
	if ((n = parse_fen_line(tmpboard, &flags, &turn,
			(fen) ? fen : (*g).tag[i].value)) == -1)
	    return 1;
	else {
	    // FIXME .ply
	    memcpy(b, tmpboard, sizeof(BOARD));
	    CLEAR_FLAG((*g).flags, GF_WK_CASTLE);
	    CLEAR_FLAG((*g).flags, GF_WQ_CASTLE);
	    CLEAR_FLAG((*g).flags, GF_BK_CASTLE);
	    CLEAR_FLAG((*g).flags, GF_BQ_CASTLE);
	    (*g).flags |= flags;
	    (*g).turn = turn;
	}
    }

    return 0;
}

/*
 * Allocates a new game and increments gindex (the current game) and gtotal
 * (the total number of games).
 */
void pgn_new_game(BOARD b)
{
    gindex = ++gtotal - 1;
    game = Realloc(game, gtotal * sizeof(GAME));
    memset(&game[gindex], 0, sizeof(GAME));
    game[gindex].history = Calloc(1, sizeof(HISTORY *));
    game[gindex].history[0] = NULL;
    game[gindex].hp = game[gindex].history;
    game[gindex].side = game[gindex].turn = WHITE;
    SET_FLAG(game[gindex].flags, GF_WK_CASTLE);
    SET_FLAG(game[gindex].flags, GF_WQ_CASTLE);
    SET_FLAG(game[gindex].flags, GF_BK_CASTLE);
    SET_FLAG(game[gindex].flags, GF_BQ_CASTLE);
    pgn_init_board(b);
    set_default_tags(&game[gindex]);
}

static int read_file(FILE *fp)
{
#ifdef DEBUG
    char buf[LINE_MAX] = {0}, *p = buf;
#endif
    int c = 0;
    int parse_error = 0;
    int ret = 0;

    while (1) {
	int nextchar = 0;
	int lastchar = c;

	if ((c = fgetc(fp)) == EOF) {
	    if (feof(fp))
		break;

	    if (ferror(fp)) {
		clearerr(fp);
		continue;
	    }
	}

	if (c == '\015')
	    continue;

	nextchar = fgetc(fp);
	ungetc(nextchar, fp);

	/*
	 * If there was a move text parsing error, keep reading until the end
	 * of the current game discarding the data.
	 */
	if (parse_error) {
	    ret = 1;
	    
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
	    nulltags = 1;
	    tag_section = 0;
	    continue;
	}

	// PGN: Application comment. The '%' must be on the first column of
	// the line. The comment continues until the end of the current line.
	if (c == '%') { 
	    if (lastchar == '\n' || lastchar == 0) {
		while ((c = fgetc(fp)) != EOF && c != '\n');
		continue;
	    }

	    // Not sure what to do here.
	}

	if (isspace(c))
	    continue;

	// PGN: Reserved.
	if (c == '<' || c == '>')
	    continue;

	// PGN: Recurrsive Annotation Variation. Read rav_text() for more
	// info.
	if (c == '(' || c == ')') {
	    switch (rav_text(&game[gindex], fp, c)) {
		case -1:
		    /*
		     * This is the end of the current RAV. This function has
		     * been called from rav_text(). Returning from this point
		     * will put us back in rav_text().
		     */
		    if (ravlevel > 0)
			return 0;

		    break;
		case 1:
		    parse_error = 1;
		    continue;
		default:
		    /*
		     * Continue processing. Probably the root move.
		     */
		    break;
	    }

	    continue;
	}

	// PGN: Numeric Annotation Glyph.
	if (c == '$' || c == '!' || c == '?' || c == '+' || c == '-' || 
		c == '~' || c == '=') {
	    ungetc(c, fp);
	    nag_text(&game[gindex], fp);
	    continue;
	}

	// PGN: Annotation. The ';' comment continues until the end of the
	// current line. The '{' type comment continues until a '}' is
	// reached.
	if (c == '{' || c == ';') {
	    annotation_text(&game[gindex], fp, (c == '{') ? '}' : '\n');
	    continue;
	}

	// PGN: Roster tag.
	if (c == '[') {
	    // First roster tag found. Initialize the data structures.
	    if (!tag_section) {
		nulltags = 0;
		tag_section = 1;

		if (gtotal && history_total(game[gindex].hp))
		    game[gindex].hindex = history_total(game[gindex].hp) - 1;

		pgn_new_game(pgnboard);
	    }

	    tag_text(&game[gindex], fp);
	    continue;
	}

	// PGN: End-of-game markers.
	if ((isdigit(c) && (nextchar == '-' || nextchar == '/')) || c == '*') {
	    ungetc(c, fp);
	    eog_text(&game[gindex], fp);
	    nulltags = 1;
	    tag_section = 0;

	    if (!done_fen_tag) {
		if (pgn_init_fen_board(&game[gindex], pgnboard, NULL)) {
		    parse_error = 1;
		    continue;
		}
	    }

	    done_fen_tag = 0;
	    continue;
	}

	// PGN: Move text.
	if (isdigit(c) || VALIDCOL(c) || c == 'N' || c == 'K' || c == 'Q' || 
		c == 'B' || c == 'R' || c == 'P' || c == 'O') {
	    ungetc(c, fp);

	    // PGN: If a FEN tag exists, initialize the board to the value.
	    if (tag_section) {
		if (pgn_init_fen_board(&game[gindex], pgnboard, NULL)) {
		    parse_error = 1;
		    continue;
		}

		done_fen_tag = 1;
		tag_section = 0;
	    }

	    // PGN: Import format doesn't require a roster tag section. We've
	    // arrived to the move text section without any tags so we
	    // initialize a new game which set's the default tags and any tags
	    // from the configuration file.
	    if (nulltags) {
		if (gtotal)
		    game[gindex].hindex = history_total(game[gindex].hp) - 1;

		pgn_new_game(pgnboard);
		nulltags = 0;
	    }

	    if (move_text(&game[gindex], fp)) {
		SET_FLAG(game[gindex].flags, GF_PERROR);
		parse_error = 1;
	    }

	    continue;
	}

#ifdef DEBUG
	*p++ = c;

	DUMP("unparsed: '%s'\n", buf);

	if (strlen(buf) + 1 == sizeof(buf))
	    bzero(buf, sizeof(buf));
#endif

	continue;
    }

    game[gindex].hp = game[gindex].history;
    return ret;
}

/*
 * Parses a PGN game file 'filename'. If 'filename' is NULL then a single
 * empty game will be allocated. If there is a parsing error 1 is returned
 * otherwise 0 is returned and the global 'gindex' is set to the last parsed
 * game in the file and the global 'gtotal' is set to the total number of
 * games in the file. For file access failures -1 is returned with errno set
 * to indicate the error.
 */
int pgn_parse_file(const char *filename)
{
    FILE *fp;
    int ret;

    if (!filename) {
	reset_game_data();
	pgn_new_game(pgnboard);
	return 0;
    }

    if (access(filename, R_OK) == -1)
	return -1;

    if ((fp = open_file(filename)) == NULL)
	return -1;

    reset_game_data();
    nulltags = 1;
    ret = read_file(fp);
    fclose(fp);

    if (gtotal < 1) {
	pgn_new_game(pgnboard);
	goto done;
    }

    pgn_sort_tags(game[gindex]);
    gtotal = gindex + 1;

done:
    pgn_switch_turn(&game[gindex].turn);
    return ret;
}

/*
 * Escape '"' and '\' in tag values.
 */
static char *pgn_add_tag_escapes(const char *str)
{
    int i, n;
    int len = strlen(str);
    static char buf[MAX_PGN_LINE_LEN] = {0};

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
    return buf;
}
	
/*
 * See pgn_write() for more info.
 */
static int write_comments_and_nag(FILE *fp, HISTORY *h, int *len)
{
    int i;
    int n;
    int x;
    int annotated = 0;

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (h->nag[i]) {
	    annotated = 1;

	    *len += integer_len(h->nag[i]) + 2;

	    if (*len + 1 >= 80) {
		fprintf(fp, "\n");
		*len = 0;
	    }

	    fprintf(fp, "$%i ", h->nag[i]);
	}
    }

    if (h->comment && h->comment[0]) {
	annotated = 1;

	fprintf(fp, "\n{");

	if ((n = strlen(h->comment) + 1) >= 80) {
	    for (i = 0, x = 0; i < (n - 1); i++, x++) {
		if (x + 1 >= 80) {
		    fprintf(fp, "\n");
		    x = 0;
		}

		if (fputc(h->comment[i], fp) == EOF)
		    warn("PGN Save");
	    }
	}
	else
	    fprintf(fp, "%s", h->comment);

	fprintf(fp, "}\n");
	*len = 0;
    }

    return annotated;
}

/*
 * Writes a PGN formatted game 'g' to the file pointed to by 'fp'. The
 * 'reduced' parameter is for writing a PGN reduced export formatted game.
 */
void pgn_write(FILE *fp, GAME g, int reduced)
{
    int i;
    int n, len = 0;
    int annotated = 0;
    int x = 0;
    int oldtotal = history_total(g.hp);
    int t = oldtotal;

    //FIXME
    /*
    if (!isfifo && g.hindex != g.htotal) {
	snprintf(buf, sizeof(buf), "%s (#%i)", GAME_SAVE_FROM_HISTORY_TITLE,
		idx + 1);
	i = message(buf, GAME_SAVE_FROM_HISTORY_PROMPT, "%s", 
			GAME_SAVE_FROM_HISTORY_TEXT);

	if (i == 'c')
	    g.htotal = g.hindex;
    }
    */

    pgn_sort_tags(g);

    for (i = 0; g.tag[i].name; i++) {
	struct tm tp;
	char tbuf[64 + 1]; //FIXME

	if (reduced && i == 7)
	    break;

	if (strcmp(g.tag[i].name, "Date") == 0) {
	    if (strptime(g.tag[i].value, TIME_FORMAT, &tp) != NULL) {
		len = strftime(tbuf, sizeof(tbuf), PGN_TIME_FORMAT, &tp) + 1;
		g.tag[i].value = Realloc(g.tag[i].value, len);
		strncpy(g.tag[i].value, tbuf, len);
	    }
	}
	else if (strcmp(g.tag[i].name, "Event") == 0) {
	    if (g.tag[i].value[0] == '\0') {
		g.tag[i].value = Realloc(g.tag[i].value, 2);
		g.tag[i].value[0] = '?';
		g.tag[i].value[1] = '\0';
	    }
	}
	else if (strcmp(g.tag[i].name, "Site") == 0) {
	    if (g.tag[i].value[0] == '\0') {
		g.tag[i].value = Realloc(g.tag[i].value, 2);
		g.tag[i].value[0] = '?';
		g.tag[i].value[1] = '\0';
	    }
	}
	else if (strcmp(g.tag[i].name, "Round") == 0) {
	    if (g.tag[i].value[0] == '\0') {
		g.tag[i].value = Realloc(g.tag[i].value, 2);
		g.tag[i].value[0] = '?';
		g.tag[i].value[1] = '\0';
	    }
	}
	else if (strcmp(g.tag[i].name, "Result") == 0) {
	    if (g.tag[i].value[0] == '\0') {
		g.tag[i].value = Realloc(g.tag[i].value, 2);
		g.tag[i].value[0] = '*';
		g.tag[i].value[1] = '\0';
	    }
	}
	else if (strcmp(g.tag[i].name, "Black") == 0) {
	    if (g.tag[i].value[0] == '\0') {
		g.tag[i].value = Realloc(g.tag[i].value, 2);
		g.tag[i].value[0] = '?';
		g.tag[i].value[1] = '\0';
	    }
	}
	else if (strcmp(g.tag[i].name, "White") == 0) {
	    if (g.tag[i].value[0] == '\0') {
		g.tag[i].value = Realloc(g.tag[i].value, 2);
		g.tag[i].value[0] = '?';
		g.tag[i].value[1] = '\0';
	    }
	}

	fprintf(fp, "[%s \"%s\"]\n", g.tag[i].name, 
		(g.tag[i].value && g.tag[i].value[0]) ? 
		pgn_add_tag_escapes(g.tag[i].value) : "");
    }

    fprintf(fp, "\n");

    /* Move text section. If it's dumping to the FIFO, dont dump comments and
     * NAG data.
     */
    // FIXME RAV
    for (i = len = 0, n = 1; i < t; i++) {
	int mlen = strlen(g.history[i]->move);

	if ((i % 2) == x) {
	    len += 2;

	    if (i == 0 && TEST_FLAG(g.flags, GF_BLACK_OPENING)) {
		len += 3;
		x = 1;
		fprintf(fp, "%u... ", n++);
	    }
	    else {
		if (i == 1 && x)
		    --n;

		fprintf(fp, "%u. ", n);
	    }
	}
	else {
	    if (annotated) {
		fprintf(fp, "%u... ", n++);
		annotated = 0;
	    }
	}

	if (!reduced)
	    annotated = write_comments_and_nag(fp, g.history[i], &len);

	if (!(i % 2) && !annotated)
	    n++;

	len += mlen + integer_len(n) + 1;

	if (len + 1 >= 80) {
	    fprintf(fp, "\n");
	    len = 0;
	}
    }

    if (strlen(g.tag[TAG_RESULT].value) + len + 1 >= 80)
	fprintf(fp, "\n");

    fprintf(fp, "%s\n\n", pgn_add_tag_escapes(g.tag[TAG_RESULT].value));

    if (!reduced) {
	CLEAR_FLAG(g.flags, GF_MODIFIED);
	CLEAR_FLAG(g.flags, GF_PERROR);
	//*gp = g;
    }
}
