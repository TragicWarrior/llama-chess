/* $Id: engine.c,v 1.6 2002-12-11 17:45:17 bjk Exp $ */
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

void parse_engine_output(char *str)
{
    char *buf = str, *tmp;
    int row = 0;
    char move[MAX_MOVE_LEN + 1];

    if (strstr(str, "Thinking...") != NULL)
	status.engine = ENGINE_THINKING;

    /* 'switch' command. */
    if (strstr(str, "White to move\n") != NULL) {
	status.bw = status.turn = WHITE;
	return;
    }
    else if (strstr(str, "Black to move\n") != NULL) {
	status.bw = status.turn = BLACK;
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

	/* This is needed when leaving history mode and the turn is now black
	 * since we just went. This cancels 'manual'.
	 */
	if (cancel_manual_mode) {
	    send_to_engine("go\n");
	    cancel_manual_mode = 0;
	}
    }

    /* This is output whenever a move is made/undone (and 'show board'). */
    if ((buf = strstr(str, "white  ")) != NULL || 
	    (buf = strstr(str, "black  ")) != NULL) {

	if (strstr(buf, "white  "))
	    status.turn = WHITE;

	if (strstr(buf, "black  "))
	    status.turn = BLACK;

	/* Engine finished move, add it to the move history. */
	if ((tmp = strstr(buf, "My move is : ")) != NULL) {
	    tmp += 13;
	    tmp = parse_piece(tmp);
	    add_to_history(&history_index, tmp);
	}

	tmp = strsep(&buf, "\n");

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
		    board[row][col++].icon = tmp[i];
	    }

	    row++;
	}

	if (browse_history)
	    status.engine = HISTORY_MODE;
	else {
	    if (status.bw == status.turn)
		status.engine = ENGINE_READY;
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
