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
/*
 * The help dialog box strings are found in the other header files.
 */
#ifndef STRINGS_H
#define STRINGS_H

#define GAME_FIND_EXPRESSION_TITLE	"Find Game by Tag Expression"
#define GAME_FIND_EXPRESSION_PROMPT	"[name expression:]value expression"
#define GAME_JUMP_TITLE			"Jump to Game Number"
#define GAME_HISTORY_JUMP_TITLE		"Jump to Move Number"
#define GAME_NOTSAVED			"*delete*"
#define GAME_SAVE_OVERWRITE_PROMPT	"'a' to append, 'o' to overwrite"
#define GAME_SAVE_MULTI_PROMPT		"Type 'c' or 'a' or any other key " \
    "to abort"
#define GAME_SAVE_MULTI_TEXT		"There is more than one game " \
    "loaded. You can save only the current game by pressing 'c', or all " \
    "games by pressing 'a'."
#define GAME_SAVE_FROM_HISTORY_TITLE	"Save From History"
#define GAME_SAVE_FROM_HISTORY_PROMPT	"Type 'c' or any other key"
#define GAME_SAVE_FROM_HISTORY_TEXT	"This game number is in history mode. You can " \
    "save all moves up to and including the current move by pressing 'c'. " \
    "Any other key will save the entire move history."
#define GAME_RESUME_HISTORY_TEXT	"Resuming a game from previous " \
    "history will remove all future moves. Do you really want to resume " \
    "from history?"
#define GAME_DELETE_ALL_TEXT		"Delete all games marked for deletion?"
#define GAME_DELETE_GAME_TEXT		"Delete the current game?"
#define GAME_NEW_PROMPT	"Really start a new game from scratch?"
#define GAME_NEW_TEXT	"Use the 'N' command to start a new game or the 'r' " \
    "command to load a previous game"
#define GAME_HELP_INDEX_TITLE	"Command Key Index"
#define GAME_HELP_REPEAT " (* = can take a repeat count)"
#define GAME_HELP_INDEX_PROMPT	"p/h/e/g or any other key to quit"
#define GAME_HELP_HISTORY_TITLE "History Mode Keys" GAME_HELP_REPEAT
#define GAME_HELP_PLAY_TITLE "Play Mode Keys" GAME_HELP_REPEAT
#define GAME_HELP_EDIT_TITLE "Edit Mode Keys" GAME_HELP_REPEAT
#define GAME_HELP_GAME_TITLE "Global Game Keys" GAME_HELP_REPEAT
#define GAME_LOAD_TITLE	"Load Filename"
#define GAME_SAVE_TITLE	"Save Game Filename"
#define GAME_HELP_PROMPT	"Type F1 for help"
#define GAME_EDIT_TITLE		"Insert Piece"
#define GAME_EDIT_PROMPT	"P=pawn, R=rook, N=knight, B=bishop, "\
    "Q=queen, K=king"
#define GAME_EDIT_TEXT		"Type the piece letter to insert. Lowercase " \
    "for a black piece, uppercase for a white piece."

/* Error strings. */
#define E_TAG_NAMETOOLONG	"Cannot add tag. Name too long."
#define E_REGCOMP_TITLE	"Error Compiling Regular Expression"
#define E_REGEXEC_TITLE	"Error Matching Regular Expression"
#define E_RESUME_BLACK	"Cannot resume a game with black starting position " \
    "(yet)."
