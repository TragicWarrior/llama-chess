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

#define RETURN		status.engine = ENGINE_READY; \
			return

enum { 
    HUMAN, ENGINE
};

pid_t enginepid;

void add_to_history(HISTORY **, unsigned char *, unsigned char *, const char *);
char *parse_piece(char *);
void move_piece(char *);
int save_pgn(const char *, int, int);
void update_status_window(void);
char *book_method(int);
int validate_move(GAME *, char *);
char *a2a4tosan(GAME *, char *);
void switch_turn(char *);
void init_history(GAME *);
void free_history_data(HISTORY *, int);
void update_status_notify(GAME, char *, ...);
void invalid_move(int, const char *);

#endif
