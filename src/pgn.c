/* $Id: pgn.c,v 1.84 2003-02-04 18:27:46 bjk Exp $ */
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
#include <dirent.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MENU_H
#include <menu.h>
#endif

#include "common.h"
#include "colors.h"
#include "pgn.h"

static int find_tag(GAME g, const char *name)
{
    int t;

    for (t = 0; t < g.tindex; t++) {
	if (strcasecmp(g.tag[t].name, name) == 0)
	    return t;
    }

    return -1;
}

static int tag_compare(const void *s1, const void *s2)
{
    const TAG *ss1 = s1;
    const TAG *ss2 = s2;

    return strcmp(ss1->name, ss2->name);
}

static void sort_tags(GAME g)
{
    TAG *t = g.tag + 7;

    qsort(t, g.tindex - 7, sizeof(TAG), tag_compare);
    return;
}

int end_of_game(const char *str)
{
    int i;
    int len;

    for (i = 0; i < NARRAY(fancy_results); i++) {
	if (strstr(str, fancy_results[i].pgn) != NULL) {
	    len = strlen(fancy_results[i].pgn) + 1;
	    game[gindex].tag[TAG_RESULT].value = 
		Realloc(game[gindex].tag[TAG_RESULT].value, len);

	    strncpy(game[gindex].tag[TAG_RESULT].value, fancy_results[i].pgn,
		    len);
	    return 1;
	}
    }

    return 0;
}

/* Returns 1 if a duplicate tag was found. 0 otherwise. The index argument is
 * a pointer to int, and incremented automatically.
 */
int add_tag(TAG **dst, int *n, char *name, char *value)
{
    int i, index = *n;
    TAG *tdata = *dst;
    int len = 0;

    name = trim(name);
    value = trim(value);

    /* If a duplicate was found, update the existing one to the new value. */
    for (i = 0; i < index; i++) {
	if (strcasecmp(tdata[i].name, name) == 0) {
	    len = (value) ? strlen(value) + 1 : 1;
	    tdata[i].value = Realloc(tdata[i].value, len);
	    strncpy(tdata[i].value, (value) ? value : "", len);
	    *dst = tdata;
	    return 1;
	}
    }

    tdata = Realloc(tdata, (index + 2) * sizeof(TAG));

    len = strlen(name) + 1;
    tdata[index].name = Malloc(len);
    strncpy(tdata[index].name, name, len);

    if (value) {
	len = strlen(value) + 1;
	tdata[index].value = Malloc(len);
	strncpy(tdata[index].value, value, len);
    }

    memset(&tdata[++index], '\0', sizeof(TAG));
    *n = index;
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

void init_board(BOARD b)
{
    int row, col;

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
	    b[row][col].valid = b[row][col].movecount = 0;
	}
    }

    return;
}

void set_default_tags()
{
    time_t now;
    char tbuf[MAX_TIME_LEN + 1] = {0};
    struct tm *tp;
    int n;

    time(&now);
    tp = localtime(&now);
    strftime(tbuf, sizeof(tbuf), PGN_TIME_FORMAT, tp);

    /* The standard seven tag roster (in order of appearance). */
    add_tag(&game[gindex].tag, &game[gindex].tindex, "Event", "?");
    add_tag(&game[gindex].tag, &game[gindex].tindex, "Site", "?");
    add_tag(&game[gindex].tag, &game[gindex].tindex, "Date", tbuf);
    add_tag(&game[gindex].tag, &game[gindex].tindex, "Round", "-");
    add_tag(&game[gindex].tag, &game[gindex].tindex, "White", 
	    config.pwd->pw_gecos);
    add_tag(&game[gindex].tag, &game[gindex].tindex, "Black", "?");
    add_tag(&game[gindex].tag, &game[gindex].tindex, "Result", "*");

    /* Add custom tags from the configuration file. */
    if (newgameinit) {
	for (n = 0; n < config.tindex; n++)
	    add_tag(&game[gindex].tag, &game[gindex].tindex, 
		    config.tag[n].name, config.tag[n].value);

	sort_tags(game[gindex]);
	newgameinit = 0;
    }

    return;
}

static void reset_game_data()
{
    if (gtotal)
	free_game_data();

    gtotal = gindex = 0;
    status.side = status.turn = WHITE;
    return;
}

static void skip_leading_space(FILE *fp)
{
    int c;

    while ((c = fgetc(fp)) != EOF && !feof(fp)) {
	if (!isspace(c))
	    break;
    }

    ungetc(c, fp);
    return;
}

static void invalid_move(const char *move)
{
    if (curses_initialized)
	cmessage(NULL, ANYKEY, "%s \"%s\" (#%i)", E_INVALID_MOVE, move,
		gindex + 1);
    else
	warnx("%s: %s \"%s\" (#%i)", loadfile, E_INVALID_MOVE, move,
		gindex + 1);

    return;
}

