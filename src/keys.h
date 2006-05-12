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
#ifndef KEYS_H
#define KEYS_H

typedef void (key_func)(void);

struct key_s {
    key_func *f;
    int c;
    char *key;
    char d[64];
    int r;
};

static key_func do_history_jump_next;
static key_func do_history_jump_prev;
static key_func do_history_next;
static key_func do_history_prev;
static key_func do_history_half_move_toggle;
static key_func do_history_jump;
static key_func do_history_find_next;
static key_func do_history_find_next;
static key_func do_history_find_prev;
static key_func do_history_annotate;
static key_func do_history_rav_next;
static key_func do_history_rav_prev;
static key_func do_history_menu;
static key_func do_history_toggle;
static key_func do_history_help;

static const struct key_s history_keys[16] = {
    { do_history_jump_next, KEY_UP, "Up", "history jump next", 1 },
    { do_history_jump_prev, KEY_DOWN, "Down", "history jump previous", 1 },
    { do_history_next, KEY_RIGHT, "Right", "next move", 1 },
    { do_history_prev, KEY_LEFT, "Left", "previous move", 1 },
    { do_history_half_move_toggle, ' ', "Space", "toggle half move (ply) stepping", 0 },
    { do_history_jump, 'j', NULL, "jump to move number", 1 },
    { do_history_find_next, '/', NULL, "new move text expression", 0 },
    { do_history_find_next, ']', NULL, "find next move text expression", 1 },
    { do_history_find_prev, '[', NULL, "find previous move text expression", 1 },
    { do_history_annotate, CTRL('a'), "Ctrl-a", "annotate the previous move", 0 },
    { do_history_rav_next, '+', NULL, "next variation of the previous move", 0 },
    { do_history_rav_prev, '-', NULL, "previous variation of the previous move", 0 },
    { do_history_menu, 'M', NULL, "move history tree", 0 },
    { do_history_help, KEY_F(1), "F1", "help", 0 },
    { do_history_toggle, 'h', NULL, "exit history mode", 0 },
    { NULL, 0, NULL, {0}, 0 }
};

static key_func do_edit_select;
static key_func do_edit_commit;
static key_func do_edit_cancel_selected;
static key_func do_edit_delete;
static key_func do_edit_insert;
static key_func do_edit_toggle_castle;
static key_func do_edit_enpassant;
static key_func do_edit_switch_turn;
static key_func do_edit_help;
static key_func do_edit_exit;

static const struct key_s edit_keys[11] = {
    { do_edit_select, ' ', "Space", "select piece for movement", 0 },
    { do_edit_commit, '\n', "Enter", "commit selected piece", 0 },
    { do_edit_cancel_selected, KEY_ESCAPE, "Escape", "cancel selected piece", 0 },
    { do_edit_delete, 'd', NULL, "remove the piece under the cursor", 0 },
    { do_edit_insert, 'i', NULL, "insert piece", 0 },
    { do_edit_toggle_castle, 'c', NULL, "toggle castling availability", 0 },
    { do_edit_enpassant, 'p', NULL, "toggle enpassant square", 0 },
    { do_edit_switch_turn, 'w', NULL, "toggle turn", 0 },
    { do_edit_help, KEY_F(1), "F1", "help", 0 },
    { do_edit_exit, 'e', NULL, "exit edit mode", 0 },
    { NULL, 0, NULL, {0}, 0 }
};

static key_func do_play_select;
static key_func do_play_commit;
static key_func do_play_cancel_selected;
static key_func do_play_set_clock;
static key_func do_play_switch_turn;
static key_func do_play_undo;
static key_func do_play_go;
static key_func do_play_send_command;
static key_func do_play_toggle_engine;
static key_func do_play_toggle_human;
static key_func do_play_help;
static key_func do_play_toggle_pause;
static key_func do_play_history_mode;
static key_func do_play_edit_mode;

static const struct key_s play_keys[15] = {
    { do_play_select, ' ', "Space", "select piece for movement", 0 },
    { do_play_commit, '\n', "Enter", "commit selected piece", 0 },
    { do_play_cancel_selected, KEY_ESCAPE, "Escape", "cancel selected piece", 0 },
    { do_play_set_clock, 'C', NULL, "set clock", 0 },
    { do_play_switch_turn, 'w', NULL, "switch turn", 0 },
    { do_play_undo, 'u', NULL, "undo previous move", 1 },
    { do_play_go, 'g', NULL, "force the chess engine to make the next move", 0 },
    { do_play_send_command, '|', NULL, "send a command to the chess engine", 0 },
    { do_play_toggle_engine, 'E', NULL, "toggle engine/engine play", 0 },
    { do_play_toggle_human, 'H', NULL, "toggle human/human play", 0 },
    { do_play_toggle_pause, 'p', NULL, "toggle pausing of this game", 0 },
    { do_play_history_mode, 'h', NULL, "enter history mode", 0 },
    { do_play_edit_mode, 'e', NULL, "enter edit mode", 0 },
    { do_play_help, KEY_F(1), "F1", "help", 0 },
    { NULL, 0, NULL, {0}, 0 }
};

static key_func do_global_tag_edit;
static key_func do_global_tag_view;
static key_func do_global_find_new;
static key_func do_global_find_next;
static key_func do_global_find_prev;
static key_func do_global_new_game;
static key_func do_global_new_all;
static key_func do_global_next_game;
static key_func do_global_prev_game;
static key_func do_global_game_jump;
static key_func do_global_toggle_delete;
static key_func do_global_delete_game;
static key_func do_global_resume_game;
static key_func do_global_save_game;
static key_func do_global_about;
static key_func do_global_quit;
static key_func do_global_toggle_engine_window;
static key_func do_global_toggle_board_details;

static const struct key_s global_keys[21] = {
    { do_global_tag_edit, 'T', NULL, "edit roster tags", 0 },
    { do_global_tag_view, 't', NULL, "view roster tags", 0 },
    { do_global_find_new, '?', NULL, "new find game expression", 0 },
    { do_global_find_next, '}', NULL, "find next game", 1 },
    { do_global_find_prev, '{', NULL, "find previous game", 1 },
    { do_global_new_game, 'n', NULL, "new game or round", 0 },
    { do_global_new_all, 'N', NULL, "new game from scratch", 0 },
    { do_global_next_game, '>', NULL, "next game", 1 },
    { do_global_prev_game, '<', NULL, "previous game", 1 },
    { do_global_game_jump, 'J', NULL, "jump to game", 1 },
    { do_global_toggle_delete, 'x', NULL, "toggle delete flag", 1 },
    { do_global_delete_game, 'X', NULL, "delete the current or flagged games", 0 },
    { do_global_resume_game, 'r', NULL, "load a PGN file", 0 },
    { do_global_save_game, 's', NULL, "save game", 0 },
    { do_global_toggle_board_details, CTRL('d'), "Ctrl-d", "toggle board details", 0 },
    { do_global_toggle_engine_window, 'W', NULL, "toggle chess engine IO window", 0 },
    { do_global_about, KEY_F(10), "F10", "version information", 0 },
    { do_global_quit, 'Q', NULL, "quit", 0 },
    { NULL, 0, NULL, {0}, 0 }
};

#endif
