/* $Id: input.h,v 1.3 2002-12-10 22:17:01 bjk Exp $ */
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
#define INPUT_WIDTH	((COLS > 60) ? 60 : COLS - 2)
#define INPUT_HELP_PROMPT	"Type ^G for available line editing keys"
#define INPUT_HELP	"Line Editing Keys"

const char *inputhelp[] = {
    "blah",
    NULL
};

char *trim(char *);
