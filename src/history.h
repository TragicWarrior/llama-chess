/* $Id: history.h,v 1.10 2002-12-20 00:31:37 bjk Exp $ */
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
#ifndef HISTORY_H
#define HISTORY_H
#define NAG_TITLE	"Editing NAG Information"
#define NAG_PROMPT	"Type CTRL-g for help"
#define NAG_HELP	"NAG Menu Keys"
#define VIEW_ANNOTATION	"Viewing Annotation for"

char **agony;

struct nags {
    char line[80];
} *nag;

const char *naghelp[] = {
    "[a-zA-Z0-9] - jump to item",
    "         UP - previous item",
    "       DOWN - next item",
    "       LEFT - previous selected item",
    "      RIGHT - next selected item",
    "     CTRL-p - previous page",
    "     CTRL-n - next page",
    "      SPACE - toggle current item",
    "      ENTER - quit with changes",
    "     ESCAPE - quit without changes",
    NULL
};

void init_board(void);
void update_status(void);
int show_message(const char *, const char *, const char *, void(*)(void*),
	void *, int, const char *, va_list);
#endif