int move_text(FILE *fp)
{
    char move[MAX_PGN_MOVE_LEN + 1] = {0}, *p;
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
	    status.turn = BLACK;

	    if (game[gindex].hindex == 0)
		game[gindex].openingside = BLACK;
	}
	else {
	    status.turn = WHITE;

	    if (game[gindex].hindex == 0)
		game[gindex].openingside = WHITE;
	}
    }
    else {
	if (game[gindex].hindex > 0)
	    switch_turn();
    }

    ungetc(c, fp);

    if (fscanf(fp, " %[a-hPRNBQK1-9#+=Ox-]%n", move, &count) != 1)
	return 1;

    if ((p = a2a4tosan(pgnboard, move)) == NULL) {
	invalid_move(move);
	return 1;
    }

    if (parse_move_text(pgnboard, p)) {
	if (game[gindex].hindex == 0) {
	    switch_turn();

	    if (parse_move_text(pgnboard, p)) {
		switch_turn();
		invalid_move(move);
		return 1;
	    }

	    game[gindex].openingside = BLACK;
	}
	else {
	    invalid_move(move);
	    return 1;
	}
    }

    add_to_history(&game[gindex].history, &game[gindex].hindex,
	    &game[gindex].htotal, p);

    /*
    printf("%s\n", p);
    dump_board(pgnboard, 0);
    */

    return 0;
}

static void nag_text(FILE *fp)
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
	if (game[gindex].history[game[gindex].hindex - 1].nag[i])
	    continue;

	game[gindex].history[game[gindex].hindex - 1].nag[i] = nag;
	break;
    }

    skip_leading_space(fp);
    return;
}

static void move_annotation(FILE *fp, int terminator)
{
    int c, lastchar = 0;
    int len = 0;
    int hindex = game[gindex].hindex - 1;
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

    game[gindex].history[hindex].comment = 
	Realloc(game[gindex].history[hindex].comment, ++len);

    strncpy(game[gindex].history[hindex].comment, buf, len);

    return;
}

static void pgn_tag(FILE *fp)
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
    add_tag(&game[gindex].tag, &game[gindex].tindex, name, value);

    skip_leading_space(fp);
    return;
}

static int eog_marker(FILE *fp)
{
    int c, i = 0;
    char buf[8], *p = buf;

    while ((c = fgetc(fp)) != EOF && !isspace(c) && i++ < sizeof(buf))
	*p++ = c;

    for (i = 0; i < NARRAY(fancy_results); i++) {
	int len;

	if (strcmp(buf, fancy_results[i].pgn) == 0) {
	    len = strlen(fancy_results[i].pgn) + 1;
	    game[gindex].tag[TAG_RESULT].value = 
		Realloc(game[gindex].tag[TAG_RESULT].value, len);
	    strncpy(game[gindex].tag[TAG_RESULT].value, fancy_results[i].pgn,
		    len);
	    break;
	}
    }

    skip_leading_space(fp);
    return 1;
}

#ifdef DEBUG
void dump_board(BOARD b, int which)
{
    int row, col;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++) {
	    if (b[row][col].icon == '.') {
		DEBUG_BOARD(which, ". ");
		continue;
	    }

	    DEBUG_BOARD(which, "%c ", (int)b[row][col].icon);
	}

	DEBUG_BOARD(which, "\n");
    }

    DEBUG_BOARD(which, "\n");
    return;
}
#endif

void new_game(BOARD b)
{
    static int firstrun;

    status.side = status.turn = WHITE;

    if (gtotal == 0)
	firstrun = 1;
    else
	gindex = gtotal - 1;

    if (!firstrun) {
	game = Realloc(game, (gindex + 2) * sizeof(GAME));
	memset(&game[gindex + 1], '\0', sizeof(GAME));
	memset(&game[gindex + 1].tag, '\0', sizeof(TAG));
	memset(&game[gindex + 1].history, '\0', sizeof(HISTORY));
	sort_tags(game[gindex]);
	gindex++;
    }
    else {
	game = Calloc(1, sizeof(GAME));
	firstrun = 0;
    }

    gtotal = gindex + 1;
    set_default_tags();
    init_board(b);

    return;
}

/* Skip RAV text section for now as it's unsupported. */
static void rav_text(FILE *fp, int which)
{
    int c;

    while ((c = fgetc(fp)) != EOF) {
	if (c == '(')
	    rav_text(fp, c);
	else if (c == ')')
	    break;
    }

    return;
}

