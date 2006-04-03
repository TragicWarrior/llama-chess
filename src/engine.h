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

#define RETURN(d)	d->status = ENGINE_READY;\
				    return

// This is a failsafe when resuming a game.
int oldhistorytotal;

void send_to_engine(GAME *g, const char *format, ...);
int start_chess_engine(GAME *);
void set_engine_defaults(GAME *, char **);
void stop_engine(GAME *);
int save_pgn(const char *filename, int isfifo, int saveindex);
void update_cursor(GAME, int);
void refresh_all(void);

#endif
