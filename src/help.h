/* $Id: help.h,v 1.1 2002-12-05 20:38:47 bjk Exp $ */
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
const char *help_title = "Command Keys";

const char *helptext[] = {
    "UP/j    - cursor up              N - new game",
    "DOWN/k  - cursor down            R - refresh screen",
    "LEFT/l  - cursor left/reverse    c - send a command to the game engine",
    "RIGHT/; - cursor right/forward   v - version information",
    "                                 q - quit",
    "SPACE   - select piece           w - switch sides",
    "ENTER   - move selected piece    s - save game",
    "ESC     - cancel selected piece  r - resume a previously saved game",
    "                                 u - take back previous move",
    "                                 h - toggle movement history",
    "                                 g - force engine to make next move",
    "                                 b - cycle through book modes",
    "                                 i - PGN information"
};

void draw_window_title(WINDOW *, const char *, int);
