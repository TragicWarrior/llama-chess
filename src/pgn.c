/* $Id: pgn.c,v 1.48 2003-01-09 17:30:13 bjk Exp $ */
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
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
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

int end_of_game(const char *str)
{
    int i;

    for (i = 0; i < NARRAY(fancy_results); i++) {
	if (strstr(str, fancy_results[i].pgn) != NULL) {
	    strncpy(game[gindex].pgn[PGN_RESULT].value, fancy_results[i].fancy,
		    sizeof(game[gindex].pgn[PGN_RESULT].value));
	    return 1;
	}
    }

    return 0;
}

/* Returns 1 if a duplicate tag was found. 0 otherwise. The index argument is
 * a pointer to int, and incremented automatically.
 */
int add_pgn_data(struct pgndata **dst, int *n, char *token, char *value)
{
    int i, index = *n;
    struct pgndata *tdata = *dst;

    token = trim(token);
    value = trim(value);

    /* If a duplicate was found, update the existing one to the new value. */
    for (i = 0; i < index; i++) {
	if (strcasecmp(tdata[i].token, token) == 0) {
	    if (value)
		strncpy(tdata[i].value, value, sizeof(tdata[i].value));
	    else
		tdata[i].value[0] = '\0';

	    *dst = tdata;
	    return 1;
	}
    }

    tdata = Realloc(tdata, (index + 2) * sizeof(struct pgndata));

    strncpy(tdata[index].token, token, sizeof(tdata[index].token));

    if (value)
	strncpy(tdata[index].value, value, sizeof(tdata[index].value));

    memset(&tdata[++index], 0, sizeof(struct pgndata));
    *n = index;
    *dst = tdata;
    return 0;
}

static char *remove_pgn_tag_escapes(const char *str)
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

void init_board(struct board_matrix matrix[8][8])
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

	    matrix[row][col].icon = (row < 2) ? c : toupper(c);
	}
    }

    return;
}

void set_pgn_defaults()
{
    time_t now;
    char tbuf[MAX_TIME_LEN + 1] = {0};
    struct passwd *pwd;
    struct tm *tp;
    int n;

    if ((pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    time(&now);
    tp = localtime(&now);
    strftime(tbuf, sizeof(tbuf), TIME_FORMAT, tp);

    /* The standard seven tag roster (in order of appearance). */
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Event",
	    UNKNOWN);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Site",
	    UNKNOWN);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Date", tbuf);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Round", "?");
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "White", 
	    pwd->pw_gecos);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Black",
	    UNKNOWN);
    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, "Result", "*");

    /* Add custom tags from the configuration file. */
    for (n = 0; n < config.pindex; n++)
	add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, 
		config.pgn[n].token, config.pgn[n].value);

    return;
}

