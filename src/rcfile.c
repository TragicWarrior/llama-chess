/* $Id: rcfile.c,v 1.6 2002-12-19 16:53:05 bjk Exp $ */
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
#include <string.h>
#include <err.h>
#include <ctype.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "rcfile.h"

void parse_rcfile(const char *filename)
{
    FILE *fp;
    char *line, buf[LINE_MAX];
    int lines = 0;
    int n;

    if ((fp = fopen(filename, "r")) == NULL)
	err(EXIT_FAILURE, "%s", filename);

    while ((line = fgets(buf, sizeof(buf), fp)) != NULL) {
	char var[30], val[50];
	char token[MAX_PGN_LINE_LEN + 1], value[MAX_PGN_LINE_LEN + 1];

	lines++;
	line = trim(line);

	if (!line[0] || line[0] == '#')
	    continue;

	if (sscanf(line, "%s %[^\n]", var, val) != 2)
	    errx(EXIT_FAILURE, "%s(%i): parse error", filename, lines);

	strncpy(val, trim(val), sizeof(val));
	strncpy(var, trim(var), sizeof(var));

	if (strcmp(var, "book") == 0) {
	    if (strcmp(val, "prefer") == 0)
		config.book_method = BOOK_PREFER;
	    else if (strcmp(val, "random") == 0)
		config.book_method = BOOK_RANDOM;
	    else if (strcmp(val, "worst") == 0)
		config.book_method = BOOK_WORST;
	    else if (strcmp(val, "best") == 0)
		config.book_method = BOOK_BEST;
	    else if (strcmp(val, "off") == 0)
		config.book_method = BOOK_OFF;
	    else
		errx(EXIT_FAILURE, "%s(%i): invalid book method \"%s\"", 
			filename, lines, val);
	}
	else if (strcmp(var, "jumpcount") == 0) {
	    if (!isinteger(val))
		errx(EXIT_FAILURE, "%s(%i): value is not an integer", filename,
			lines);

	    config.history_jump = atoi(val);
	}
	else if (strcmp(var, "depth") == 0) {
	    if (!isinteger(val))
		errx(EXIT_FAILURE, "%s(%i): value is not an integer", filename,
			lines);

	    config.engine_depth = atoi(val);
	}
	else if (strcmp(var, "pgntag") == 0) {
	    if ((n = sscanf(val, "%s %s ", token, value)) < 1 || 
		    n > 2)
		errx(EXIT_FAILURE, "%s(%i): invalid value \"%s\"", filename, 
			lines, val);

	    if (n == 1)
		value[0] = 0;

	    for (n = 0; n < strlen(token); n++) {
		if (!isalpha(token[n]) && token[n] != '_')
		    errx(EXIT_FAILURE, 
			    "%s(%i): token names must match 0-9A-Za-z_.",
			    filename, lines);
	    }

	    token[0] = toupper(token[0]);
	    add_pgn_data(&config.pgn, &config.pindex, trim(token), trim(value));
	}
	else
	    errx(EXIT_FAILURE, "%s(%i): invalid parameter \"%s\"", filename,
		    lines, var);
    }

    fclose(fp);
    return;
}
