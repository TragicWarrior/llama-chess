/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2007 Ben Kibbey <bjk@luxsci.net>

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
#include <err.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_PATHS_H
#include <paths.h>
#endif

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_CURSES_H
#include <curses.h>
#endif

#ifdef HAVE_PANEL_H
#include <panel.h>
#endif

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#include "chess.h"
#include "conf.h"
#include "misc.h"
#include "strings.h"
#include "window.h"
#include "message.h"
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

int send_to_engine(GAME g, int status, const char *format, ...)
{
    va_list ap;
    int len;
    char *line;
    int try = 0;
    struct userdata_s *d = g->data;

    if (!d->engine || d->engine->status == ENGINE_OFFLINE || 
	    TEST_FLAG(d->flags, CF_HUMAN))
	return 1;

    if (send_signal_to_engine(d->engine->pid, SIGINT))
	return 1;

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
			return 1;
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

    d->engine->status = status;
    free(line);
    return 0;
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

int init_chess_engine(GAME g)
{
    struct userdata_s *d = g->data;
    int w, x;

    if (start_chess_engine(g) > 0) {
	d->sp.icon = 0;
	return 1;
    }

    x = pgn_tag_find(g->tag, "FEN");
    w = pgn_tag_find(g->tag, "SetUp");

    if ((w >= 0 && x >= 0 && atoi(g->tag[w]->value) == 1) || 
	    (x >= 0 && w == -1))
	add_engine_command(g, ENGINE_READY, "setboard %s\n", g->tag[x]->value);
    else
	add_engine_command(g, ENGINE_READY, "setboard %s\n",
		pgn_game_to_fen(g, d->b));

    return 0;
}

/* Is this dangerous if pty permissions are wrong? */
static pid_t exec_chess_engine(GAME g, char **args)
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

    /*
     * FIXME shouldn't block here. When theres a high load average, the engine
     * process will fail to start. Better to handle this in game_loop() and
     * give up after a timer expires.
     */
    sleep(1);

    if (send_signal_to_engine(pid, 0))
	return -2;

    close(to[0]);
    close(from[1]);

    d->engine->fd[ENGINE_IN_FD] = from[0];
    d->engine->fd[ENGINE_OUT_FD] = to[1];
    fcntl(d->engine->fd[ENGINE_IN_FD], F_SETFL, O_NONBLOCK);
    fcntl(d->engine->fd[ENGINE_OUT_FD], F_SETFL, O_NONBLOCK);
    d->engine->pid = pid;
    return 0;
}

void stop_engine(GAME g)
{
    struct userdata_s *d = g->data;
    int s;

    if (!d->engine || d->engine->pid == -1 || d->engine->status == ENGINE_OFFLINE)
	return;

    send_to_engine(g, ENGINE_READY, "quit\n");

    if (!send_signal_to_engine(d->engine->pid, 0)) {
	if (!send_signal_to_engine(d->engine->pid, SIGTERM))
	    send_signal_to_engine(d->engine->pid, SIGKILL);
    }

    waitpid(d->engine->pid, &s, 0);
    d->engine->pid = -1;
    d->engine->status = ENGINE_OFFLINE;
}

void set_engine_defaults(GAME g, char **init)
{
    int i;

    if (!init)
	return;

    for (i = 0; init[i]; i++)
	add_engine_command(g, ENGINE_READY, "%s\n", init[i]);
}

int start_chess_engine(GAME g)
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
    update_status_window(g);
    refresh_all();

    switch (exec_chess_engine(g, args)) {
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
    update_status_window(g);
    return ret;
}

