/* $Id: engine.c,v 1.4 2002-12-06 21:41:20 bjk Exp $ */
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
#include <err.h>
#include <limits.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "engine.h"

static void parse_piece(char *str)
{
    int len;
    char *tmp;

    if ((tmp = strsep(&str, "\n")) == NULL)
	tmp = str;

    len = strlen(tmp);

    if (tmp[len - 1] == '#')
	status.notify = "Game Over!";
    else if (tmp[len - 1] == '+')
	status.notify = "Check!";
    else if (tmp[len - 2] == '=') /* FIXME */
	status.notify = "Promotion!";
    else
	status.notify = NULL;

    return;
}

#define HUMAN 0
#define ENGINE 1

void parse_engine_output(char *str)
{
    char *buf = str, *tmp;
    int row = 0;
    int i = 0;
    char move[MAX_MOVE_LEN + 1];


    if (strstr(str, "Thinking...") != NULL)
	status.engine = ENGINE_THINKING;

    /* This loads a PGN game into the history array. */
    if ((buf = strstr(str, "      White   Black")) != NULL) {
	if ((tmp = strsep(&buf, "\n")) != NULL) {
	    while ((tmp = strsep(&buf, "\n")) != NULL) {
		char black[MAX_MOVE_LEN + 1], white[MAX_MOVE_LEN + 1];

		if (!tmp[0])
		    break;

		if (sscanf(tmp, "%*u%*c%s%s", white, black) != 2) {
		    message(NULL, ANYKEY, "parse error while getting history");
		    return;
		}

		add_to_history(&i, white);
		add_to_history(&i, black);
	    }

	    history_index = i;
	    browse_history = 1;
	    status.engine = HISTORY_MODE;
	}
    }

    /* 'switch' command. */
    if (strstr(str, "White to move\n") != NULL) {
	status.bw = WHITE;

	if (status.turn == BLACK)
	    status.turn = WHITE;

	return;
    }
    else if (strstr(str, "Black to move\n") != NULL) {
	status.bw = BLACK;

	if (status.turn == WHITE)
	    status.turn = BLACK;

	return;
    }

    /* Bad engine command or move. */
    if ((tmp = strstr(str, "Illegal move: ")) != NULL) {
	str[strlen(str) - 1] = 0;
	message(NULL, ANYKEY, "%s", str);
	return;
    }

    /* Human move. Add it to the move history (if not browsing). */
    if (!browse_history) {
	if (sscanf(str, "%*u%*c%s", move) == 1)
	    add_to_history(&history_index, move);
    }

    /* This is output whenever a move is made/undone (and 'show board'). */
    if ((buf = strstr(str, "white  ")) != NULL || 
	    (buf = strstr(str, "black  ")) != NULL) {

	/* Engine finished move, add it to the move history. */
	if ((tmp = strstr(buf, "My move is : ")) != NULL) {
	    status.engine = ENGINE_READY;
	    tmp += 13;

	    if (tmp[strlen(tmp) - 1] == '\n')
		tmp[strlen(tmp) - 1] = 0;

	    add_to_history(&history_index, tmp);
	    parse_piece(tmp);
	}

	tmp = strsep(&buf, "\n");

	/* Whose turn? */
	if (strstr(tmp, "white  KQkq") != NULL)
	    status.turn = WHITE;
	else
	    status.turn = BLACK;

	/* Parse the board. */
	while ((tmp = strsep(&buf, "\n")) != NULL) {
	    int i, col = 0;

	    if (!*tmp)
		break;

	    for (i = 0; i < strlen(tmp); i++) {
		if (tmp[i] == ' ')
		    continue;

		if (tmp[i] == '.')
		    board[row][col++].icon = ' ';
		else
		    board[row][col++].icon = (isupper(tmp[i])) 
			? tmp[i] | A_BOLD : tmp[i];
	    }

	    row++;
	}

	return;
    }

    /* Miscellaneous one-liners. */
    if ((tmp = strstr(str, "Cannot open file ")) != NULL) {
	str[strlen(str) - 1] = 0;
	message(NULL, ANYKEY, "%s", str);
	return;
    }

    if ((tmp = strstr(str, "Cannot write to file ")) != NULL) {
	str[strlen(str) - 1] = 0;
	message(NULL, ANYKEY, "%s", str);
	return;
    }

    if ((tmp = strstr(str, " No book found.")) != NULL)
	status.book_method = -1; 
    else if ((tmp = strstr(str, "book now off")) != NULL)
	status.book_method = BOOK_OFF;
    else if ((tmp = strstr(str, "book now on")) != NULL)
	status.book_method = BOOK_PREFER;
    else if ((tmp = strstr(str, "book now best")) != NULL)
	status.book_method = BOOK_BEST;
    else if ((tmp = strstr(str, "book now worst")) != NULL)
	status.book_method = BOOK_WORST;
    else if ((tmp = strstr(str, "book now random")) != NULL)
	status.book_method = BOOK_RANDOM;

    return;
}

void init_chess_engine()
{
    pid_t pid;
    int p1[2], p2[2];

    if (pipe(p1) < 0)
	err(EXIT_FAILURE, "pipe()");

    if (pipe(p2) < 0)
	err(EXIT_FAILURE, "pipe()");

    switch ((pid = fork())) {
	case -1:
	    err(EXIT_FAILURE, "fork()");
	case 0:
	    close(STDIN_FILENO);
	    dup2(p1[0], STDIN_FILENO);
	    close(STDOUT_FILENO);
	    close(STDERR_FILENO);
	    dup2(p2[1], STDOUT_FILENO);
	    dup2(p2[1], STDERR_FILENO);
	    close(p1[0]);
	    close(p1[1]);
	    close(p2[0]);
	    close(p2[1]);
	    execlp("gnuchess", "gnuchess", NULL);
	    err(EXIT_FAILURE, "execlp()");
	default:
	    break;
    }

    close(p1[0]);
    close(p2[1]);
    from_engine = p2[0];
    to_engine = p1[1];
    fcntl(from_engine, F_SETFL, O_NONBLOCK | O_DIRECT);
    fcntl(to_engine, F_SETFL, O_NONBLOCK | O_DIRECT);
    return;
}

void send_to_engine(const char *format, ...)
{
    va_list ap;
    int len;
    char *line;

    va_start(ap, format);
#ifdef HAVE_VASPRINTF
    len = vasprintf(&line, format, ap);
#else
    line = Malloc(LINE_MAX);
    len = vsnprintf(line, LINE_MAX, format, ap);
#endif
    va_end(ap);

    while (1) {
	int n;
	fd_set fds;
	struct timeval tv;

	FD_ZERO(&fds);
	FD_SET(to_engine, &fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if ((n = select(to_engine + 1, NULL, &fds, NULL, &tv)) > 0) {
	    if (FD_ISSET(to_engine, &fds)) {
		if ((n = write(to_engine, line, len)) != len) {
		    message(NULL, ANYKEY, "write() error to engine. "
			    "Expected %i, got %i.", len, n);
		}
		else {
		    break;
		}
	    }
	}
	else {
	//    message(ERROR, ANYKEY, "write() timeout, trying again.\n");
	}
    }

    free(line);
    return;
}
