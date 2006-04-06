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

#include "chess.h"
#include "conf.h"
#include "misc.h"
#include "strings.h"
#include "window.h"
#include "common.h"
#include "engine.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

int send_signal_to_engine(pid_t pid, int sig)
{
    if (kill(pid, sig) == -1)
	return 1;

    return 0;
}

void send_to_engine(GAME *g, const char *format, ...)
{
    va_list ap;
    int len;
    char *line;
    int try = 0;
    struct userdata_s *d = g->data;

    if (!d->engine || d->engine->status == ENGINE_OFFLINE || 
	    TEST_FLAG(d->flags, CF_HUMAN))
	return;

    if (send_signal_to_engine(d->engine->pid, SIGINT))
	return;

    d->engine->status = ENGINE_THINKING;
    update_status_window(*g);
    refresh_all();

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
	FD_SET(d->engine->fd[ENGINE_OUT_FD], &fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if ((n = select(d->engine->fd[ENGINE_OUT_FD] + 1, NULL, &fds, NULL,
			&tv)) > 0) {
	    if (FD_ISSET(d->engine->fd[ENGINE_OUT_FD], &fds)) {
		n = write(d->engine->fd[ENGINE_OUT_FD], line, len);

		if (n == -1) {
		    if (errno == EAGAIN)
			continue;

		    if (kill(d->engine->pid, 0) == -1) {
			message(ERROR, ANYKEY, "Could not write to engine. "
				"Process no longer exists.");
			d->engine->status = ENGINE_OFFLINE;
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
static pid_t init_chess_engine(GAME *g, char **args)
{
    pid_t pid;
    int from[2], to[2];
    struct userdata_s *d = g->data;
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
    if ((to[0] = open(pty, O_RDWR, 0)) == -1)
#else
    if ((to[0] = open(ptsname(to[1]), O_RDWR, 0)) == -1)
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

    d->engine->fd[ENGINE_IN_FD] = from[0];
    d->engine->fd[ENGINE_OUT_FD] = to[1];
    fcntl(d->engine->fd[ENGINE_IN_FD], F_SETFL, O_NONBLOCK | O_DIRECT);
    fcntl(d->engine->fd[ENGINE_OUT_FD], F_SETFL, O_NONBLOCK | O_DIRECT);
    d->engine->pid = pid;
    return 0;
}

void stop_engine(GAME *g)
{
    struct userdata_s *d = g->data;
    int s;

    if (!d->engine || d->engine->status == ENGINE_OFFLINE)
	return;

    send_to_engine(g, "quit\n");

    if (!send_signal_to_engine(d->engine->pid, 0)) {
	if (!send_signal_to_engine(d->engine->pid, SIGTERM))
	    send_signal_to_engine(d->engine->pid, SIGKILL);
    }

    waitpid(d->engine->pid, &s, 0);
}

void set_engine_defaults(GAME *g, char **init)
{
    int i;

    if (!init)
	return;

    for (i = 0; init[i]; i++)
	send_to_engine(g, "%s\n", init[i]);
}

int start_chess_engine(GAME *g)
{
    char **args;
    int i;
    int ret = 1;
    struct userdata_s *d = g->data;

    if (d->engine)
	return -1;

    args = parseargs(config.engine_cmd);

    d->engine = Calloc(1, sizeof(struct engine_s));
    d->engine->status = ENGINE_INITIALIZING;
    update_status_window(*g);
    refresh_all();

    switch (init_chess_engine(g, args)) {
	case -1:
	    /* Pty allocation. */
	    message(ERROR, ANYKEY, "Could not allocate PTY");
	    d->engine->status = ENGINE_OFFLINE;
	    break;
	case -2:
	    /* Could not execute engine. */
	    message(ERROR, ANYKEY, "%s: %s", args[0], strerror(errno));
	    d->engine->status = ENGINE_OFFLINE;
	    break;
	default:
	    ret = 0;
	    set_engine_defaults(g, config.einit);
	    d->engine->status = ENGINE_READY;
	    break;
    }

    for (i = 0; args[i]; i++)
	free(args[i]);

    free(args);
    update_status_window(*g);
    return ret;
}

/* Once the PGN parser has been well tested, validate_move() from the human
 * move can disappear.
 */
void parse_gnuchess_line(GAME *g, char *str)
{
    char m[MAX_SAN_MOVE_LEN + 1] = {0}, *p = m;
    int count;
    struct userdata_s *d = g->data;

    /* Human move. Add it to the move history. */
    if (sscanf(str, "%*d%*1[.]%*1[ ]%[a-zA-Z0-9+=#-]%n", m, &count) == 1) {
	/*
	if (pgn_validate_move(g, g->b, p)) {
	    invalid_move(0, m);
	    RETURN(d);
	}
	*/

        if (pgn_history_total(g->hp) == 0 && g->side == BLACK)
	    SET_FLAG(g->flags, GF_BLACK_OPENING);

	pgn_history_add(g, p);
	SET_FLAG(g->flags, GF_MODIFIED);
	pgn_switch_turn(g);
	str += count;
	return;
    }

    /* Engine move. */
    if (sscanf(str, "%*d%*1[.]%*1[ ]%*3[.]%*1[ ]%[a-zA-Z0-9+=#-]%n", m, 
		&count) == 1) {
	/* Moves from the engine are in a2a4 format (Xboard protocol) so we
	 * need to convert them.
	 */

	p = m;

	if (pgn_validate_move(g, g->b, &p)) {
	    invalid_move(0, m);
	    RETURN(d);
	}

        if (pgn_history_total(g->hp) == 0 && g->side == BLACK)
	    SET_FLAG(g->flags, GF_BLACK_OPENING);

	pgn_history_add(g, p);
	SET_FLAG(g->flags, GF_MODIFIED);
	pgn_switch_turn(g);
	str += count;

	if (TEST_FLAG(g->flags, GF_GAMEOVER)) {
	    //pgn_board_update(g, g.htotal); FIXME
	    RETURN(d);
	}

	if (TEST_FLAG(d->flags, CF_ENGINE_LOOP) && 
		!TEST_FLAG(d->flags, CF_HUMAN)) {
	    update_cursor(*g, g->hindex);
	    send_to_engine(g, "go\n");
	    return;
	}

	RETURN(d);
    }

    if (TEST_FLAG(g->flags, GF_GAMEOVER)) {
	//pgn_board_update(g); FIXME
	RETURN(d);
    }

    /* Miscellaneous one-liners. */
    /* 'switch' command. */
    if (strncmp(str, "White to move", 13) == 0) {
	g->side = g->turn = WHITE;
	RETURN(d);
    }
    else if (strncmp(str, "Black to move", 13) == 0) {
	g->side = g->turn = BLACK;
	RETURN(d);
    }

    /* Bad engine command or move. */
    if (strncmp(str, "Illegal move: ", 14) == 0) {
	invalid_move(0, p);
	RETURN(d);
    }
}

static void parse_engine_line(GAME *g, char *line)
{
    line = trim(line);

    if (!*line)
	return;

    parse_gnuchess_line(g, line);
}

void parse_engine_output(GAME *g, char *str)
{
    char buf[LINE_MAX], *p = buf;

    while (*str) {
	*p = '\0';

	/* FIXME test this ("White ... : ", "Black ... : "). Needed for the
	 * 'g'o command. */
	if (*str == ':') {
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
