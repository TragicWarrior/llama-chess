/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2013 Ben Kibbey <bjk@luxsci.net>

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
#ifndef COMMON_H
#define COMMON_H

#ifndef _
#include "gettext.h"
#define _(msgid) gettext(msgid)
#endif

#define CF_ENGINE_LOOP	0x01
#define CF_HUMAN	0x02
#define CF_NEW		0x04
#define CF_CLOCK	0x08
#define CF_MODIFIED	0x10
#define CF_DELETE	0x20
#ifdef WITH_LIBPERL
#define CF_PERL		0x40
#endif

#define MAX_TC		8	/* Time controls. */

struct clock_s {
    struct timeval elapsed;
    unsigned short move;	/* move count */
    int tc[MAX_TC][2];		/* 0 = move count, 1 = time (in seconds) */
    int tcn;
    int incr;
};

/*
 * Attached to game[n].data.
 */
struct userdata_s {
    BOARD b;
    struct engine_s *engine;
    unsigned short flags;
    char c_row;
    char c_col;
    char paused;
    unsigned n;
    unsigned char mode;
    struct clock_s wclock;
    struct clock_s bclock;
    struct timeval elapsed;

    // The selected piece.
    struct {
	unsigned char icon;
	char scol;
	char srow;
	char col;
	char row;
    } sp;

    void *data; // For the history menu

#ifdef WITH_LIBPERL
    char *perlfen;
    char *oldfen;
    unsigned short perlflags;
#endif
};

/* A pointer to the game in focus. */
GAME gp;

void gameover(GAME);
void update_cursor(GAME, int);
void invalid_move(int n, int e, const char *m);
void update_status_window(GAME g);
void update_all(GAME g);

#endif
