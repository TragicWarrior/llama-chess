/* $Id: engine.c,v 1.30 2003-01-25 17:43:29 bjk Exp $ */
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

		    if (kill(enginepid, 0) == -1) {
			message(ERROR, ANYKEY, "Could not write to engine. "
				"Process no longer exists.");
			engine_initialized = 0;
			status.engine = ENGINE_OFFLINE;
			update_status();
			return;
		    }

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

/* This is ripped from XBoard. */
static int get_pty(char *pty_name)
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

static char **parseargs(char *str)
{
    char **pptr, *s;
    char arg[255];
    int index = 0;
    int quote = 0;
    int lastchar = 0;
    int i;

    if (!str)
	return NULL;

    if (!(pptr = malloc(sizeof(char *))))
	return NULL;

    for (i = 0, s = str; *s; lastchar = *s++) {
	if ((*s == '\"' || *s == '\'') && lastchar != '\\') {
	    quote = (quote) ? 0 : 1;
	    continue;
	}

	if (*s == ' ' && !quote) {
	    arg[i] = 0;
	    pptr = realloc(pptr, (index + 2) * sizeof(char *));
	    pptr[index++] = strdup(arg);
	    arg[0] = i = 0;
	    continue;
	}
	
	if ((i + 1) == sizeof(arg))
	    continue;

	arg[i++] = *s;
    }

    arg[i] = 0;

    if (arg[0]) {
	pptr = realloc(pptr, (index + 2) * sizeof(char *));
	pptr[index++] = strdup(arg);
    }

    pptr[index] = NULL;
    return pptr;
}

/* Is this dangerous if pty permissions are wrong? */
pid_t init_chess_engine(char **args)
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
    errno = 0;

    switch ((pid = fork())) {
	case -1:
	    return -2;
	case 0:
	    dup2(to[0], STDIN_FILENO);
	    dup2(from[1], STDOUT_FILENO);
	    close(to[0]);
	    close(to[1]);
	    close(from[0]);
	    close(from[1]);
	    dup2(STDOUT_FILENO, STDERR_FILENO);
	    execvp(args[0], args);
	    _exit(EXIT_FAILURE);
	default:
	    break;
    }

    sleep(1);

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

void set_engine_defaults()
{
    SEND_TO_ENGINE("book %s\n", book_method(config.book_method));
    SEND_TO_ENGINE("depth %i\n", config.engine_depth);
    return;
}

void stop_engine()
{
    if (!engine_initialized)
	return;

    SEND_TO_ENGINE("quit\n");

    if (kill(enginepid, 0) != -1)
	kill(enginepid, SIGTERM);

    if (kill(enginepid, 0) != -1)
	kill(enginepid, SIGKILL);

    return;
}

int start_chess_engine()
{
    char **args;

    status.engine = ENGINE_INITIALIZING;
    update_status();
    update_panels();
    doupdate();

    args = parseargs(config.engine_cmd);
    enginepid = init_chess_engine(args);

    switch (enginepid) {
	case -1:
	    /* Pty allocation. */
	    status.engine = ENGINE_OFFLINE;
	    message(ERROR, ANYKEY, "Could not allocate PTY");
	    break;
	case -2:
	    /* Could not execute engine. */
	    status.engine = ENGINE_OFFLINE;
	    message(ERROR, ANYKEY, "%s: %s", args[0], strerror(errno));
	    break;
	default:
	    status.engine = ENGINE_READY;
	    break;
    }

    if (enginepid > 0)
	set_engine_defaults();

    update_status();
    return enginepid;
}

/* Once the PGN parser has been well tested, parse_move_text() from the human
 * move can disappear.
 */
void parse_engine_output(BOARD b, char *str)
{
    char *tmp;
    char move[MAX_PGN_MOVE_LEN + 1] = {0}, *p = move;
    int count;

    /* Human move. Add it to the move history. */
    if (sscanf(str, "%*d%*1[.]%*1[ ]%[a-zA-Z0-9+=#-]%n", move, &count)
	    == 1) {
	if (parse_move_text(b, move, 0)) {
	    message(ERROR, ANYKEY, "BUG: %s: %s", E_INVALID_MOVE, move);
	    return;
	}

        if (game[gindex].htotal == 0 && status.side == BLACK)                   
	    game[gindex].openingside = BLACK;

	add_to_history(&game[gindex].history, &game[gindex].hindex, 
		&game[gindex].htotal, p);

	switch_turn();

	status.engine = ENGINE_THINKING;
	sp.icon = 0;
	str += count;

	/* This needs to be here in case the engine move text is bunched
	 * up with the white move text.
	 */
	goto engine_move;
    }

    /* Engine move. */
engine_move:
    if (sscanf(str, "%*d%*1[.]%*1[ ]%*3[.]%*1[ ]%[a-zA-Z0-9+=#-]%n", move, 
		&count) == 1) {
	/* Moves from the engine are in a2a4 format (Xboard protocol) so we
	 * need to convert them.
	 */
	if ((p = a2a4tosan(b, move)) == NULL)
	    return;

	if (parse_move_text(b, p, 0)) {
	    message(ERROR, ANYKEY, "BUG: %s: %s", E_INVALID_MOVE, p);
	    return;
	}

        if (game[gindex].htotal == 0 && status.side == BLACK)                   
	    game[gindex].openingside = BLACK;

	add_to_history(&game[gindex].history, &game[gindex].hindex, 
		&game[gindex].htotal, p);

	switch_turn();

	str += count;
	RETURN;
    }

    /* Miscellaneous one-liners. */

    /* The engine is now reading a FIFO. Dump what we need to it. */
    if (strstr(str, "pgnload") != NULL) {
	if (save_pgn(config.fifo, 1)) {
	    game[gindex].htotal = oldhistorytotal;
	    oldhistorytotal = 0;
	    return;
	}

	set_engine_defaults();
	return;
    }

    /* 'depth' command. */
    if ((tmp = strstr(str, "Search to a depth of ")) != NULL) {
	tmp += 21;
	tmp = trim(tmp);
	config.engine_depth = atoi(tmp);
    }

    /* 'switch' command. */
    if (strstr(str, "White to move") != NULL) {
	status.side = status.turn = WHITE;
	RETURN;
    }
    else if (strstr(str, "Black to move") != NULL) {
	status.side = status.turn = BLACK;
	RETURN;
    }

    /* Bad engine command or move. */
    if ((tmp = strstr(str, "Illegal move: ")) != NULL) {
	status.notify = "Illegal move";
	sp.icon = 0;
	RETURN;
    }

    if ((tmp = strstr(str, "Cannot open file ")) != NULL) {
	status.notify = "Engine could not open file";
	RETURN;
    }

    if ((tmp = strstr(str, " No book found.")) != NULL)
	config.book_method = -1; 
    else if ((tmp = strstr(str, "book now off.")) != NULL)
	config.book_method = BOOK_OFF;
    else if ((tmp = strstr(str, "book now on.")) != NULL)
	config.book_method = BOOK_PREFER;
    else if ((tmp = strstr(str, "book now best.")) != NULL)
	config.book_method = BOOK_BEST;
    else if ((tmp = strstr(str, "book now worst.")) != NULL)
	config.book_method = BOOK_WORST;
    else if ((tmp = strstr(str, "book now random.")) != NULL)
	config.book_method = BOOK_RANDOM;

    return;
}

