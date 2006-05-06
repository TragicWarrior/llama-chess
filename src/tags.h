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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef TAGS_H
#define TAGS_H

static const char *view_help = {
    "    UP/DOWN - previous/next menu item\n" \
    "   HOME/END - first/last menu item\n" \
    "  PGDN/PGUP - next/previous page\n" \
    "  a-zA-Z0-9 - jump to item\n" \
    "      ENTER - view selected item\n" \
    "     ESCAPE - cancel"
};

static const char *edit_help = {
    "    UP/DOWN - previous/next menu item\n" \
    "   HOME/END - first/last menu item\n" \
    "  PGDN/PGUP - next/previous page\n" \
    "  a-zA-Z0-9 - jump to item\n" \
    "      ENTER - edit select item\n" \
    "     CTRL-a - add an entry\n" \
    "     CTRL-f - add FEN tag from current position\n" \
    "     CTRL-r - remove selected entry\n" \
    "     CTRL-t - add custom tags\n" \
    "     CTRL-x - quit with changes\n" \
    "     ESCAPE - quit without changes"
};

static const char *cc_help = {
    "    UP/DOWN - previous/next menu item\n" \
    "   HOME/END - first/last menu item\n" \
    "  PGDN/PGUP - next/previous page\n" \
    "  a-zA-Z0-9 - jump to item\n" \
    "      ENTER - select item\n" \
    "     ESCAPE - cancel"
};

static struct country_codes {
    char code[4];
    char country[64];
} *ccodes;

void update_status_notify(GAME g, char *fmt, ...);

#endif
