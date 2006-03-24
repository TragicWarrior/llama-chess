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
#ifdef DEBUG
#ifndef DEBUG_H
#define DEBUG_H

#define DUMP(fmt, args...)	(write_debug_output(0, fmt, ## args))
#define DUMP_F(fmt, args...)	(write_debug_output(1, fmt, ## args))

unsigned gcurrent;

void write_debug_output(int, const char *, ...);

#ifdef CHESS_H
void dump_board(int which, BOARD b);
#endif

#ifdef WINDOW_H
#define dmsg(...) cmessage("DEBUG", ANYKEY, __VA_ARGS__)
#endif

#endif
#endif
