/* $Id: engine.c,v 1.10 2002-12-16 17:52:05 bjk Exp $ */
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
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdarg.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "engine.h"

void send_to_engine(const char *format, ...)
{
    va_list ap;
    int len;
    char *line;
    int try = 0;

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
	FD_SET(enginefd[1], &fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if ((n = select(enginefd[1] + 1, NULL, &fds, NULL, &tv)) > 0) {
	    if (FD_ISSET(enginefd[1], &fds)) {
		n = write(enginefd[1], line, len);

		if (n == -1) {
		    if (errno == EAGAIN)
			continue;

		    message(ERROR, ANYKEY, "Attempt #%i. write(): %s", ++try,
			    strerror(errno));
		    continue;
		}

		if (len != n) {
		    message(NULL, ANYKEY, "try #%i: write() error to engine. "
			    "Expected %i, got %i.", ++try, len, n);
		    continue;
		}

		break;
	    }
	}
	else {
	    /* timeout */
	}
    }

    free(line);
    return;
}

void parse_engine_output(char *str)
{
    char *tmp;
    char move[MAX_PGN_MOVE_LEN + 1];

    /* 'switch' command. */
    if (strstr(str, "White to move") != NULL) {
	status.bw = status.turn = WHITE;
	RETURN;
    }
    else if (strstr(str, "Black to move") != NULL) {
	status.bw = status.turn = BLACK;
	RETURN;
    }

    /* Bad engine command or move. */
    if ((tmp = strstr(str, "Illegal move: ")) != NULL) {
	status.notify = "Illegal move";
	RETURN;
    }

    /* Human move. Add it to the move history (if not browsing). */
    if (!browse_history) {
	if (sscanf(str, "%*d%*1[.]%*1[ ]%[a-zA-Z0-9+=#-] ", move) == 1) {
	    add_to_history(&game[gindex].history, &game[gindex].hindex, 
		    &game[gindex].htotal, move);

	    if (status.turn == WHITE)
		status.turn = BLACK;
	    else if (status.turn == BLACK)
		status.turn = WHITE;

	    status.engine = ENGINE_THINKING;
	    move_piece(move);
	    sp.icon = 0;
	    return;
	}

	/* This is needed when leaving history mode and the turn is now black
	 * since we just went. This cancels 'manual'.
	 */
	/* FIXME this relies on the fifo stuff. */
	if (cancel_manual_mode) {
	    SEND_TO_ENGINE("go\n");
	    cancel_manual_mode = 0;
	}
    }

    /* Engine move. */
    if (sscanf(str, "%*d%*1[.]%*1[ ]%*3[.]%*1[ ]%[a-zA-Z0-9+=#-] ", move) 
	    == 1) {
	add_to_history(&game[gindex].history, &game[gindex].hindex, 
		&game[gindex].htotal, move);

	if (status.turn == WHITE)
	    status.turn = BLACK;
	else if (status.turn == BLACK)
	    status.turn = WHITE;

	move_piece(move);
	RETURN;
    }

    /* Miscellaneous one-liners. */
    if ((tmp = strstr(str, "Cannot open file ")) != NULL) {
	status.notify = "Engine could not open file";
	RETURN;
    }

    if ((tmp = strstr(str, " No book found.")) != NULL)
	status.book_method = -1; 
    else if ((tmp = strstr(str, "book now off.")) != NULL)
	status.book_method = BOOK_OFF;
    else if ((tmp = strstr(str, "book now on.")) != NULL)
	status.book_method = BOOK_PREFER;
    else if ((tmp = strstr(str, "book now best.")) != NULL)
	status.book_method = BOOK_BEST;
    else if ((tmp = strstr(str, "book now worst.")) != NULL)
	status.book_method = BOOK_WORST;
    else if ((tmp = strstr(str, "book now random.")) != NULL)
	status.book_method = BOOK_RANDOM;

    RETURN;
}

/* This is ripped from XBoard. */
static int get_pty(char pty_name[])
{
  struct stat stb;
  int c, i;
  int fd;

  /* Some systems name their pseudoterminals so that there are gaps in
     the usual sequence - for example, on HP9000/S700 systems, there
     are no pseudoterminals with names ending in 'f'.  So we wait for
     three failures in a row before deciding that we've reached the
     end of the ptys.  */
  int failed_count = 0;

  for (c = FIRST_PTY_LETTER; c <= LAST_PTY_LETTER; c++)
    for (i = 0; i < 16; i++) {
	sprintf (pty_name, "/dev/pty%c%x", c, i);

	if (stat (pty_name, &stb) < 0) {
	    failed_count++;

	    if (failed_count >= 3)
	      return -1;
	}
	else
	  failed_count = 0;

	fd = open(pty_name, O_RDWR, 0);

	if (fd >= 0) {
	    /* check to make certain that both sides are available
	       this avoids a nasty yet stupid bug in rlogins */
            sprintf (pty_name, "/dev/tty%c%x", c, i);

	    if (access (pty_name, 6) != 0) {
		close (fd);
		continue;
	    }

#ifdef IBMRTAIX
	      /* On AIX, the parent gets SIGHUP when a pty attached
                 child dies.  So, we ignore SIGHUP once we've started
                 a child on a pty.  Note that this may cause xboard
                 not to die when it should, i.e., when its own
                 controlling tty goes away.
	      */
	    signal(SIGHUP, SIG_IGN);
#endif /* IBMRTAIX */
	    return fd;
	}
    }

    return -1;
}

/* Is this dangerous if pty permissions are wrong? */
pid_t init_chess_engine()
{
    pid_t pid;
    int from[2], to[2];
    char pty[FILENAME_MAX];

    if ((to[1] = get_pty(pty)) == -1) {
	errno = 0;
	return -1;
    }

    from[0] = to[1];
    errno = 0;

    if ((to[0] = open(pty, O_RDWR, 0)) == -1)
	return -1;

    from[1] = to[0];

    switch ((pid = fork())) {
	case -1:
	    err(EXIT_FAILURE, "fork()");
	case 0:
	    dup2(to[0], STDIN_FILENO);
	    dup2(from[1], STDOUT_FILENO);
	    close(to[0]);
	    close(to[1]);
	    close(from[0]);
	    close(from[1]);
	    dup2(STDOUT_FILENO, STDERR_FILENO);
	    execlp("gnuchess", "gnuchess", "xboard", NULL);
	    err(EXIT_FAILURE, "execlp()");
	default:
	    break;
    }

    if (kill(pid, 0) == -1)
	return -2;

    close(to[0]);
    close(from[1]);

    enginefd[0] = from[0];
    enginefd[1] = to[1];

    fcntl(enginefd[0], F_SETFL, O_NONBLOCK);
    fcntl(enginefd[1], F_SETFL, O_NONBLOCK);

    engine_initialized = 1;
    return pid;
}
