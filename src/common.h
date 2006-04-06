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
#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

enum {
    ENGINE_OFFLINE = -1, ENGINE_READY, ENGINE_THINKING, ENGINE_INITIALIZING
};

enum { 
    HUMAN, ENGINE
};

enum {
    ENGINE_IN_FD,
    ENGINE_OUT_FD,
};

#define CF_ENGINE_LOOP	0x01
#define CF_HUMAN	0x02

struct engine_s {
    int fd[2];
    pid_t pid;
    int status;
};

/*
 * Attached to game[n].data.
 */
struct userdata_s {
    struct engine_s *engine;
    unsigned short flags;
};

char **enginebuf;

void invalid_move(int n, const char *m);
void update_status_window(GAME g);

#endif
