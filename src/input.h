/* $Id: input.h,v 1.7 2002-12-21 21:32:17 bjk Exp $ */
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
#ifndef INPUT_H
#define INPUT_H

#define INPUT_WIDTH	((COLS > 60) ? 60 : COLS - 2)
#define INPUT_HELP_PROMPT	"Type CTRL-g for available line editing keys"
#define INPUT_HELP	"Line Editing Keys"

const char *inputhelp[] = {
    "UP/DOWN/LEFT/RIGHT - position cursor",
    "            CTRL-A - move cursor to the beginning of line",
    "            CTRL-E - move cursor to the end of line",
    "            CTRL-B - move cursor to previous word",
    "            CTRL-W - move cursor to next word",
    "            CTRL-X - delete word under cursor",
    "            CTRL-K - delete from cursor to end of line",
    "            CTRL-U - clear entire input field",
    "         BACKSPACE - delete previous character",
    "            ESCAPE - quit without changes",
    "             ENTER - quit with changes",
    NULL
};

void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);
void help(const char *, const char **);

#endif