#define E_HOME_ENV	"The HOME environment variable is unset."
#define E_TAG_DATE_FMT	"The \"Date\" tag must be in YYYY.MM.DD format."
#define E_FILEEXISTS	"File exists:"
#define E_REMOVE_STR	"Cannot remove the Seven Tag Roster"
#define E_DUPLICATE_TAG	"Could not add duplicate tag"
#define E_DELETE_GAME	"Cannot delete last game."
#define E_A2A4_PARSE	"Parse error. Probably a bug."
#define E_PGN_PARSE	"PGN parse error."
#define E_FEN_PARSE	"FEN parse error."
#define E_BROKEN_PIPE	"Broken pipe. Quitting."
#define E_NOTADIR	"Not a directory."
#define E_NOTAREGFILE	"Not a regular file."
#define E_INITCURSES	"Could not initialize curses."
#define E_SAVE_COMPRESS	"Cannot append to compressed file."
#define E_INVALID_MOVE	"Invalid move"
#define E_AMBIGUOUS	"Ambiguous move"
#define E_INVALID_COMMAND	"Invalid engine command or move"
#define E_SELECT_TURN	"It is not your turn to move. You can switch sides " \
    "by pressing 'w' or force the engine to make the next move by " \
    "pressing 'g'."

/* The notification line in the status window. */
#define NOTIFY_SAVED		"Game saved."
#define NOTIFY_SAVE_ABORTED	"Save game aborted."
#define NOTIFY_SAVE_FAILED	"Save game failed."
#define NOTIFY_CHECK		"Check!"
#define NOTIFY_GAMEOVER_WWINS	"Game over! White wins."
#define NOTIFY_GAMEOVER_BWINS	"Game over! Black wins."
#define NOTIFY_GAMEOVER_DRAW	"Game over! Draw."
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
#define NAG_EDIT_PROMPT	"Type F1 for help"
#define NAG_EDIT_HELP	"NAG Menu Keys"

/* The input window. */
#define INPUT_HELP_PROMPT	GAME_HELP_PROMPT
#define INPUT_HELP_TITLE	"Line Editing Keys"

/* Clock setting input window. */
#define CLOCK_TITLE		"Set Clock"
#define CLOCK_HELP		"Format is: [+]digit[hms] [digit[hms]] ..."

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
#define PRESS_ENTER		"Press ENTER"
#define FIND_REGEXP		"Find Move Text Expression"
#define COPY_DATAFILE		"Copying"
#define NONE			"none"
#define x_grid_chars		"abcdefgh"
#define ANYKEY			"[ press any key to continue ]"
#define YESNO			"[ Yes or No ]"
#define ERROR			"[ ERROR ]"
#define CONFIRM			"[ CONFIRM ]"
#define UNKNOWN			"(empty value)"
#define UNAVAILABLE	"not available"
#define HELP_PROMPT	"Type F1 for help"
#define ENGINE_CMD_TITLE "Engine Command"
#define N_OF_N_STR	"of"
#define WHITE_STR	"white"
#define BLACK_STR	"black"

/* Status window strings. */
#define STATUS_FILE_STR		"File:"
#define STATUS_MODE_STR		"Mode:"
#define STATUS_GAME_STR		"Game:"
#define STATUS_ENGINE_STR	"Engine:"
#define STATUS_TURN_STR		"Turn:"
#define STATUS_CLOCK_STR	"Clock:"

/* Engine status strings. */
#define ENGINE_READY_STR	"ready"
#define ENGINE_PONDER_STR	"pondering..."
#define ENGINE_OFFLINE_STR	"offline"
#define ENGINE_INITIALIZING_STR	"initializing..."

/* Mode status strings. */
#define MODE_HISTORY_STR	"move history"
#define MODE_EDIT_STR		"edit"
#define MODE_PLAY_STR		"play"

/* History window strings. */
#define HISTORY_PLY_STEP	" (ply)"
#define HISTORY_MOVE_STR	"Move:"
#define HISTORY_MOVE_NEXT_STR	"Next move:"
#define HISTORY_MOVE_PREV_STR	"Prev move:"

/* White and black window strings. */
#define BW_NAME_STR	"Name:"
#define BW_CAPTURE_STR	"Captures:"

/* These are displayed in menu status/prompt bars. */
#define MENU_ITEM_STR	"Item"
#define MENU_TAG_STR	"Tag"

#endif
