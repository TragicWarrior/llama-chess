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
#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>
#include <ncurses.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "chess.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

void write_debug_output(int which, const char *format, ...)
{
    FILE *fp = stderr;
    va_list ap;
    char *buf;

    va_start(ap, format);
    vasprintf(&buf, format, ap);
    va_end(ap);

    if (which) {
	if ((fp = fopen("debug", "a")) == NULL)
	    return;
    }

    fprintf(fp, "%s", buf);

    if (which)
	fclose(fp);

    free(buf);
    return;
}

char *debug_board(BOARD b)
{
    static char buf[64 + 8 + 16 + 1];
    char *p = buf;
    int row, col;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++) {
	    *p++ = (b[row][col].enpassant) ? 'x' : b[row][col].icon & A_CHARTEXT;
	    if (col < 7)
		*p++ = ' ';
	}

	*p++ = '\n';
    }

    *p = 0;
    return buf;
}

void dump_board(int which, BOARD b)
{
    write_debug_output(which, "%s", debug_board(b));
}
#endif
