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
#ifndef ENGINE_H
#define ENGINE_H

#if 0
#define RETURN		status.engine = ENGINE_READY; \
			return
#endif
#define RETURN return

#define SEND_TO_ENGINE(fmt, args...)	(engine_initialized) ? \
    send_to_engine(fmt, ##args) : 0

enum {
    ENGINE_OFFLINE = -1, ENGINE_READY, ENGINE_THINKING, ENGINE_INITIALIZING
};

enum { 
    HUMAN, ENGINE
};

pid_t enginepid;

/* Chess engine file descriptors. 0 = from, 1 = to. */
int enginefd[2];
int engine_initialized;

// This is a failsafe when resuming a game.
int oldhistorytotal;

void send_to_engine(const char *format, ...);
int start_chess_engine();
void set_engine_defaults();
void stop_engine();
int save_pgn(const char *filename, int isfifo, int saveindex);

#endif
