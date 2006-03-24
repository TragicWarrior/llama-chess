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
#ifndef WINDOW_H
#define WINDOW_H

#define cmessage(title, prompt, args...)	\
    dump_message(title, prompt, 1, NULL, NULL, NULL, 0, ##args)

#define message(title, prompt, args...) \
    dump_message(title, prompt, 0, NULL, NULL, NULL, 0, ##args)

#define show_message(title, prompt, ehelp, func, arg, key, args...) \
    dump_message(title, prompt, 0, ehelp, func, arg, key, ##args)

#define get_input_str(title, init) \
    get_input(title, init, 1, 0, NULL, NULL, NULL, 0, -1, 20)

#define get_input_str_clear(title, init) \
    get_input(title, init, 1, 1, NULL, NULL, NULL, 0, -1, 20)

int dump_message(const char *, const char *, int, const char *, 
	void (*)(void *), void *, int, const char *, ...);

#define CALCPOSY(y)		((y > LINES - 1) ? 0 : LINES / 2 - y / 2)
#define CALCPOSX(x)		(COLS / 2 - x / 2)
#define CENTERX(x, str)		(x / 2 - strlen(str) / 2)

#endif
