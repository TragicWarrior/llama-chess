/* $Id: strings.h,v 1.12 2003-01-27 16:55:16 bjk Exp $ */
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
/*
 * The actual help text strings are found in the other header files.
 */
#ifndef STRINGS_H
#define STRINGS_H

#define GAME_JUMP_TITLE			"Jump to Game Number"
#define GAME_HISTORY_JUMP_TITLE		"Jump to Move Number"
#define GAME_NOTSAVED			"*delete*"
#define GAME_SAVE_OVERWRITE_PROMPT	"'a' to append, 'o' to overwrite"
#define GAME_EDIT_TAG_PROMPT		"Edit roster tags?"
#define GAME_SAVE_MULTI_PROMPT		"Type 'c' or 'a' or ESCAPE to cancel"
#define GAME_SAVE_MULTI_TEXT		"There is more than one game " \
    "loaded. You can save only the current game by pressing 'c', or all " \
    "games by pressing 'a'."
#define GAME_SAVE_HISTORY_PROMPT	"Type 'a' or 'c' or ESCAPE to cancel"
#define GAME_SAVE_HISTORY_TEXT		"You are in history mode. You can " \
    "save all moves up to and including the current move by pressing 'c', " \
    "or the whole game history by pressing 'a'."
#define GAME_RESUME_HISTORY_TEXT	"Resuming a game from previous " \
    "history will remove all future moves. Do you really want to resume " \
    "from history?"
#define GAME_DELETE_ALL_TEXT		"Delete all games marked for deletion?"
#define GAME_DELETE_GAME_TEXT		"Delete the current game?"
#define GAME_NEW_PROMPT	"Really start a new game from scratch?"
#define GAME_NEW_TEXT	"Use the 'N' command to start a new game or the 'r' " \
    "command to load a previous game"
#define GAME_HELP	"Command Keys"
#define GAME_LOAD_TITLE	"Load Filename"
#define GAME_SAVE_TITLE	"Save Game Filename"

/* Error strings. */
#define E_RESUME_BLACK	"Cannot resume a game with black starting position " \
    "(yet)."
#define E_HOME_ENV	"The HOME environment variable is unset."
#define E_TAG_DATE_FMT	"The \"Date\" tag must be in YYYY.MM.DD format."
#define E_FILEEXISTS	"File exists:"
#define E_REMOVE_STR	"Cannot remove the Seven Tag Roster"
#define E_DUPLICATE_TAG	"Could not add duplicate tag"
#define E_DELETE_GAME	"Cannot delete last game."
#define E_AGONY		"Could not open agony data file."
#define E_CCODE_FILE	"Could not open country code data file."
#define E_A2A4_PARSE	"Parse error. Probably a bug."
#define E_PGN_PARSE	"Parse error."
#define E_BROKEN_PIPE	"Broken pipe. Quitting."
#define E_NOTADIR	"Not a directory."
#define E_NOTAREGFILE	"Not a regular file."
#define E_INITCURSES	"Could not initialize curses."
#define E_SAVE_NOMOVES	"Refusing to save null move game"
#define E_SAVE_NOGMOVES	"No games contain any moves. Aborting save."
#define E_SAVE_COMPRESS	"Cannot append to compressed file."
#define E_INVALID_MOVE	"Invalid move"
#define E_SELECT_TURN	"It is not your turn to move. You can switch sides " \
    "by pressing 'w' or force the engine to make the next move by " \
    "pressing 'g'."

/* The notification line in the status window. */
#define NOTIFY_SAVED		"Game saved."
#define NOTIFY_CHECK		"Check!"
#define NOTIFY_CHECKMATE	"Checkmate. Game over!"
#define NOTIFY_ENPASSANT	"En Passant"
#define NOTIFY_PROMOTION	"Promotion!"
#define NOTIFY_BCASTLEQ		"Black castles queen side"
#define NOTIFY_BCASTLEK		"Black castles king side"
#define NOTIFY_WCASTLEQ		"White castles queen side"
#define NOTIFY_WCASTLEK		"White castles king side"

/* Pawn promotion window. */
#define PROMOTION_TITLE		"Select Pawn Promotion Piece"
#define PROMOTION_PROMPT	"R/N/B/Q"
#define PROMOTION_TEXT		"R = Rook, N = Knight, B = Bishop, Q = Queen"

