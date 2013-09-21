/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2013 Ben Kibbey <bjk@luxsci.net>

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
#ifndef DEBUG_H
#define DEBUG_H

#define DUMP(fmt, args...)	write_debug_output(NULL, fmt, ## args)
#define DUMP_F(file, fmt, args...)	write_debug_output(file, fmt, ## args)

void write_debug_output(const char *, const char *, ...);
char *debug_board(BOARD b);
void dump_board(const char *, BOARD b);

#endif
