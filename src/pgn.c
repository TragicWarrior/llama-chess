/* $Id: pgn.c,v 1.1.1.1 2002-12-05 20:38:47 bjk Exp $ */
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

/* This tries to match the PGN specification as much as possible. The only
 * real exception are the 'Result' and 'Date' tags which are converted for
 * easier reading. It'll be converted back to the standard when saving to a
 * file.
 *
 * User defined tags are supported and can be displayed in-game (see help).
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <err.h>
#include <string.h>
#include <time.h>
#include <pwd.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "pgn.h"

static void add_pgn_data(int index, const char *token, const char *value)
{
    pgn = Realloc(pgn, (index + 2) * sizeof(struct pgndata));

    strncpy(pgn[index].token, token, sizeof(pgn[index].token));
    strncpy(pgn[index].value, value, sizeof(pgn[index].value));
    return;
}

static void init_data()
{
    time_t now;
    char tbuf[MAX_TIME_LEN + 1] = {0};
    struct passwd *pwd;
    struct tm *tp;

    if ((pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    pgn_index = 0;
    time(&now);
    tp = localtime(&now);
    strftime(tbuf, sizeof(tbuf), TIME_FORMAT, tp);

    /* The standard seven tag roster (in order of appearance). */
    add_pgn_data(pgn_index++, "Event", UNKNOWN);
    add_pgn_data(pgn_index++, "Site", UNKNOWN);
    add_pgn_data(pgn_index++, "Date", tbuf);
    add_pgn_data(pgn_index++, "Round", UNKNOWN);
    add_pgn_data(pgn_index++, "White", pwd->pw_gecos);
    add_pgn_data(pgn_index++, "Black", UNKNOWN);
    add_pgn_data(pgn_index++, "Result", UNKNOWN);

    return;
}

int parse_pgn_file(const char *filename)
{
    FILE *fp;
    char buf[MAX_PGN_LINE_LEN], *tmp;

    if (!filename[0]) {
	init_data();
	return 0;
    }

    if ((fp = fopen(filename, "r")) == NULL)
	return 1;

    pgn_index = 0;

    while ((tmp = fgets(buf, sizeof(buf), fp)) != NULL) {
	char *token, *value;
	int len = strlen(tmp);
	int i;
	char tbuf[MAX_TIME_LEN + 1] = {0};
	struct tm tp;

	/* Need more comment handling. */
	if (tmp[0] == '%')
	    continue;

	if (tmp[len - 1] == '\n')
	    tmp[len-- - 1] = 0;

	/* Must be a roster tag... */
	if (tmp[0] == '[') {
	    tmp++;

	    if ((token = strsep(&tmp, " ")) != NULL) {
		tmp++; /* Skip the initial value quote. */
		tmp[strlen(tmp) - 2] = 0; /* Remove trailing '"]' from value. */
		value = tmp;

		if (strcmp(token, "Date") == 0) {
		    if (strptime(value, PGN_TIME_FORMAT, &tp) != NULL) {
			strftime(tbuf, sizeof(tbuf), TIME_FORMAT, &tp);
			value = (char *)tbuf;
		    }
		}
		else if (strcmp(token, "Result") == 0) {
		    for (i = 0; i < NARRAY(fancy_results); i++) {
			if (strcmp(value, fancy_results[i].pgn) == 0)
			    value = fancy_results[i].fancy;
		    }
		}

		add_pgn_data(pgn_index++, token, value);
	    }

	    continue;
	}
    
	/* Must be move text... */
    }

    fclose(fp);
    return 0;
}
