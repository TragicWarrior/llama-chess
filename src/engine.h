/* $Id: engine.h,v 1.8 2003-01-06 19:59:15 bjk Exp $ */
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
#ifndef ENGINE_H
#define ENGINE_H
#define FIRST_PTY_LETTER	'p'
#define LAST_PTY_LETTER		'z'

#define RETURN		status.engine = ENGINE_READY; \
			return

enum { 
    HUMAN, ENGINE
};

pid_t enginepid;

void add_to_history(struct history **, int *, int *, const char *);
char *parse_piece(char *);
void move_piece(char *);
int save_pgn(const char *, struct pgndata *, int);
void update_status();
char *book_method(int);
int parse_move_text(struct board_matrix [][], char *);

#endif
