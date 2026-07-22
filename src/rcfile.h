/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2024 Ben Kibbey <bjk@luxsci.net>

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
#ifndef RCFILE_H
#define RCFILE_H

static const struct custom_key_s
{
  int mode;
  const char *name;
  key_func *func;
  int r;
} config_keys[] =
{
  {MODE_PLAY, "select", do_play_select, 0},
  {MODE_PLAY, "commit", do_play_commit, 0},
  {MODE_PLAY, "cancel_selected", do_play_cancel_selected, 0},
  {MODE_PLAY, "set_clock", do_play_set_clock, 0},
//    {MODE_PLAY, "switch_turn", do_play_switch_turn, 0},
  {MODE_PLAY, "undo", do_play_undo, 1},
  {MODE_PLAY, "go", do_play_go, 0},
  {MODE_PLAY, "send_command", do_play_send_command, 0},
  {MODE_PLAY, "toggle_play_eh", do_play_toggle_eh_mode, 0},
  {MODE_PLAY, "toggle_engine", do_play_toggle_engine, 0},
  {MODE_PLAY, "toggle_human", do_play_toggle_human, 0},
  {MODE_PLAY, "toggle_pause", do_play_toggle_pause, 0},
  {MODE_PLAY, "history_mode", do_play_history_mode, 0},
  {MODE_PLAY, "edit_mode", do_play_edit_mode, 0},
  {MODE_PLAY, "toggle_strict_castling", do_play_toggle_strict_castling, 0},
  {MODE_HISTORY, "jump_next", do_history_jump_next, 1},
  {MODE_HISTORY, "jump_prev", do_history_jump_prev, 1},
  {MODE_HISTORY, "next", do_history_next, 1},
  {MODE_HISTORY, "prev", do_history_prev, 1},
  {MODE_HISTORY, "half_move_toggle", do_history_half_move_toggle, 0},
  {MODE_HISTORY, "rotate_board", do_history_rotate_board, 1},
  {MODE_HISTORY, "jump", do_history_jump, 1},
  {MODE_HISTORY, "find_next", do_history_find_next, 1},
  {MODE_HISTORY, "find_new", do_history_find_new, 0},
  {MODE_HISTORY, "find_prev", do_history_find_prev, 1},
  {MODE_HISTORY, "annotate", do_history_annotate, 0},
  {MODE_HISTORY, "rav_next", do_history_rav_next, 0},
  {MODE_HISTORY, "rav_prev", do_history_rav_prev, 0},
  {MODE_HISTORY, "menu", do_history_menu, 0},
  {MODE_HISTORY, "toggle", do_history_toggle, 0},
  {MODE_EDIT, "select", do_edit_select, 0},
  {MODE_EDIT, "commit", do_edit_commit, 0},
  {MODE_EDIT, "cancel_selected", do_edit_cancel_selected, 0},
  {MODE_EDIT, "delete", do_edit_delete, 0},
  {MODE_EDIT, "insert", do_edit_insert, 0},
  {MODE_EDIT, "toggle_castle", do_edit_toggle_castle, 0},
  {MODE_EDIT, "enpassant", do_edit_enpassant, 0},
  {MODE_EDIT, "switch_turn", do_edit_switch_turn, 0},
  {MODE_EDIT, "exit", do_edit_exit, 0},
  {MODE_ANY, "tag_edit", do_global_tag_edit, 0},
  {MODE_ANY, "tag_view", do_global_tag_view, 0},
  {MODE_ANY, "find_new", do_global_find_new, 0},
  {MODE_ANY, "find_next", do_global_find_next, 1},
  {MODE_ANY, "find_prev", do_global_find_prev, 1},
  {MODE_ANY, "new_game", do_global_new_game, 0},
  {MODE_ANY, "new_all", do_global_new_all, 0},
  {MODE_ANY, "copy_game", do_global_copy_game, 0},
  {MODE_ANY, "copy_game_fen", do_global_copy_game_fen, 0},
  {MODE_ANY, "next_game", do_global_next_game, 1},
  {MODE_ANY, "prev_game", do_global_prev_game, 1},
  {MODE_ANY, "game_jump", do_global_game_jump, 1},
  {MODE_ANY, "toggle_delete", do_global_toggle_delete, 0},
  {MODE_ANY, "delete_game", do_global_delete_game, 0},
  {MODE_ANY, "resume_game", do_global_resume_game, 0},
  {MODE_ANY, "save_game", do_global_save_game, 0},
  {MODE_ANY, "about", do_global_about, 0},
  {MODE_ANY, "quit", do_global_quit, 0},
  {MODE_ANY, "toggle_engine_window", do_global_toggle_engine_window, 0},
  {MODE_ANY, "toggle_board_details", do_global_toggle_board_details, 0},
  {MODE_ANY, "redraw", do_global_redraw, 0},
  {MODE_ANY, "help", do_global_help, 0},
#ifdef WITH_LIBPERL
  {MODE_ANY, "perl", do_global_perl, 0},
#endif
  {-1, NULL, NULL, 0}
};

void set_config_defaults ();
void parse_rcfile (const char *filename);
void add_key_binding (struct key_s ***, key_func *, wint_t c, char *, int);
void set_default_keys ();
const wchar_t *key_lookup (struct key_s **keys, key_func f);
wint_t keycode_lookup (struct key_s **keys, key_func f);
/* True if keycode c is bound to function f (any of possibly several aliases). */
int key_matches (struct key_s **keys, key_func f, wint_t c);
char *fancy_key_name (wint_t c);
struct key_s * key_lookup_by_keycode (struct key_s **keys, wint_t c);

#endif