int parse_pgn_file(BOARD b, const char *filename)
{
    FILE *fp;
    char buf[LINE_MAX] = {0}, *p = buf;
    int compressed = 0;
    int c;
    int tag_section = 0;
    int parse_error = 0;
    int nulltags = 1;
    int n, i;
    BOARD tmpboard;

    if (!*filename) {
	reset_game_data();
	newgameinit = 1;
	new_game(b);
	return 0;
    }

    if (access(filename, R_OK) == -1) {
	if (curses_initialized)
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	else
	    warn("%s", filename);

	return 1;
    }

    if ((fp = open_file(filename, &compressed)) == NULL)
	return 1;

    reset_game_data();

    while (1) {
	int nextchar = 0;

	if ((c = fgetc(fp)) == EOF) {
	    if (feof(fp))
		break;

	    if (ferror(fp)) {
		if (curses_initialized)
		    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
		else
		    warnx("%s: %s", filename, strerror(errno));

		clearerr(fp);
		continue;
	    }
	}

	nextchar = fgetc(fp);
	ungetc(nextchar, fp);

	/* If there was a move text parsing error, keep reading until the end
	 * of the current game discarding the data.
	 */
	if (parse_error) {
	    if ((c == '\n' && nextchar == '\n')) {
		parse_error = 0;
		nulltags = 1;
		tag_section = 0;
	    }
	    else
		continue;
	}

	if ((c == '\n' && nextchar == '\n')) {
	    nulltags = 1;
	    tag_section = 0;
	    continue;
	}

	if (c == '%') {
	    while ((c = fgetc(fp)) != EOF && c != '\n');
	    continue;
	}

	if (isspace(c))
	    continue;

	if (c == '<' || c == '>')
	    continue;

	if (c == '(' || c == ')') {
	    rav_text(fp, c);
	    continue;
	}

	if (c == '$' || c == '!' || c == '?' || c == '+' || c == '-' || 
		c == '~' || c == '=') {
	    ungetc(c, fp);
	    nag_text(fp);
	    continue;
	}

	if (c == '{' || c == ';') {
	    move_annotation(fp, (c == '{') ? '}' : '\n');
	    continue;
	}

	if (c == '[') {
	    if (!tag_section) {
		nulltags = 0;
		tag_section = 1;
		new_game(pgnboard);
	    }

	    pgn_tag(fp);
	    continue;
	}

	/* EOG markers. */
	if ((isdigit(c) && (nextchar == '-' || nextchar == '/')) || c == '*') {
	    ungetc(c, fp);
	    eog_marker(fp);
	    nulltags = 1;
	    continue;
	}

	if (isdigit(c) || VALIDCOL(c) || c == 'N' || c == 'K' || c == 'Q' || 
		c == 'B' || c == 'R' || c == 'P' || c == 'O') {
	    ungetc(c, fp);

	    tag_section = 0;

	    if (nulltags) {
		new_game(pgnboard);
		nulltags = 0;
	    }

	    if (move_text(fp))
		parse_error = 1;

	    continue;
	}

	*p++ = c;

#ifdef DEBUG
	DUMP("unparsed: '%s'\n", buf);
#endif

	if (strlen(buf) + 1 == sizeof(buf))
	    bzero(buf, sizeof(buf));

	continue;
    }

    fclose(fp);

    if (gtotal < 1) {
	new_game(b);
	goto done;
    }

    sort_tags(game[gindex]);
    gtotal = gindex + 1;

    n = find_tag(game[gindex], "Setup");
    i = find_tag(game[gindex], "FEN");

    if ((n > -1 && i > -1 && atoi(game[gindex].tag[n].value) == 1) 
	    || (i != -1 && n == -1)) {
	if ((n = parse_fen_line(tmpboard, game[gindex].tag[i].value)) == -1)
	    cmessage(ERROR, ANYKEY, "%s", E_FEN_PARSE); 
	else {
	    copy_board(tmpboard, pgnboard);
	    game[gindex].fentag = i;
	    game[gindex].hindex = (n * 2 > game[gindex].htotal - 1) ?
		game[gindex].hindex : n * 2;
	}
    }

    copy_board(pgnboard, b);

done:
    if (compressed)
	unlink(config.tmpfile);

    //exit(0);
    return 0;
}

static char *add_tag_escapes(const char *str)
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
	
static int dump_comments_and_nag(FILE *fp, HISTORY h, int *len)
{
    int i;
    int n;
    int x;
    int annotated = 0;

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (h.nag[i]) {
	    annotated = 1;

	    *len += integer_len(h.nag[i]) + 2;

	    if (*len + 1 >= 80) {
		fprintf(fp, "\n");
		*len = 0;
	    }

	    fprintf(fp, "$%i ", h.nag[i]);
	}
    }

    if (h.comment && h.comment[0]) {
	annotated = 1;

	fprintf(fp, "\n{");

	if ((n = strlen(h.comment) + 1) >= 80) {
	    for (i = 0, x = 0; i < (n - 1); i++, x++) {
		if (x + 1 >= 80) {
		    fprintf(fp, "\n");
		    x = 0;
		}

		if (fputc(h.comment[i], fp) == EOF)
		    warn("PGN Save");
	    }
	}
	else
	    fprintf(fp, "%s", h.comment);

	fprintf(fp, "}\n");
	*len = 0;
    }

    return annotated;
}

