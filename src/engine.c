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
#include <paths.h>

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
			//update_status_window(NULL);
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

#ifndef UNIX98
/* From the Xboard and screen packages. */
static int get_pty(char *pty_name)
{
    int i;
    int fd;

    for (i = 0; PTY_MAJOR[i]; i++) {
	int n;

	for (n = 0; PTY_MINOR[n]; n++) {
	    sprintf(pty_name, "%spty%c%c", _PATH_DEV, PTY_MAJOR[i], 
		    PTY_MINOR[n]);

	    if ((fd = open(pty_name, O_RDWR | O_NOCTTY)) == -1)
		continue;

	    sprintf(pty_name, "%stty%c%c", _PATH_DEV, PTY_MAJOR[i], 
		    PTY_MINOR[n]);

	    if (access(pty_name, R_OK | W_OK) == -1) {
		close(fd);
		continue;
	    }

	    return fd;
	}
    }

    return -1;
}
#endif

static char **parseargs(char *str)
{
    char **pptr, *s;
    char arg[255];
    int n = 0;
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
	    pptr = realloc(pptr, (n + 2) * sizeof(char *));
	    pptr[n++] = strdup(arg);
	    arg[0] = i = 0;
	    continue;
	}
	
	if ((i + 1) == sizeof(arg))
	    continue;

	arg[i++] = *s;
    }

    arg[i] = 0;

    if (arg[0]) {
	pptr = realloc(pptr, (n + 2) * sizeof(char *));
	pptr[n++] = strdup(arg);
    }

    pptr[n] = NULL;
    return pptr;
}

/* Is this dangerous if pty permissions are wrong? */
pid_t init_chess_engine(char **args)
{
    pid_t pid;
    int from[2], to[2];
#ifndef UNIX98
    char pty[FILENAME_MAX];

    if ((to[1] = get_pty(pty)) == -1) {
	errno = 0;
	return -1;
    }
#else
    if ((to[1] = open("/dev/ptmx", O_RDWR | O_NOCTTY)) == -1) {
	return -1;
    }

    if (grantpt(to[1]) == -1) {
	return -1;
    }

    if (unlockpt(to[1]) == -1) {
	return -1;
    }
#endif

    from[0] = to[1];
    errno = 0;

#ifndef UNIX98
    if ((to[0] = open(pty, O_RDWR | O_NOCTTY)) == -1)
#else
    if ((to[0] = open(ptsname(to[1]), O_RDWR | O_NOCTTY)) == -1)
#endif
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
    fcntl(enginefd[0], F_SETFL, O_NONBLOCK | O_DIRECT);
    fcntl(enginefd[1], F_SETFL, O_NONBLOCK | O_DIRECT);
    engine_initialized = 1;
    return pid;
}

void set_engine_defaults()
{
    if (status.engine == GNUCHESS)
	SEND_TO_ENGINE("book %s\n", book_method(config.book_method));

    SEND_TO_ENGINE("depth %i\n", config.engine_depth);
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
    int i;

    status.engine = ENGINE_INITIALIZING;
    //update_status_window();
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

    for (i = 0; args[i]; i++)
	free(args[i]);

    free(args);

    if (enginepid > 0)
	set_engine_defaults();

    //update_status_window();
    return enginepid;
}

/* Once the PGN parser has been well tested, validate_move() from the human
 * move can disappear.
 */
