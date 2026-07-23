/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
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
#ifndef KEYS_H
#define KEYS_H

#define CTRL_KEY(x) ((x) & 0x1f)
#define KEY_ESCAPE CTRL_KEY('[')

enum
{
    MODE_ANY = -1,
    MODE_HISTORY,
    MODE_PLAY,
    MODE_EDIT
};

enum
{
    PLAY_HE,
    PLAY_EH
};

enum
{
    CURSOR_POSITION,
    SP_POSITION,
    SPS_POSITION
};

typedef void(key_func)(void);

struct key_s
{
    key_func *f;
    wint_t c;
    wchar_t *key;
    wchar_t *d;
    int r;
};

struct macro_s
{
    wint_t c;
    wint_t *keys;
    wchar_t *desc;
    int n;
    int total;
    int mode;
};

extern struct macro_s **macros;
extern int *macro_depth;
extern int macro_depth_n;

extern struct key_s **history_keys;
key_func do_history_jump_next;
key_func do_history_jump_prev;
key_func do_history_next;
key_func do_history_prev;
key_func do_history_half_move_toggle;
key_func do_history_rotate_board;
key_func do_history_jump;
key_func do_history_find_new;
key_func do_history_find_next;
key_func do_history_find_prev;
key_func do_history_annotate;
key_func do_history_rav_next;
key_func do_history_rav_prev;
key_func do_history_menu;
key_func do_history_toggle;

extern struct key_s **edit_keys;
key_func do_edit_select;
key_func do_edit_commit;
key_func do_edit_cancel_selected;
key_func do_edit_delete;
key_func do_edit_insert;
key_func do_edit_toggle_castle;
key_func do_edit_enpassant;
key_func do_edit_switch_turn;
key_func do_edit_exit;

extern struct key_s **play_keys;
key_func do_play_select;
key_func do_play_commit;
key_func do_play_cancel_selected;
key_func do_play_set_clock;
//key_func do_play_switch_turn;
key_func do_play_undo;
key_func do_play_go;
key_func do_play_send_command;
key_func do_play_toggle_eh_mode;
key_func do_play_toggle_engine;
key_func do_play_toggle_human;
key_func do_play_toggle_pause;
key_func do_play_history_mode;
key_func do_play_edit_mode;
key_func do_play_toggle_strict_castling;
key_func do_play_toggle_valid_moves;

extern struct key_s **global_keys;
key_func do_global_tag_edit;
key_func do_global_tag_view;
key_func do_global_find_new;
key_func do_global_find_next;
key_func do_global_find_prev;
key_func do_global_new_game;
key_func do_global_new_all;
key_func do_global_copy_game;
key_func do_global_copy_game_fen;
key_func do_global_next_game;
key_func do_global_prev_game;
key_func do_global_game_jump;
key_func do_global_toggle_delete;
key_func do_global_delete_game;
key_func do_global_resume_game;
key_func do_global_save_game;
key_func do_global_connect_ollama;
key_func do_global_disconnect_ollama;
key_func do_global_about;
key_func do_global_quit;
key_func do_global_toggle_engine_window;
key_func do_global_toggle_board_details;
key_func do_global_redraw;
key_func do_global_help;
#ifdef WITH_LIBPERL
key_func do_global_perl;
#endif

#endif