/* Book methods ('b' command). These are sent to the engine (GNU Chess). */
#define BOOK_OFF_STR	"off"
#define BOOK_PREFER_STR	"prefer"
#define BOOK_BEST_STR	"best"
#define BOOK_WORST_STR	"worst"
#define BOOK_RANDOM_STR	"random"

/* Window titles. */
#define HISTORY_WINDOW_TITLE	"Move History"
#define TAG_WINDOW_TITLE	"Seven Tag Roster"
#define STATUS_WINDOW_TITLE	"Game Status"

/* Annotation viewing and editing windows. */
#define ANNOTATION_VIEW_TITLE	"Viewing Annotation for"
#define ANNOTATION_EDIT_TITLE	"Editing Annotation for"

/* NAG editing window. */
#define NAG_PROMPT	"Type CTRL-t to edit NAG"
#define NAG_EDIT_TITLE	"Numeric Annotation Glyphs"
#define NAG_EDIT_PROMPT	"Type CTRL-g for help"
#define NAG_EDIT_HELP	"NAG Menu Keys"

/* The input window. */
#define INPUT_HELP_PROMPT	"Type CTRL-g for available line editing keys"
#define INPUT_HELP_TITLE	"Line Editing Keys"

/* Country code menu window when editing the "Site" tag. */
#define CC_PROMPT	"Type CTRL-t for country codes"
#define CC_TITLE	"Country Codes"
#define CC_KEY_HELP	"Country Code Keys"

/* Tag editing and viewing windows. */
#define TAG_VIEW_TITLE		"Viewing Roster Tags"
#define TAG_VIEW_HELP		"Tag Viewing Keys"
#define TAG_EDIT_TITLE		"Editing Roster Tags"
#define TAG_EDIT_HELP		"Tag Editing Keys"
#define TAG_EDIT_TAG_TITLE	"Editing Tag"
#define TAG_VIEW_TAG_TITLE	"Viewing Tag"
#define TAG_NEW_TITLE		"New Tag Name"
#define TAG_RESULT_FANCY_WHITE	"white wins"
#define TAG_RESULT_FANCY_BLACK	"black wins"
#define TAG_RESULT_FANCY_DRAW	"draw"
#define TAG_RESULT_FANCY_NA	"undetermined"

/* The file browser. */
#define BROWSER_HELP		"File Browser Keys"
#define BROWSER_CHDIR_TITLE	"Change Directory"
#define BROWSER_PROMPT		"Type TAB for file browser"

/* Miscellaneous strings. */
#define COPY_DATAFILE		"Copying"
#define NONE			"none"
#define x_grid_chars		"abcdefgh"
#define ANYKEY			"[ press any key to continue ]"
#define YESNO			"[ Yes or No ]"
#define ERROR			"[ ERROR ]"
#define CONFIRM			"[ CONFIRM ]"
#define UNKNOWN			"(empty value)"
#define UNAVAILABLE	"not available"
#define HELP_PROMPT	"Type CTRL-g for help"
#define ENGINE_CMD_TITLE "Engine Command"
#define N_OF_N_STR	"of"
#define WHITE_STR	"white"
#define BLACK_STR	"black"

/* Status window strings. */
#define STATUS_GAME_STR		"Game:"
#define STATUS_ENGINE_STR	"Engine:"
#define STATUS_DEPTH_STR	"Depth:"
#define STATUS_BOOK_STR		"Book:"
#define STATUS_TURN_STR		"Turn:"

/* Engine status strings. */
#define ENGINE_READY_STR	"ready"
#define ENGINE_THINKING_STR	"thinking..."
#define ENGINE_OFFLINE_STR	"offline"
#define ENGINE_INITIALIZING_STR	"initializing..."
#define ENGINE_MOVE_HISTORY_STR	" (move history)"

/* History window strings. */
#define HISTORY_MOVE_STR	"Move:"
#define HISTORY_MOVE_NEXT_STR	"Next move:"
#define HISTORY_ANNO_NEXT	"(press ']')"
#define HISTORY_MOVE_PREV_STR	"Prev move:"
#define HISTORY_ANNO_PREV	"(press '[')"

/* White and black window strings. */
#define BW_NAME_STR	"Name:"
#define BW_CAPTURE_STR	"Captures:"

/* These are displayed in menu status/prompt bars. */
#define MENU_ITEM_STR	"Item"
#define MENU_TAG_STR	"Tag"

#endif