static void reset_game_data()
{
    if (gtotal)
	free_game_data();

    gtotal = gindex = 0;
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

int move_text(FILE *fp)
{
    char move[MAX_PGN_MOVE_LEN + 1] = {0}, *p;
    int c;
    int count;

    while((c = fgetc(fp)) != EOF) {
	if (isspace(c) || isdigit(c) || c == '.')
	    continue;

	break;
    }

    ungetc(c, fp);

    if (fscanf(fp, " %[a-hPRNBQK1-9#+=Ox-]%n", move, &count) != 1)
	return 1;

    if ((p = a2a4tosan(pgnboard, move)) == NULL)
	return 1;

    if (parse_move_text(pgnboard, p, 0))
	return 1;

    add_to_history(&game[gindex].history, &game[gindex].hindex,
	    &game[gindex].htotal, p);

    /*
    printf("%s %s\n", move, p);
    dump_board(pgnboard);
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

    return;
}

static void move_annotation(FILE *fp, int terminator)
{
    char *a = game[gindex].history[game[gindex].hindex - 1].comment;
    int c, lastchar = 0;

    skip_leading_space(fp);

    while ((c = fgetc(fp)) != EOF && c != terminator) {
	if (c == '\n')
	    continue;

	if (isspace(c) && isspace(lastchar))
	    continue;

	*a++ = lastchar = c;
    }

    *a = '\0';

    strncpy(game[gindex].history[game[gindex].hindex - 1].comment, 
	    trim(game[gindex].history[game[gindex].hindex - 1].comment),
	    sizeof(game[gindex].history[game[gindex].hindex - 1].comment));

    return;
}

static void pgn_tag(FILE *fp)
{
    char name[255], *n = name;
    char value[255], *v = value;
    int c, i = 0;
    int quoted_string = 0;

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

	*v++ = c;
    }

    *v = '\0';

    while (isspace(*--v))
	*v = '\0';

    if (*v == '\"')
	*v = '\0';

    while (isspace(*--v))
	*v = '\0';

    if (*value == '\0')
	strncpy(value, UNKNOWN, sizeof(value));

    add_pgn_data(&game[gindex].pgn, &game[gindex].pindex, name, 
	    remove_pgn_tag_escapes(value));

    return;
}

static int eog_marker(FILE *fp)
{
    int c, i = 0;
    char buf[8], *p = buf;

    while ((c = fgetc(fp)) != EOF && !isspace(c) && i++ < sizeof(buf))
	*p++ = c;

    for (i = 0; i < NARRAY(fancy_results); i++) {
	if (strcmp(buf, fancy_results[i].pgn) == 0) {
	    strncpy(game[gindex].pgn[PGN_RESULT].value, fancy_results[i].pgn,
		    sizeof(game[gindex].pgn[PGN_RESULT].value));
	    break;
	}
    }

    return 1;
}

#ifdef DEBUG
void dump_board(struct board_matrix b[][8])
{
    int row, col;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++) {
	    if (b[row][col].icon == '.') {
		printf(". ");
		continue;
	    }

	    printf("%c ", b[row][col].icon);
	}

	printf("\n");
    }

    printf("\n");
    return;
}
#endif

void new_game(struct board_matrix b[][8])
{
    static int firstrun;

    if (gtotal == 0)
	firstrun = 1;
    else
	gindex = gtotal - 1;

    if (!firstrun) {
	game = Realloc(game, (gindex + 2) * sizeof(struct games));
	game[gindex + 1].pindex = game[gindex + 1].hindex = 0;

	memset(&game[gindex + 1].pgn, 0, sizeof(struct pgndata));
	memset(&game[gindex + 1].history, 0, sizeof(struct history));
	gindex++;
    }
    else {
	game = Calloc(1, sizeof(struct games));
	firstrun = 0;
    }

    gtotal = gindex + 1;
    set_pgn_defaults();
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

int parse_pgn_file(const char *filename)
{
    FILE *fp;
    char buf[LINE_MAX] = {0}, *p = buf;
    int c;
    int tag_section = 0;
    int row, col;

    if (!*filename) {
	reset_game_data();
	new_game(board);
	return 0;
    }

    if ((fp = fopen(filename, "r")) == NULL)
	return 1;

    reset_game_data();

    while (1) {
	int nextchar = 0;

	if ((c = fgetc(fp)) == EOF) {
	    if (feof(fp))
		break;

	    if (ferror(fp)) {
		if (curses_initialized)
		    message(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
		else
		    warnx("%s: %s", filename, strerror(errno));

		clearerr(fp);
		continue;
	    }
	}

	if (c == '%') {
	    while ((c = fgetc(fp)) != EOF && c != '\n');
	    continue;
	}

	if (isspace(c))
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
		tag_section = 1;
		new_game(pgnboard);
	    }

	    pgn_tag(fp);
	    continue;
	}

	nextchar = fgetc(fp);

	/* EOG markers. */
	if ((isdigit(c) && (nextchar == '-' || nextchar == '/')) || c == '*') {
	    ungetc(nextchar, fp);
	    ungetc(c, fp);
	    eog_marker(fp);
	    continue;
	}

	ungetc(nextchar, fp);

	if (isdigit(c) || (c >= 'a' && c <= 'h') || c == 'N' || c == 'K'
		|| c == 'Q' || c == 'B' || c == 'R' || c == 'P' || c == 'O') {
	    ungetc(c, fp);

	    if (isdigit(c))
		status.turn = WHITE;
	    else
		status.turn = BLACK;

	    tag_section = 0;

	    if (move_text(fp))
		break;

	    continue;
	}

	*p++ = c;

	DEBUG("unparsed: '%s'\n", buf);

	if (strlen(buf) + 1 == sizeof(buf))
	    bzero(buf, sizeof(buf));

	continue;
    }

    fclose(fp);
    gtotal = gindex + 1;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++)
	    board[row][col].icon = pgnboard[row][col].icon;
    }

    return 0;
}