void parse_gnuchess_line(GAME *g, char *str)
{
    char m[MAX_SAN_MOVE_LEN + 1] = {0}, *p = m;
    int count;

    /* Human move. Add it to the move history. */
    if (sscanf(str, "%*d%*1[.]%*1[ ]%[a-zA-Z0-9+=#-]%n", m, &count) == 1) {
	if (validate_move(g, m)) {
	    invalid_move(0, m);
	    return;
	}

        if ((*g).htotal == 0 && (*g).side == BLACK)                   
	    SET_FLAG((*g).flags, GF_BLACK_OPENING);

	add_to_history(&(*g).hp, &(*g).hindex, &(*g).htotal, p);
	SET_FLAG((*g).flags, GF_MODIFIED);
	switch_turn(&(*g).turn);
	(*g).sp.icon = 0;
	str += count;
	status.engine = ENGINE_THINKING;
	return;
    }

    /* Engine move. */
    if (sscanf(str, "%*d%*1[.]%*1[ ]%*3[.]%*1[ ]%[a-zA-Z0-9+=#-]%n", m, 
		&count) == 1) {
	/* Moves from the engine are in a2a4 format (Xboard protocol) so we
	 * need to convert them.
	 */
	if ((p = a2a4tosan(g, m)) == NULL)
	    return;

	if (validate_move(g, p)) {
	    invalid_move(0, p);
	    return;
	}

        if ((*g).htotal == 0 && (*g).side == BLACK)                   
	    SET_FLAG((*g).flags, GF_BLACK_OPENING);

	add_to_history(&(*g).hp, &(*g).hindex, &(*g).htotal, p);
	SET_FLAG((*g).flags, GF_MODIFIED);
	switch_turn(&(*g).turn);
	str += count;

	if (TEST_FLAG((*g).flags, GF_GAMEOVER)) {
	    init_history(g);
	    RETURN;
	}

	RETURN;
    }

    if (TEST_FLAG((*g).flags, GF_GAMEOVER)) {
	init_history(g);
	RETURN;
    }

    /* Miscellaneous one-liners. */

    /* The engine is now reading a FIFO. Dump what we need to it. */
    // FIXME FEN
    if (strncmp(str, "pgnload ", 8) == 0) {
	if (save_pgn(config.fifo, 1, gindex)) {
	    (*g).htotal = oldhistorytotal;
	    oldhistorytotal = 0;
	    return;
	}
	else {
	    free_history_data((*g).hp, (*g).hindex + 1);
	    CLEAR_FLAG((*g).flags, GF_GAMEOVER);
	    SET_FLAG((*g).flags, GF_MODIFIED);
	}

	set_engine_defaults();
	return;
    }

    /* 'depth' command. */
    if (strncmp(str, "Search to a depth of ", 21) == 0) {
	str += 21;
	str = trim(str);
	config.engine_depth = atoi(str);
    }

    /* 'switch' command. */
    if (strncmp(str, "White to move", 13) == 0) {
	(*g).side = (*g).turn = WHITE;
	RETURN;
    }
    else if (strncmp(str, "Black to move", 13) == 0) {
	(*g).side = (*g).turn = BLACK;
	RETURN;
    }

    /* Bad engine command or move. */
    if (strncmp(str, "Illegal move: ", 14) == 0) {
	update_status_notify(*g, "%s", E_INVALID_COMMAND);
	(*g).sp.icon = 0;
	RETURN;
    }

    if (strcmp(str, "No book found.") == 0)
	config.book_method = -1; 
    else if (strcmp(str, "book now off.") == 0)
	config.book_method = BOOK_OFF;
    else if (strcmp(str, "book now on.") == 0)
	config.book_method = BOOK_PREFER;
    else if (strcmp(str, "book now best.") == 0)
	config.book_method = BOOK_BEST;
    else if (strcmp(str, "book now worst.") == 0)
	config.book_method = BOOK_WORST;
    else if (strcmp(str, "book now random.") == 0)
	config.book_method = BOOK_RANDOM;
}

static void parse_engine_line(GAME *g, char *line)
{
    line = trim(line);

    if (!*line)
	return;

    switch (config.engine) {
	case GNUCHESS:
	    parse_gnuchess_line(g, line);
	    break;
	default:
	    break;
    }
}

void parse_engine_output(GAME *g, char *str)
{
    char buf[LINE_MAX], *p = buf;

    while (*str) {
	*p = '\0';

	/* FIXME test this ("White ... : ", "Black ... : "). Needed for the
	 * 'g'o command. */
	if (status.engine == GNUCHESS && *str == ':') {
	    p = buf;
	    str++;
	    continue;
	}

	if (*str == '\n') {
	    *p = '\0';
	    parse_engine_line(g, buf);
	    str++;
	    p = buf;
	    continue;
	}

	*p++ = *str++;
    }
}