static void parse_xboard_line(GAME g, char *str)
{
    char m[MAX_SAN_MOVE_LEN + 1] = {0}, *p = m;
    struct userdata_s *d = g->data;
    int n;
    char *frfr;
    int count;

    p = str;

    // 1. a2a4 (gnuchess)
    while (isdigit(*p))
	p++;

    // gnuchess echos our input move so we don't need the duplicate (above).
    if (*p == '.' && *(p + 1) == ' ' && *(p + 2) != '.')
	return;

    // 1. ... a2a4 (gnuchess/engine move)
    if (*p == '.' && *(p + 1) == ' ' && *(p + 2) == '.') {
	while (*p == ' ' || *p == '.')
	    p++;
    }

    if (strncmp(str, "move ", 5) == 0)
	p = str + 5;

    if (strlen(p) > MAX_SAN_MOVE_LEN)
	return;

    // We should now have the real move which may be in SAN or frfr format.
    if (sscanf(p, "%[0-9a-hprnqkxPRNBQKO+=#-]%n", m, &count) == 1) {
	/* 
	 * For engine commands (the '|' key). Don't try and validate them.
	 */
	if (count != strlen(p))
	    return;

	if (TEST_FLAG(g->flags, GF_GAMEOVER)) {
	    gameover(g);
	    return;
	}

	if (strlen(m) < 2)
	    RETURN(d);

	p = m + strlen(m) - 1;

	// Test SAN or a2a4 format.
	if (!isdigit(*p) && *p != 'O' && *p != '+' && *p != '#' &&
		((*(p - 1) != '=' || !isdigit(*(p - 1))) && 
		 pgn_piece_to_int(*p) == -1))
	    return;

	p = m;

	if ((n = pgn_parse_move(g, d->b, &p, &frfr)) != E_PGN_OK) {
	    invalid_move(d->n + 1, n, m);
	    RETURN(d);
	}

	pgn_history_add(g, p);
	SET_FLAG(d->flags, CF_MODIFIED);
	pgn_switch_turn(g);

	if (TEST_FLAG(d->flags, CF_ENGINE_LOOP)) {
	    update_cursor(g, g->hindex);

	    if (TEST_FLAG(g->flags, GF_GAMEOVER)) {
		gameover(g);
		return;
	    }

	    add_engine_command(g, ENGINE_THINKING, "go\n");
	    return;
	}

	if (g->side == g->turn)
	    RETURN(d);

	d->engine->status = ENGINE_THINKING;
    }

    return;
}

void parse_engine_output(GAME g, char *str)
{
    char buf[255], *p = buf;

    while (*str) {
	if (*str == '\n' || *str == '\r') {
	    *p = '\0';

	    if (*buf) {
		append_enginebuf(g, buf);
		parse_xboard_line(g, buf);
	    }

	    str++;

	    if (*str == '\n')
		str++;

	    p = buf;
	    continue;
	}

	*p++ = *str++;
    }
}

void send_engine_command(GAME g)
{
    struct userdata_s *d = g->data;
    struct queue_s **q = d->engine->queue;
    int i;

    if (!d->engine || !d->engine->queue)
	return;

    if (!q || !q[0])
	return;

    if (send_to_engine(g, q[0]->status, "%s", q[0]->line) == 0) {
	if (g == gp)
	    update_status_window(g);
    }

    free(q[0]->line);
    free(q[0]);

    for (i = 0; q[i]; i++) {
	if (q[i+1])
	    q[i] = q[i+1];
	else {
	    q[i] = NULL;
	    break;
	}
    }
}

void add_engine_command(GAME g, int s, char *fmt, ...)
{
    va_list ap;
    int i = 0;
    struct userdata_s *d = g->data;
    struct queue_s **q;
    char *line;

    if (!d->engine || d->engine->status == ENGINE_OFFLINE) {
	if (init_chess_engine(g))
	    return;
    }

    q = d->engine->queue;

    if (q)
	for (i = 0; q[i]; i++);

    q = Realloc(q, (i + 2) * sizeof(struct queue_s *));
    va_start(ap, fmt);
#ifdef HAVE_VASPRINTF
    vasprintf(&line, fmt, ap);
#else
    line = Malloc(LINE_MAX + 1);
    vsnprintf(line, LINE_MAX, fmt, ap);
#endif
    va_end(ap);
    q[i] = Malloc(sizeof(struct queue_s));
    q[i]->line = line;
    q[i++]->status = (s == -1) ? d->engine->status : s;
    q[i] = NULL;
    d->engine->queue = q;
}