static char *pgn_escapes(const char *str)
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
	
static int integer_len(int n)
{
    int len = 1;

    if (n >= 10)
	len++;
    else if (n >= 100)
	len++;
    else if (n >= 1000)
	len++;
    else if (n >= 10000)
	len++;
    else if (n >= 100000)
	len++;

    return len;
}

static void dump_comments_and_nag(FILE *fp, int index, int *len)
{
    int i;
    int n;
    int x;

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (game[gindex].history[index].nag[i]) {
	    *len += integer_len(game[gindex].history[index].nag[i]) + 2;

	    if (*len + 1 >= 80) {
		fprintf(fp, "\n");
		*len = 0;
	    }

	    fprintf(fp, "$%i ", game[gindex].history[index].nag[i]);
	}
    }

    if (game[gindex].history[index].comment[0]) {
	fprintf(fp, "\n{");

	if ((n = strlen(game[gindex].history[index].comment) + 1) >= 80) {
	    for (i = 0, x = 0; i < (n - 1); i++, x++) {
		if (x + 1 >= 80) {
		    fprintf(fp, "\n");
		    x = 0;
		}

		if (fputc(game[gindex].history[index].comment[i], fp) == EOF)
		    warn("PGN Save");
	    }
	}
	else
	    fprintf(fp, "%s", game[gindex].history[index].comment);

	fprintf(fp, "}\n");
	*len = 0;
    }

    return;
}

static void dumpgame(FILE *fp, struct games g, int isfifo)
{
    int i;
    int n, len = 0;

    if (!isfifo) {
	for (i = 0; g.pgn[i].token[0]; i++) {
	    struct tm tp;

	    if (strcmp(g.pgn[i].token, "Date") == 0) {
		if (strptime(g.pgn[i].value, TIME_FORMAT, &tp) != NULL)
		    strftime(g.pgn[i].value, sizeof(g.pgn[i].value),
			    PGN_TIME_FORMAT, &tp);
	    }
	    else if (strcmp(g.pgn[i].token, "Round") == 0) {
		if (strcmp(g.pgn[i].value, UNKNOWN) == 0) {
		    g.pgn[i].value[0] = '?';
		    g.pgn[i].value[1] = '\0';
		}
	    }
	    else if (strcmp(g.pgn[i].token, "Result") == 0) {
		if (strcmp(g.pgn[i].value, UNKNOWN) == 0) {
		    g.pgn[i].value[0] = '*';
		    g.pgn[i].value[1] = '\0';
		}
		else {
		    for (n = 0; n < NARRAY(fancy_results); n++) {
			if (strcmp(g.pgn[i].value, fancy_results[n].fancy)
				== 0) {
			    strncpy(g.pgn[i].value, fancy_results[n].pgn, 
				    sizeof(g.pgn[i].value));
			    n = -1;
			    break;
			}
			else if (strcmp(g.pgn[i].value, fancy_results[n].pgn)
				== 0) {
			    n = -1;
			    break;
			}
		    }

		    if (n != -1) {
			g.pgn[i].value[0] = '*';
			g.pgn[i].value[1] = '\0';
		    }
		}
	    }
	    else if (strcmp(g.pgn[i].value, UNKNOWN) == 0)
		g.pgn[i].value[0] = '\0';

	    fprintf(fp, "[%s \"%s\"]\n", g.pgn[i].token, 
		    (g.pgn[i].value[0]) ? pgn_escapes(g.pgn[i].value) : "");
	}

	fprintf(fp, "\n");
    }

    /* Move text section. If it's dumping to the FIFO, dont dump comments and
     * NAG data.
     */
    for (i = len = 0, n = 1; i < g.htotal; i++) {
	int mlen = strlen(g.history[i].move);

	if (!(i % 2)) {
	    len += 2;
	    fprintf(fp, "%u. ", n++);
	}

	fprintf(fp, "%s ", g.history[i].move);

	if (!isfifo)
	    dump_comments_and_nag(fp, i, &len);

	len += mlen + integer_len(n) + 1;

	if (len + 1 >= 80) {
	    fprintf(fp, "\n");
	    len = 0;
	}
    }

    if (strlen(g.pgn[PGN_RESULT].value) + len + 1 >= 80)
	fprintf(fp, "\n");

    fprintf(fp, "%s\n\n", pgn_escapes(g.pgn[PGN_RESULT].value));
    return;
}

