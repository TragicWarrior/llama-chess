/* $Id: strings.h,v 1.8 2003-01-14 20:44:15 bjk Exp $ */
/*
    Copyright (C) 2002-2003 Ben Kibbey <bjk@arbornet.org>

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

#define SAVE_MULTIGAME_P	"Type 'c' or 'a' or ESCAPE to cancel"
#define SAVE_MULTIGAME	"There is more than one game loaded. You can save " \
    "only the current game by pressing 'c', or all games by pressing 'a'."
#define SAVE_HISTORY_P	"Type 'a' or 'c' or ESCAPE to cancel"
#define SAVE_HISTORY    "You are in history mode. You can save all moves up " \
    "to and including the current move by pressing 'c', or the whole game " \
    "history by pressing 'a'."
#define RESUME_HISTORY	"Resuming a game from previous history will remove " \
    "all future moves. Do you really want to resume from history?"
#define DELETE_GAME	"Really delete game"

#define SAVE_PGN_P	"Edit PGN tags?"
#define NEWGAME_P	"Really start a new game from scratch?"
#define NEWGAME		"Use the 'N' command to start a new game or the 'r' " \
    "command to load a previous game"

#define E_DELETE_GAME	"Cannot delete last game."
#define E_AGONY		"Could not open agony data file."
#define E_A2A4_PARSE	"Parse error. Probably a bug."
#define E_PGN_PARSE	"Parse error."
#define E_BROKEN_PIPE	"Broken pipe. Quitting."
#define E_NOTADIR	"not a directory"
#define E_INITCURSES	"Could not initialize curses."
#define E_SAVE_NOMOVES	"Refusing to save null move game"
#define E_SAVE_NOGMOVES	"No games contain any moves. Aborting save."
#define E_SAVE_COMPRESS	"Cannot append to compressed file."
#define E_INVALID_MOVE	"Invalid move"
#define E_SELECT_TURN	"It is not your turn to move. You can switch sides " \
    "by pressing 'w' or force the engine to make the next move by " \
    "pressing 'g'."

#define NOTIFY_SAVED	"Game saved."
#define NOTIFY_CHECK	"Check!"
#define NOTIFY_ENPASSANT	"En Passant"
#define NOTIFY_PROMOTION	"Promotion!"
#define NOTIFY_BCASTLEQ	"Black castles queen side"
#define NOTIFY_BCASTLEK	"Black castles king side"
#define NOTIFY_WCASTLEQ	"White castles queen side"
#define NOTIFY_WCASTLEK	"White castles king side"

#define UNAVAILABLE	"not available"

#endif
