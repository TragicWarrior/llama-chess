/* $Id: history.h,v 1.17 2003-01-09 18:46:35 bjk Exp $ */
/*
    Copyright (C) 2002-2003 Ben Kibbey <bjk@arbornet.org>

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

#define NAG_TITLE	"Editing NAG Information"
#define NAG_PROMPT	"Type CTRL-g for help"
#define NAG_HELP	"NAG Menu Keys"
#define VIEW_ANNOTATION	"Viewing Annotation for"

struct nags {
    char line[80];
} *nag;

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

void init_board(struct board_matrix[][]);
void update_status(void);
void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);
void help(const char *, const char **);
int parse_move_text(struct board_matrix [][], char *, int);

#endif