int save_pgn(const char *filename, int isfifo)
{
    FILE *fp;
    char *mode = NULL;
    int c;
    char buf[FILENAME_MAX];
    struct stat st;
    int i;
    int cgame = 0;

    if (gtotal > 1 && !isfifo) {
	c = message_uncentered(NULL, SAVE_MULTIGAME_P, "%s", SAVE_MULTIGAME);

	if (c == 'c')
	    cgame = 1;
	else if (c == 'a');
	else
	    return 1;
    }

    if (filename[0] != '/' && config.savedirectory[0] && !isfifo) {
	if (stat(config.savedirectory, &st) == -1) {
	    if (errno == ENOENT) {
		if (mkdir(config.savedirectory, 0755) == -1) {
		    message(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			    strerror(errno));
		    return 1;
		}
	    }
	    else {
		message(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			strerror(errno));
		return 1;
	    }
	}

	stat(config.savedirectory, &st);

	if (!S_ISDIR(st.st_mode)) {
	    message(ERROR, ANYKEY, "%s: not a directory", 
		    config.savedirectory);
	    return 1;
	}

	snprintf(buf, sizeof(buf), "%s/%s", config.savedirectory, filename);
	filename = buf;
    }

    if (!isfifo && !cgame)
	strncpy(pgnfile, filename, sizeof(pgnfile));

    /* This is a hack to resume an existing game when more than one game is
     * available. Also resuming a saved game and a game from history.
     */
    if (isfifo)
	mode = "w";
    else {
	if (access(filename, W_OK) == 0) {
	    c = message(NULL, OVERWRITE_PROMPT,
		    "File \"%s\" exists.", filename);

	    switch (c) {
		case 'a':
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

    if ((fp = fopen(filename, mode)) == NULL) {
	message(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	return 1;
    }

    if (isfifo || cgame)
	dumpgame(fp, game[gindex], isfifo);
    else {
	for (i = 0; i < gtotal; i++) {
	    if (game[i].htotal)
		dumpgame(fp, game[i], isfifo);
	    else
		message(NULL, ANYKEY, "%s #%i.", E_SAVE_NOMOVES, i + 1);
	}
    }

    fclose(fp);
    return 0;
}

static void cleanup(WINDOW *win, PANEL *panel, MENU *menu, ITEM **items,
	struct d_entries *entries)
{
    int i;

    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; items[i]; i++)
	free_item(items[i]);

    if (entries) {
	for (i = 0; entries[i].name; i++) {
	    free(entries[i].name);
	    free(entries[i].fancy);
	}

	free(entries);
    }

    del_panel(panel);
    delwin(win);
}

struct pgndata *edit_pgn_data(int edit)
{
    struct pgndata *data = NULL;
    struct tm tp;
    int data_index = 0;
    int i, lastindex = 0;

    /* Edit the backup copy, not the original in case the save fails. */
    for (i = 0; i < game[gindex].pindex; i++)
	add_pgn_data(&data, &data_index, game[gindex].pgn[i].token,
		game[gindex].pgn[i].value);

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

	for (i = 0; i < data_index; i++) {
	    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	    mitems[i] = new_item(data[i].token,
		    (strlen(data[i].value) > MAX_VALUE_WIDTH)
		     ? "Press ENTER..." : data[i].value);
	}

	mitems[i] = NULL;
	menu = new_menu(mitems);
	scale_menu(menu, &rows, &cols);

	/* +14 for the extra prompt info. */
	if (cols < strlen(HELP_PROMPT) + 14)
	    cols = strlen(HELP_PROMPT) + 14;

	win = newwin(rows + 4, cols + 2, CALCPOSY(rows) - 2, CALCPOSX(cols));
	set_menu_win(menu, win);
	subw = derwin(win, rows, cols, 2, 1);
	set_menu_sub(menu, subw);
	set_menu_fore(menu, A_REVERSE);
	set_menu_grey(menu, A_NORMAL);
	set_menu_mark(menu, NULL);
	set_menu_pad(menu, '-');
	set_menu_spacing(menu, 3, 0, 0);
	menu_opts_off(menu, O_NONCYCLIC);
	post_menu(menu);
	panel = new_panel(win);
	wbkgd(win, CP_MESSAGE_WINDOW);
	draw_window_title(win, (edit) ? PGN_EDIT_TITLE : PGN_INFO_TITLE, 
		cols + 2, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);

	cbreak();
	noecho();
	keypad(win, TRUE);
	curs_set(0);
	set_menu_pattern(menu, mbuf);

	while (1) {
	    int c;
	    struct pgndata *tmppgn = NULL;
	    char *newtag = NULL;
	    int tpgn_index = 0;
	    char *tmp;

	    if (set_current_item(menu, mitems[lastindex]) != E_OK) {
		lastindex = item_count(menu) - 1;
		continue;
	    }

	    snprintf(buf, sizeof(buf), "Tag %i of %i  %s",
		    item_index(current_item(menu)) + 1, item_count(menu), 
		    HELP_PROMPT);
	    draw_prompt(win, rows + 2, cols + 2, buf, CP_MESSAGE_PROMPT);

	    /* This nl() statement needs to be here because NL is recognized
	     * for some reason after the first selection.
	     */
	    nl();
	    update_panels();
	    doupdate();

	    c = wgetch(win);

	    switch (c) {
		case CTRL('G'):
		    if (edit)
			help(PGN_EDIT_HELP, pgn_edit_help);
		    else
			help(PGN_INFO_HELP, pgn_info_help);
		    break;
		case CTRL('R'):
		    if (!edit)
			break;

		    selected = item_index(current_item(menu));

		    if (selected <= 6) {
			message(NULL, ANYKEY, PGN_REMOVE_STR);
			goto cleanup;
		    }

		    for (i = 0; i < data_index; i++) {
			if (i == selected)
			    continue;

			add_pgn_data(&tmppgn, &tpgn_index, data[i].token,
				data[i].value);
		    }

		    for (i = data_index = 0; i < tpgn_index; i++) {
			add_pgn_data(&data, &data_index, tmppgn[i].token,
				tmppgn[i].value);
		    }

		    free(tmppgn);
		    goto cleanup;
		    break;
		case CTRL('A'):
		    if (!edit)
			break;

		    if ((newtag = get_input(PGN_NEW_TAG, NULL, 1, 0, NULL, NULL,
				    NULL, 0, FIELD_TYPE_PGN_TAG_NAME)) == NULL)
			break;

		    newtag[0] = toupper(newtag[0]);

		    for (i = 0; i < data_index; i++) {
			if (strcasecmp(data[i].token, newtag) == 0) {
			    selected = i;
			    goto gotitem;
			}
		    }

		    add_pgn_data(&data, &data_index, newtag, NULL);

		    selected = data_index - 1;
		    goto gotitem;
		    break;
		case KEY_HOME:
		    menu_driver(menu, REQ_FIRST_ITEM);
		    break;
		case KEY_END:
		    menu_driver(menu, REQ_LAST_ITEM);
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
		    cleanup(win, panel, menu, mitems, NULL);
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

	if (!edit) {
	    if (strcmp(data[selected].value, UNKNOWN) == 0)
		goto cleanup;

	    snprintf(buf, sizeof(buf), "Tag Information for \"%s\"", 
		    data[selected].token);
	    message(buf, ANYKEY, "%s", data[selected].value);
	    goto cleanup;
	}

	snprintf(buf, sizeof(buf), "%s \"%s\"", PGN_EDIT_TAG,
		data[selected].token);

	if (strcmp(data[selected].token, "Date") == 0) {
	    tmp = get_input(buf, data[selected].value, 0, 0, 0, NULL, NULL,
		    NULL, FIELD_TYPE_PGN_DATE);

	    if (tmp) {
		if (strptime(tmp, PGN_TIME_FORMAT, &tp) == NULL) {
		    message(ERROR, ANYKEY, "The \"Date\" tag must be in "
			    "YYYY.MM.DD format");
		    goto cleanup;
		}
	    }
	    else
		goto cleanup;
	}
	else if (strcmp(data[selected].token, "Round") == 0)
	    tmp = get_input(buf, NULL, 1, 1, NULL, NULL, NULL, 0,
		    FIELD_TYPE_PGN_ROUND);
	else if (strcmp(data[selected].token, "Result") == 0) {
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
	    if (strcmp(data[selected].value, UNKNOWN) == 0)
		tmp = NULL;
	    else
		tmp = data[selected].value;

	    tmp = get_input(buf, tmp, 0, 0, NULL, NULL, NULL, 0, -1);
	}

	strncpy(data[selected].value, (tmp) ? tmp : UNKNOWN,
		sizeof(data[selected].value));

cleanup:
	cleanup(win, panel, menu, mitems, NULL);
    }

done:
    if (!edit) {
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
	entries[index].fancy = (char *)Malloc(len);
	strncpy(entries[index].fancy, tmp, len);

	if (S_ISDIR(st.st_mode))
	    entries[index].fancy[len - 2] = '/';

	tp = localtime(&st.st_mtime);
	strftime(tbuf, sizeof(tbuf), "%b %d %T", tp);

	snprintf(entries[index].desc, sizeof(entries[index].desc), "%-7i %s", 
		n, tbuf);

	memset(&entries[++index], 0, sizeof(struct d_entries));
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

    if (config.savedirectory[0]) {
	if ((dp = opendir(config.savedirectory)) == NULL) {
	    message(ERROR, ANYKEY, "%s: %s", config.savedirectory,
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
	    message(ERROR, ANYKEY, "%s: %s", path, strerror(errno));
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
	draw_prompt(win, rows + 2, cols, HELP_PROMPT, CP_MESSAGE_PROMPT);

	cbreak();
	noecho();
	keypad(win, TRUE);
	set_menu_pattern(menu, mbuf);

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
		    cleanup(win, panel, menu, mitems, entries);
		    file[0] = '\0';
		    goto done;
		    break;
		case CTRL('G'):
		    help(BROWSER_HELP, file_browser_help);
		    break;
		case '~':
		    if ((tmp = getenv("HOME")) == NULL) {
			message(ERROR, ANYKEY, 
				"HOME environment variable unset");
			break;
		    }

		    strncpy(path, tmp, sizeof(path));
		    cleanup(win, panel, menu, mitems, entries);
		    goto again;
		    break;
		case CTRL('X'):
		    if ((tmp = get_input_str_clear(CHANGE_DIRECTORY, NULL)) 
			    == NULL)
			break;

		    tmp = tilde_expand(tmp);
		    strncpy(path, tmp, sizeof(path));
		    cleanup(win, panel, menu, mitems, entries);
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
	    message(ERROR, ANYKEY, "%s", strerror(errno));
	    cleanup(win, panel, menu, mitems, entries);
	    continue;
	}

	cleanup(win, panel, menu, mitems, entries);

	if (S_ISDIR(st.st_mode)) {
	    strncpy(path, file, sizeof(path));
	    continue;
	}

	if (S_ISREG(st.st_mode))
	    break;

	message(ERROR, ANYKEY, "Not a regular file.");
    }

done:
    chdir(oldwd);
    free(oldwd);
    return (*file) ? file : NULL;
}
