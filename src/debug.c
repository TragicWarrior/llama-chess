/* $Id: debug.c,v 1.1 2003-09-23 14:28:30 bjk Exp $ */
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
#ifdef DEBUG
#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

void write_debug_output(int which, const char *format, ...)
{
    FILE *fp = stdout;
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

void dump_board(int which, BOARD b)
{
    int row, col;

    for (row = 0; row < 8; row++) {
	for (col = 0; col < 8; col++)
	    write_debug_output(which, "%c ", (char)b[row][col].icon);

	write_debug_output(which, "%c", '\n');
    }

    return;
}
#endif
