/* $Id: strings.h,v 1.1 2003-01-08 14:21:14 bjk Exp $ */
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
#ifndef STRINGS_H
#define STRINGS_H

#define SAVE_HISTORY_P	"Type 'a' or 'c' or ESCAPE to cancel"
#define SAVE_HISTORY    "You are in history mode. You can save all moves up " \
    "to and including the current move by pressing 'c', or the whole game " \
    "history by pressing 'a'."
#define RESUME_HISTORY	"Resume game from history?"

#define SAVE_PGN_P	"Edit PGN tags?"
#define NEWGAME_P	"Really start a new game?"
#define NEWGAME		"Use the 'N' command to start a new game or the 'r' " \
    "command to load a previous game"

#define E_AGONY		"Could not open agony data file."
#define E_A2A4_PARSE	"Parse error. Probably a bug."
#define E_PGN_PARSE	"Parse error."
#define E_SAVE_NOMOVE	"No moves to save."
#define E_BROKEN_PIPE	"Broken pipe. Quitting."
#define E_NOTADIR	"not a directory"
#define E_INITCURSES	"Could not initialize curses."

#define NOTIFY_SAVED	"Game saved."

#endif