static void dumpgame(FILE *fp, GAME g, int index, int isfifo)
{
    int i;
    int n, len = 0;
    int annotated = 0;
    int x = 0;
    char buf[80];
    int oldtotal = g.htotal;

    if (!isfifo && g.hindex != g.htotal) {
	snprintf(buf, sizeof(buf), "%s (#%i)", GAME_SAVE_FROM_HISTORY_TITLE,
		index + 1);
	i = message(buf, GAME_SAVE_FROM_HISTORY_PROMPT, "%s", 
			GAME_SAVE_FROM_HISTORY_TEXT);

	if (i == 'c')
	    g.htotal = g.hindex;
    }

    sort_tags(g);

    for (i = 0; g.tag[i].name; i++) {
	struct tm tp;
	char buf[MAX_TIME_LEN + 1];

	if (isfifo && i == 7)
	    break;

	if (strcmp(g.tag[i].name, "Date") == 0) {
	    if (strptime(g.tag[i].value, TIME_FORMAT, &tp) != NULL) {
		len = strftime(buf, sizeof(buf), PGN_TIME_FORMAT, &tp) + 1;
		g.tag[i].value = Realloc(g.tag[i].value, len);
		strncpy(g.tag[i].value, buf, len);
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
	    else {
		for (n = 0; n < NARRAY(fancy_results); n++) {
		    if (strcmp(g.tag[i].value, fancy_results[n].pgn) == 0) {
			n = -1;
			break;
		    }
		}

		if (n != -1) {
		    g.tag[i].value = Realloc(g.tag[i].value, 2);
		    g.tag[i].value[0] = '*';
		    g.tag[i].value[1] = '\0';
		}
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
	else if (strcmp(g.tag[i].value, UNKNOWN) == 0) {
	    g.tag[i].value = Realloc(g.tag[i].value, 1);
	    g.tag[i].value[0] = '\0';
	}

	fprintf(fp, "[%s \"%s\"]\n", g.tag[i].name, 
		(g.tag[i].value && g.tag[i].value[0]) ? 
		add_tag_escapes(g.tag[i].value) : "");
    }

    fprintf(fp, "\n");

    /* Move text section. If it's dumping to the FIFO, dont dump comments and
     * NAG data.
     */
    for (i = len = 0, n = 1; i < g.htotal; i++) {
	int mlen = strlen(g.history[i].move);

	if ((i % 2) == x) {
	    len += 2;

	    if (i == 0 && g.openingside == BLACK) {
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

	fprintf(fp, "%s ", g.history[i].move);

	if (!isfifo)
	    annotated = dump_comments_and_nag(fp, g.history[i], &len);

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

    fprintf(fp, "%s\n\n", add_tag_escapes(g.tag[TAG_RESULT].value));
    g.htotal = oldtotal;
    return;
}

/* If the saveindex argument is -1, all games will be saved. Otherwise it's a
 * game index number.
 */
int save_pgn(const char *filename, int isfifo, int saveindex)
{
    FILE *fp;
    char *mode = NULL;
    int c;
    char buf[FILENAME_MAX];
    struct stat st;
    int i;
    char *command = NULL;
    int saveindex_max = (saveindex == -1) ? gtotal : saveindex + 1;

    if (filename[0] != '/' && config.savedirectory && !isfifo) {
	if (stat(config.savedirectory, &st) == -1) {
	    if (errno == ENOENT) {
		if (mkdir(config.savedirectory, 0755) == -1) {
		    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			    strerror(errno));
		    return 1;
		}
	    }
	    else {
		cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			strerror(errno));
		return 1;
	    }
	}

	stat(config.savedirectory, &st);

	if (!S_ISDIR(st.st_mode)) {
	    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory, E_NOTADIR);
	    return 1;
	}

	snprintf(buf, sizeof(buf), "%s/%s", config.savedirectory, filename);
	filename = buf;
    }

    if (!isfifo)
	command = compression_cmd(filename, 0);

    /* This is a hack to resume an existing game when more than one game is
     * available. Also resuming a saved game and a game from history.
     */
    if (isfifo)
	mode = "w";
    else {
	if (access(filename, W_OK) == 0) {
	    c = cmessage(NULL, GAME_SAVE_OVERWRITE_PROMPT,
		    "%s \"%s\"", E_FILEEXISTS, filename);

	    switch (c) {
		case 'a':
		    if (command) {
			cmessage(NULL, ANYKEY, "%s", E_SAVE_COMPRESS);
			return 1;
		    }

		    mode = "a";
		    break;
		case 'o':
		    mode = "w+";
		    break;
		default:
		    return 1;
	    }
	}
	else
	    mode = "a";
    }

    if (command) {
	if ((fp = popen(command, "w")) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	    return 1;
	}
    }
    else {
	if ((fp = fopen(filename, mode)) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	    return 1;
	}
    }

    if (isfifo)
	dumpgame(fp, game[saveindex], saveindex, isfifo);
    else {
	for (i = (saveindex == -1) ? 0 : saveindex; i < saveindex_max; i++)
	    dumpgame(fp, game[i], i, isfifo);
    }

    if (command)
	pclose(fp);
    else
	fclose(fp);

    if (!isfifo && saveindex == -1)
	strncpy(loadfile, filename, sizeof(loadfile));

    return 0;
}

static void cleanup(WINDOW *win, WINDOW *subw, PANEL *panel, MENU *menu, 
	ITEM **items, struct d_entries *entries)
{
    int i;

    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; items[i]; i++)
	free_item(items[i]);

    free(items);

    if (entries) {
	for (i = 0; entries[i].name; i++) {
	    free(entries[i].name);
	    free(entries[i].fancy);
	}

	free(entries);
    }

    del_panel(panel);
    delwin(subw);
    delwin(win);
}

static int init_country_codes()
{
    FILE *fp;
    char line[LINE_MAX], *s;
    int cindex = 0;

    if ((fp = fopen(config.ccfile, "r")) == NULL) {
	cmessage(ERROR, ANYKEY, "%s: %s", config.ccfile, strerror(errno));
	return 1;
    }

    while ((s = fgets(line, sizeof(line), fp)) != NULL) {
	char *tmp;

	if ((tmp = strsep(&s, " ")) == NULL)
	    continue;

	s = trim(s);
	tmp = trim(tmp);

	if (!s || !tmp)
	    continue;

	ccodes = Realloc(ccodes, (cindex + 2) * sizeof(struct country_codes));
	strncpy(ccodes[cindex].code, tmp, sizeof(ccodes[cindex].code));
	strncpy(ccodes[cindex].country, s, sizeof(ccodes[cindex].country));
	cindex++;
    }

    memset(&ccodes[cindex], '\0', sizeof(struct country_codes));
    fclose(fp);

    return 0;
}

char *country_codes(void *arg)
{
    WINDOW *win, *subw;
    PANEL *panel;
    ITEM **mitems = NULL;
    MENU *menu;
    int i = 0, n;
    int rows, cols;
    char *mbuf = NULL;
    char *tmp = NULL;

    if (!ccodes) {
	if (init_country_codes())
	    return NULL;
    }

    for (n = i = 0; ccodes[n].code[0]; n++, i++) {
	mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	mitems[i] = new_item(ccodes[n].country, ccodes[n].code);
    }

    mitems[i] = NULL;
    menu = new_menu(mitems);
    scale_menu(menu, &rows, &cols);

    if (cols < strlen(HELP_PROMPT) + 21)
	cols = strlen(HELP_PROMPT) + 21;

    win = newwin(rows + 4, cols + 4, CALCPOSY(rows) - 2, CALCPOSX(cols));
    set_menu_win(menu, win);
    subw = derwin(win, rows, cols + 2, 2, 1);
    set_menu_sub(menu, subw);
    set_menu_fore(menu, A_REVERSE);
    set_menu_grey(menu, A_NORMAL);
    set_menu_mark(menu, NULL);
    set_menu_spacing(menu, 0, 0, 0);
    menu_opts_off(menu, O_NONCYCLIC);
    post_menu(menu);
    panel = new_panel(win);
    cbreak();
    noecho();
    keypad(win, TRUE);
    set_menu_pattern(menu, mbuf);
    wbkgd(win, CP_MESSAGE_WINDOW);
    draw_window_title(win, CC_TITLE, cols + 4, CP_MESSAGE_TITLE,
	    CP_MESSAGE_BORDER);

    while (1) {
	int c;
	char buf[cols - 4];

	wattron(win, A_REVERSE);

	for (c = 1; c < (cols + 2) - 1; c++)
	    mvwprintw(win, rows + 2, c, " ");

	c = item_index(current_item(menu)) + 1;

	snprintf(buf, sizeof(buf), "%s %i %s %i %s", MENU_ITEM_STR, c, 
		N_OF_N_STR, item_count(menu), HELP_PROMPT);
	draw_prompt(win, rows + 2, cols + 2, buf, CP_MESSAGE_PROMPT);

	wattroff(win, A_REVERSE);

	/* This nl() statement needs to be here because NL is recognized
	 * for some reason after the first selection.
	 */
	nl();
	update_panels();
	doupdate();

	c = wgetch(win);

	switch (c) {
	    case CTRL('G'):
		help(CC_KEY_HELP, cc_help);
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
	    case ' ':
	    case KEY_NPAGE:
	    case CTRL('N'):
		if (menu_driver(menu, REQ_SCR_DPAGE) == E_REQUEST_DENIED)
		    menu_driver(menu, REQ_LAST_ITEM);
		break;
	    case '\n':
		tmp = (char *)item_description(current_item(menu));
		goto done;
		break;
	    case KEY_ESCAPE:
		tmp = NULL;
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

done:
    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; mitems[i]; i++)
	free_item(mitems[i]);

    del_panel(panel);
    delwin(subw);
    delwin(win);
    return tmp;
}

void free_tag_data(TAG *data, int index)
{
    int i;

    for (i = 0; i < index; i++) {
	free(data[i].name);
	free(data[i].value);
    }

    return;
}

TAG *edit_tags(BOARD b, TAG *old, int maxtags, int edit)
{
    TAG *data = NULL;
    struct tm tp;
    int data_index = 0;
    int i, lastindex = 0;
    int len;

    /* Edit the backup copy, not the original in case the save fails. */
    for (i = 0; i < maxtags; i++)
	add_tag(&data, &data_index, old[i].name, old[i].value);

    while (1) {
	WINDOW *win, *subw;
	PANEL *panel;
	ITEM **mitems = NULL;
	MENU *menu;
	int i;
	char buf[76] = {0};
	char *tmp = NULL;
	int rows, cols;
	int selected = -1;
	char *mbuf = NULL;
	int nlen = 0, vlen = 0;

	for (i = 0; i < data_index; i++) {
	    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));

	    if (data[i].value[0]) {
		nlen = strlen(data[i].name);
		vlen = strlen(data[i].value);

		/* The +6 is for the menu padding. */
		mitems[i] = new_item(data[i].name,
			(nlen + vlen + 6 >= MAX_VALUE_WIDTH)
			? PRESS_ENTER : data[i].value);
	    }
	    else
		mitems[i] = new_item(data[i].name, UNKNOWN);
	}

	mitems[i] = NULL;
	menu = new_menu(mitems);
	scale_menu(menu, &rows, &cols);

	/* +14 for the extra prompt info. */
	if (cols < strlen(HELP_PROMPT) + 14)
	    cols = strlen(HELP_PROMPT) + 14;

	win = newwin(rows + 4, cols + 4, CALCPOSY(rows) - 2, CALCPOSX(cols));
	set_menu_win(menu, win);
	subw = derwin(win, rows, cols + 2, 2, 1);
	set_menu_sub(menu, subw);
	set_menu_fore(menu, A_REVERSE);
	set_menu_grey(menu, A_NORMAL);
	set_menu_mark(menu, NULL);
	set_menu_pad(menu, '-');
	set_menu_spacing(menu, 3, 0, 0);
	menu_opts_off(menu, O_NONCYCLIC);
	post_menu(menu);
	panel = new_panel(win);
	cbreak();
	noecho();
	nl();
	keypad(win, TRUE);
	set_menu_pattern(menu, mbuf);
	wbkgd(win, CP_MESSAGE_WINDOW);
	draw_window_title(win, (edit) ? TAG_EDIT_TITLE : TAG_VIEW_TITLE, 
		cols + 4, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);

	while (1) {
	    int c;
	    TAG *tmppgn = NULL;
	    char *newtag = NULL;
	    int tpgn_index = 0;
	    char *tmp;

	    if (set_current_item(menu, mitems[lastindex]) != E_OK) {
		lastindex = item_count(menu) - 1;
		continue;
	    }

	    snprintf(buf, sizeof(buf), "%s %i %s %i  %s", MENU_TAG_STR,
		    item_index(current_item(menu)) + 1, N_OF_N_STR,
		    item_count(menu), HELP_PROMPT);
	    draw_prompt(win, rows + 2, cols + 4, buf, CP_MESSAGE_PROMPT);

	    update_panels();
	    doupdate();

	    c = wgetch(win);

	    switch (c) {
		case CTRL('G'):
		    if (edit)
			help(TAG_EDIT_HELP, pgn_edit_help);
		    else
			help(TAG_VIEW_HELP, pgn_info_help);
		    break;
		case CTRL('R'):
		    if (!edit)
			break;

		    selected = item_index(current_item(menu));

		    if (selected <= 6) {
			cmessage(NULL, ANYKEY, "%s", E_REMOVE_STR);
			goto cleanup;
		    }

		    for (i = 0; i < data_index; i++) {
			if (i == selected)
			    continue;

			add_tag(&tmppgn, &tpgn_index, data[i].name,
				data[i].value);
		    }

		    free_tag_data(data, data_index);

		    for (i = data_index = 0; i < tpgn_index; i++) {
			add_tag(&data, &data_index, tmppgn[i].name,
				tmppgn[i].value);
		    }

		    free_tag_data(tmppgn, tpgn_index);
		    free(tmppgn);
		    goto cleanup;
		    break;
		case CTRL('A'):
		    if (!edit)
			break;

		    if ((newtag = get_input(TAG_NEW_TITLE, NULL, 1, 1, NULL,
				    NULL, NULL, 0, FIELD_TYPE_PGN_TAG_NAME))
			    == NULL)
			break;

		    newtag[0] = toupper(newtag[0]);

		    if (strlen(newtag) > MAX_VALUE_WIDTH - 6 - 
			    strlen(PRESS_ENTER)) {
			cmessage(ERROR, ANYKEY, "%s", E_TAG_NAMETOOLONG);
			break;
		    }

		    for (i = 0; i < data_index; i++) {
			if (strcasecmp(data[i].name, newtag) == 0) {
			    selected = i;
			    goto gotitem;
			}
		    }

		    add_tag(&data, &data_index, newtag, NULL);

		    selected = data_index - 1;
		    goto gotitem;
		    break;
		case KEY_HOME:
		    menu_driver(menu, REQ_FIRST_ITEM);
		    break;
		case KEY_END:
		    menu_driver(menu, REQ_LAST_ITEM);
		    break;
		case CTRL('F'):
		    if (!edit)
			break;

		    add_tag(&data, &data_index, "FEN",
			    board_to_fen(b, game[gindex]));

		    selected = data_index - 1;
		    goto gotitem;
		    break;
		case KEY_NPAGE:
		case CTRL('N'):
		    if (menu_driver(menu, REQ_SCR_DPAGE) == E_REQUEST_DENIED)
			menu_driver(menu, REQ_LAST_ITEM);
		    break;
		case KEY_PPAGE:
		case CTRL('P'):
		    if (menu_driver(menu, REQ_SCR_UPAGE) == E_REQUEST_DENIED)
			menu_driver(menu, REQ_FIRST_ITEM);
		    break;
		case KEY_UP:
		    menu_driver(menu, REQ_UP_ITEM);
		    break;
		case KEY_DOWN:
		    menu_driver(menu, REQ_DOWN_ITEM);
		    break;
		case '\n':
		    selected = item_index(current_item(menu));
		    goto gotitem;
		    break;
		case KEY_ESCAPE:
		    cleanup(win, subw, panel, menu, mitems, NULL);
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

	    lastindex = item_index(current_item(menu));
	}

gotitem:
	lastindex = selected;
	nlen = strlen(data[selected].name) + 3;
	nlen += (edit) ? strlen(TAG_EDIT_TAG_TITLE) : strlen(TAG_VIEW_TAG_TITLE);

	if (nlen > MAX_VALUE_WIDTH)
	    snprintf(buf, sizeof(buf), "%s", data[selected].name);
	else
	    snprintf(buf, sizeof(buf), "%s \"%s\"",
		    (edit) ? TAG_EDIT_TAG_TITLE : TAG_VIEW_TAG_TITLE,
		    data[selected].name);

	if (!edit) {
	    if (strcmp(item_description(mitems[selected]), UNKNOWN) == 0)
		goto cleanup;

	    cmessage(buf, ANYKEY, "%s", data[selected].value);
	    goto cleanup;
	}

	if (strcmp(data[selected].name, "Date") == 0) {
	    tmp = get_input(buf, data[selected].value, 0, 0, NULL, NULL, NULL,
		    0, FIELD_TYPE_PGN_DATE);

	    if (tmp) {
		if (strptime(tmp, PGN_TIME_FORMAT, &tp) == NULL) {
		    cmessage(ERROR, ANYKEY, "%s", E_TAG_DATE_FMT);
		    goto cleanup;
		}
	    }
	    else
		goto cleanup;
	}
	else if (strcmp(data[selected].name, "Site") == 0) {
	    tmp = get_input(buf, data[selected].value, 1, 1, CC_PROMPT,
		    country_codes, NULL, CTRL('t'), -1);

	    if (!tmp)
		tmp = "?";
	}
	else if (strcmp(data[selected].name, "Round") == 0) {
	    tmp = get_input(buf, NULL, 1, 1, NULL, NULL, NULL, 0,
		    FIELD_TYPE_PGN_ROUND);

	    if (!tmp) {
		if (gtotal > 1)
		    tmp = "?";
		else
		    tmp = "-";
	    }
	}
	else if (strcmp(data[selected].name, "Result") == 0) {
	    tmp = get_input(buf, data[selected].value, 1, 1, NULL, NULL, NULL, 
		    0, -1);

	    if (!tmp)
		tmp = "*";

	    for (i = 0; i < NARRAY(fancy_results); i++) {
		if (strcmp(tmp, fancy_results[i].pgn) == 0) {
		    i = -1;
		    break;
		}
	    }

	    if (i != -1)
		tmp = "*";
	}
	else {
	    if (item_description(mitems[selected]) && 
		    strcmp(item_description(mitems[selected]), UNKNOWN) == 0)
		tmp = NULL;
	    else
		tmp = data[selected].value;

	    tmp = get_input(buf, tmp, 0, 0, NULL, NULL, NULL, 0, -1);
	}

	len = (tmp) ? strlen(tmp) + 1 : 1;
	data[selected].value = Realloc(data[selected].value, len);
	strncpy(data[selected].value, (tmp) ? tmp : "", len);

cleanup:
	cleanup(win, subw, panel, menu, mitems, NULL);
    }

done:
    if (!edit) {
	free_tag_data(data, data_index);
	free(data);
	return NULL;
    }

    return data;
}

static int sort_entries(const void *s1, const void *s2)
{
    const struct d_entries *ss1 = s1;
    const struct d_entries *ss2 = s2;

    return strcmp(ss1->name, ss2->name);
}

static struct d_entries *get_directory_entries(const char *path)
{
    DIR *dp;
    struct dirent *entry;
    struct d_entries *entries = NULL;
    int index = 0;

    if ((dp = opendir(path)) == NULL)
	return NULL;

    while ((entry = readdir(dp)) != NULL) {
	struct stat st;
	int len;
	char tbuf[MAX_TIME_LEN + 1] = {0};
	struct tm *tp;
	char buf[FILENAME_MAX];
	char *tmp;
	size_t n;

	if (entry->d_name[0] == '.' && entry->d_name[1] == 0)
	    continue;

	snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);

	if (stat(buf, &st) == -1)
	    continue;

	n = st.st_size ;
	entries = Realloc(entries, (index + 2) * sizeof(struct d_entries));
	entries[index].name = strdup(buf);
	tmp = real_filename(buf);
	len = strlen(tmp) + 2;
	entries[index].fancy = Malloc(len);
	strncpy(entries[index].fancy, tmp, len);

	if (S_ISDIR(st.st_mode))
	    entries[index].fancy[len - 2] = '/';

	tp = localtime(&st.st_mtime);
	strftime(tbuf, sizeof(tbuf), "%b %d %T", tp);

	snprintf(entries[index].desc, sizeof(entries[index].desc), "%-7i %s", 
		n, tbuf);

	memset(&entries[++index], '\0', sizeof(struct d_entries));
    }

    closedir(dp);
    qsort(entries, index, sizeof(struct d_entries), sort_entries);
    return entries;
}

char *browse_directory(void *arg)
{
    int i;
    char path[FILENAME_MAX] = {0};
    static char file[FILENAME_MAX];
    char *oldwd = getcwd(NULL, 0);
    DIR *dp;
    char *inputstr = (char *)arg;
    int initkey = inputstr[0];

    if (config.savedirectory) {
	if ((dp = opendir(config.savedirectory)) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
		    strerror(errno));
	    getcwd(path, sizeof(path));
	}
	else {
	    closedir(dp);
	    strncpy(path, config.savedirectory, sizeof(path));
	}
    }
    else
	getcwd(path, sizeof(path));

again:
    while (1) {
	WINDOW *win, *subw;
	PANEL *panel;
	ITEM **mitems = NULL;
	MENU *menu;
	char *tmp = NULL;
	int rows, cols;
	int selected = -1;
	char *mbuf = NULL;
	struct d_entries *entries = NULL;
	struct stat st;
	int index = 0;
	int len = strlen(path);

	/* /some/path/blah/../ */
	if (path[len - 1] == '.' && path[len - 2] == '.' &&
		path[len - 3] == '/') {
	    tmp = path;
	    tmp += strlen(path) - 5;

	    /* /some/path/ */
	    while (*--tmp != '/')
		*tmp = '\0';

	    if (!*path) {
		path[0] = '/';
		path[1] = '\0';
	    }
	}

	if (path[1] && path[strlen(path) - 1] == '/')
	    path[strlen(path) - 1] = '\0';

	if ((entries = get_directory_entries(path)) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", path, strerror(errno));
	    return NULL;
	}

	for (i = 0; entries[i].name; i++) {
	    mitems = Realloc(mitems, (index + 2) * sizeof(ITEM));
	    mitems[index++] = new_item(entries[i].fancy, entries[i].desc);
	}

	mitems[index] = NULL;
	menu = new_menu(mitems);
	scale_menu(menu, &rows, &cols);

	if (cols < strlen(path))
	    cols = strlen(path);

	if (cols < strlen(HELP_PROMPT))
	    cols = strlen(HELP_PROMPT);

	rows = BROWSE_HEIGHT;
	cols += 2;

	win = newwin(rows + 4, cols, CALCPOSY(rows) - 2, CALCPOSX(cols));
	set_menu_format(menu, BROWSE_HEIGHT, 0);
	set_menu_win(menu, win);
	subw = derwin(win, rows, cols - 2, 2, 1);
	set_menu_sub(menu, subw);
	set_menu_fore(menu, A_REVERSE);
	set_menu_grey(menu, A_NORMAL);
	set_menu_mark(menu, NULL);
	set_menu_spacing(menu, 2, 0, 0);
	menu_opts_off(menu, O_NONCYCLIC);
	post_menu(menu);
	panel = new_panel(win);

	draw_window_title(win, path, cols, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);
	draw_prompt(win, rows + 2, cols + 2, HELP_PROMPT, CP_MESSAGE_PROMPT);

	cbreak();
	noecho();
	keypad(win, TRUE);
	set_menu_pattern(menu, mbuf);

	if (isgraph(initkey)) {
	    menu_driver(menu, initkey);
	    initkey = '\0';
	}

	while (1) {
	    int c;

	    /* This nl() statement needs to be here because NL is recognized
	     * for some reason after the first selection.
	     */
	    nl();
	    update_panels();
	    doupdate();

	    c = wgetch(win);

	    switch (c) {
		case CTRL('P'):
		case KEY_PPAGE:
		    menu_driver(menu, REQ_SCR_UPAGE);
		    break;
		case ' ':
		case CTRL('N'):
		case KEY_NPAGE:
		    menu_driver(menu, REQ_SCR_DPAGE);
		    break;
		case KEY_UP:
		    menu_driver(menu, REQ_UP_ITEM);
		    break;
		case KEY_DOWN:
		    menu_driver(menu, REQ_DOWN_ITEM);
		    break;
		case '\n':
		    selected = item_index(current_item(menu));
		    goto gotitem;
		    break;
		case KEY_ESCAPE:
		    cleanup(win, subw, panel, menu, mitems, entries);
		    file[0] = '\0';
		    goto done;
		    break;
		case CTRL('G'):
		    help(BROWSER_HELP, file_browser_help);
		    break;
		case '~':
		    if ((tmp = getenv("HOME")) == NULL) {
			cmessage(ERROR, ANYKEY, "%s", E_HOME_ENV);
			break;
		    }

		    strncpy(path, tmp, sizeof(path));
		    cleanup(win, subw, panel, menu, mitems, entries);
		    goto again;
		    break;
		case CTRL('X'):
		    if ((tmp = get_input_str_clear(BROWSER_CHDIR_TITLE, NULL)) 
			    == NULL)
			break;

		    tmp = tilde_expand(tmp);
		    strncpy(path, tmp, sizeof(path));
		    cleanup(win, subw, panel, menu, mitems, entries);
		    goto again;
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
	snprintf(file, sizeof(file), "%s", entries[selected].name);

	if (stat(file, &st) == -1) {
	    cmessage(ERROR, ANYKEY, "%s", strerror(errno));
	    cleanup(win, subw, panel, menu, mitems, entries);
	    continue;
	}

	cleanup(win, subw, panel, menu, mitems, entries);

	if (S_ISDIR(st.st_mode)) {
	    strncpy(path, file, sizeof(path));
	    continue;
	}

	if (S_ISREG(st.st_mode))
	    break;

	cmessage(ERROR, ANYKEY, "%s", E_NOTAREGFILE);
    }

done:
    chdir(oldwd);
    free(oldwd);
    return (*file) ? file : NULL;
}
