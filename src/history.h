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
#ifndef HISTORY_H
#define HISTORY_H

struct nag_s {
    char *line;
} *nags;

const char *naghelp[] = {
    "    UP/DOWN - previous/next item",
    " LEFT/RIGHT - previous/next selected item",
    "       HOME - first item",
    "        END - last item",
    "CTRL-p/PGUP - previous page",
    "CTRL-n/PGDN - next page",
    "  a-zA-Z0-9 - jump to item",
    "      SPACE - toggle current item",
    "      ENTER - quit with changes",
    "     ESCAPE - quit without changes",
    NULL
};

void init_board(BOARD);
void update_status_window(void);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);
void help(const char *, const char *, const char **);
int parse_move_text(GAME, BOARD, char *);
void switch_turn(GAME *);
int parse_fen_line(GAME *, BOARD, char *);
void update_all(GAME);
void update_status_notify(GAME, char *, ...);
void invalid_move(int, const char *);

#endif
