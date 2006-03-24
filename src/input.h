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
#ifndef INPUT_H
#define INPUT_H

#define INPUT_WIDTH	((COLS > 60) ? 60 : COLS - 2)
#define CTRL(x)			((x) & 0x1f)
#define KEY_ESCAPE		CTRL('[')

enum {
    FIELD_TYPE_ALNUM, FIELD_TYPE_ALPHA, FIELD_TYPE_INTEGER,
    FIELD_TYPE_NUMERIC, FIELD_TYPE_REGEXP, FIELD_TYPE_IPV4, FIELD_TYPE_ENUM,
    FIELD_TYPE_PGN_TAG_NAME, FIELD_TYPE_PGN_DATE, FIELD_TYPE_PGN_ROUND
};

void draw_window_title(WINDOW *, const char *, int, chtype, chtype);
void draw_prompt(WINDOW *win, int, int, const char *, chtype);
int help(const char *, const char *, const char **);
char *get_input(const char *title, const char *init, int lines, int reset,
	const char *extra_help, char *(*custom_func)(void *), void *arg, 
	chtype ckey, int type, ...);

#endif
