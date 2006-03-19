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
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <err.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "epd.h"

char *board_to_fen(GAME g)
{
    int row, col;
    int i;
    static char buf[MAX_PGN_LINE_LEN], *p;
    int oldturn = g.turn;
    char enpassant[3] = {0}, *e;

    for (i = g.htotal; i >= g.hindex - 1; i--)
	switch_turn(&g);

    p = buf;

    for (row = 0; row < 8; row++) {
	int count = 0;

	for (col = 0; col < 8; col++) {
	    if (g.b[row][col].icon == 'x') {
		e = enpassant;
		g.b[row][col].icon = int_to_piece(g, OPEN_SQUARE);
		*e++ = 'a' + col;
		*e++ = ('0' + 8) - row;
		*e = 0;
	    }

	    if (piece_to_int(g.b[row][col].icon) == OPEN_SQUARE) {
		count++;
		continue;
	    }

	    if (count) {
		*p++ = '0' + count;
		count = 0;
	    }

	    *p++ = g.b[row][col].icon;
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

    if (TEST_FLAG(g.flags, GF_WK_CASTLE))
	*p++ = 'K';

    if (TEST_FLAG(g.flags, GF_WQ_CASTLE))
	*p++ = 'Q';

    if (TEST_FLAG(g.flags, GF_BK_CASTLE))
	*p++ = 'k';

    if (TEST_FLAG(g.flags, GF_BQ_CASTLE))
	*p++ = 'q';

    *p++ = ' ';

    if (enpassant[0]) {
	e = enpassant;
	*p++ = *e++;
	*p++ = *e++;
    }
    else
	*p++ = '-';

    *p++ = ' ';

    if (g.ply >= 10) {
	*p++ = '0' + (g.ply / 10);
	*p++ = '0' + g.ply % 10;
    }
    else
	*p++ = '0' + g.ply;

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

int parse_fen_line(GAME *g, BOARD b, char *str)
{
    char *tmp;
    char line[LINE_MAX], *s;
    int row = 8, col = 1;
    int moven;

    strncpy(line, str, sizeof(line));
    s = line;

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
			int_to_piece(*g, OPEN_SQUARE);
	    } 
	    else if (piece_to_int(*tmp) != -1)
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
	    (*g).turn = BLACK;
	    break;
	case 'w':
	    (*g).turn = WHITE;
	    break;
	default:
	    return 1;
    }

    tmp++;
    CLEAR_FLAG((*g).flags, GF_WK_CASTLE);
    CLEAR_FLAG((*g).flags, GF_WQ_CASTLE);
    CLEAR_FLAG((*g).flags, GF_BK_CASTLE);
    CLEAR_FLAG((*g).flags, GF_BQ_CASTLE);

    while (*tmp && *tmp != ' ') {
	switch (*tmp++) {
	    case 'K':
		SET_FLAG((*g).flags, GF_WK_CASTLE);
		break;
	    case 'Q':
		SET_FLAG((*g).flags, GF_WQ_CASTLE);
		break;
	    case 'k':
		SET_FLAG((*g).flags, GF_BK_CASTLE);
		break;
	    case 'q':
		SET_FLAG((*g).flags, GF_BQ_CASTLE);
		break;
	    default:
		return -1;
	}
    }

    while (*tmp)
	tmp++;

    while (*tmp != ' ')
	tmp--;

    moven = atoi(tmp);
    return moven;
}

// FIXME
#if 0
int parse_fen_file(const char *filename)
{
    FILE *fp;
    int compressed = 0;
    char line[LINE_MAX], *p;
    int ret = 0;

    if (access(filename, R_OK) == -1) {
	if (curses_initialized)
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	else
	    warn("%s", filename);

	return 1;
    }

    if ((fp = open_file(filename, &compressed)) == NULL)
	return 1;

    while ((p = fgets(line, sizeof(line), fp)) != NULL) {
	p = trim(p);

	if (*p == '%' || *p == '\0')
	    continue;

	if (parse_fen_line(b, p) == -1) {
	    if (curses_initialized)
		cmessage(ERROR, ANYKEY, "%s: %s", filename, E_FEN_PARSE);
	    else
		warnx("%s: %s", filename, E_FEN_PARSE);

	    ret = 1;
	    break;
	}
    }

    fclose(fp);

    if (compressed)
	unlink(config.tmpfile);

    return ret;
}
#endif
