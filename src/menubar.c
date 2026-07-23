/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2026 cboard VDK port

    VWM-style pull-down menubar: F10 focuses the bar; Left/Right move across
    top-level menus; Enter/Down opens a dropdown; Esc closes.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <vdk.h>

#include "common.h"
#include "conf.h"
#include "colors.h"
#include "keys.h"
#include "window.h"
#include "ui_screen.h"
#include "menubar.h"

#ifndef N_
#define N_(s) gettext_noop(s)
#endif

enum
{
  MB_FILE = 0,
  MB_GAME,
  MB_PLAY,
  MB_HISTORY,
  MB_EDIT,
  MB_VIEW,
  MB_TAGS,
  MB_HELP,
  MB_COUNT
};

struct mb_item
{
  const char *label;
  key_func *func; /* NULL = separator */
  int mode_mask;  /* bit (1<<MODE_*) or -1 for any */
};

#define MODE_MASK_ANY (-1)
#define MODE_MASK_PLAY (1 << MODE_PLAY)
#define MODE_MASK_HISTORY (1 << MODE_HISTORY)
#define MODE_MASK_EDIT (1 << MODE_EDIT)
#define MODE_MASK_PLAY_HIST (MODE_MASK_PLAY | MODE_MASK_HISTORY)
#define MODE_MASK_PLAY_EDIT (MODE_MASK_PLAY | MODE_MASK_EDIT)

static vk_menubar_t *menubar;
static vk_window_t *dropdown;
static int dropdown_idx = -1;
static int focused;

static const struct mb_item file_items[] = {
    {N_("Open PGN…"), do_global_resume_game, MODE_MASK_ANY},
    {N_("Save…"), do_global_save_game, MODE_MASK_ANY},
    {NULL, NULL, 0},
    {N_("New game / round"), do_global_new_game, MODE_MASK_ANY},
    {N_("New from scratch…"), do_global_new_all, MODE_MASK_ANY},
    {NULL, NULL, 0},
    {N_("Quit…"), do_global_quit, MODE_MASK_ANY},
    {NULL, NULL, -2}};

static const struct mb_item game_items[] = {
    {N_("Next game"), do_global_next_game, MODE_MASK_ANY},
    {N_("Previous game"), do_global_prev_game, MODE_MASK_ANY},
    {N_("Jump to game…"), do_global_game_jump, MODE_MASK_ANY},
    {NULL, NULL, 0},
    {N_("Find game…"), do_global_find_new, MODE_MASK_ANY},
    {N_("Find next"), do_global_find_next, MODE_MASK_ANY},
    {N_("Find previous"), do_global_find_prev, MODE_MASK_ANY},
    {NULL, NULL, 0},
    {N_("Copy game"), do_global_copy_game, MODE_MASK_ANY},
    {N_("Copy as FEN"), do_global_copy_game_fen, MODE_MASK_ANY},
    {NULL, NULL, 0},
    {N_("Toggle delete flag"), do_global_toggle_delete, MODE_MASK_ANY},
    {N_("Delete game(s)…"), do_global_delete_game, MODE_MASK_ANY},
    {NULL, NULL, -2}};

static const struct mb_item play_items[] = {
    {N_("Undo move"), do_play_undo, MODE_MASK_PLAY},
    {N_("Engine move (Go)"), do_play_go, MODE_MASK_PLAY},
    {N_("Send engine command…"), do_play_send_command, MODE_MASK_PLAY},
    {N_("Set clock…"), do_play_set_clock, MODE_MASK_PLAY},
    {NULL, NULL, 0},
    {N_("Human vs Engine"), do_play_toggle_eh_mode, MODE_MASK_PLAY},
    {N_("Engine vs Engine"), do_play_toggle_engine, MODE_MASK_PLAY},
    {N_("Human vs Human"), do_play_toggle_human, MODE_MASK_PLAY},
    {N_("Pause / resume"), do_play_toggle_pause, MODE_MASK_PLAY},
    {N_("Strict castling"), do_play_toggle_strict_castling, MODE_MASK_PLAY},
    {N_("Toggle valid moves"), do_play_toggle_valid_moves, MODE_MASK_PLAY},
    {NULL, NULL, 0},
    {N_("History mode"), do_play_history_mode, MODE_MASK_PLAY},
    {N_("Edit mode"), do_play_edit_mode, MODE_MASK_PLAY},
    {NULL, NULL, -2}};

static const struct mb_item history_items[] = {
    {N_("Exit history mode"), do_history_toggle, MODE_MASK_HISTORY},
    {N_("Enter history mode"), do_play_history_mode, MODE_MASK_PLAY},
    {NULL, NULL, 0},
    {N_("Next move"), do_history_next, MODE_MASK_HISTORY},
    {N_("Previous move"), do_history_prev, MODE_MASK_HISTORY},
    {N_("Jump forward"), do_history_jump_next, MODE_MASK_HISTORY},
    {N_("Jump back"), do_history_jump_prev, MODE_MASK_HISTORY},
    {N_("Jump to move…"), do_history_jump, MODE_MASK_HISTORY},
    {NULL, NULL, 0},
    {N_("Search moves…"), do_history_find_new, MODE_MASK_HISTORY},
    {N_("Find next"), do_history_find_next, MODE_MASK_HISTORY},
    {N_("Find previous"), do_history_find_prev, MODE_MASK_HISTORY},
    {NULL, NULL, 0},
    {N_("Annotate…"), do_history_annotate, MODE_MASK_HISTORY},
    {N_("Next variation"), do_history_rav_next, MODE_MASK_HISTORY},
    {N_("Previous variation"), do_history_rav_prev, MODE_MASK_HISTORY},
    {N_("History tree…"), do_history_menu, MODE_MASK_HISTORY},
    {N_("Half-move step"), do_history_half_move_toggle, MODE_MASK_HISTORY},
    {N_("Rotate board"), do_history_rotate_board, MODE_MASK_HISTORY},
    {NULL, NULL, -2}};

static const struct mb_item edit_items[] = {
    {N_("Exit edit mode"), do_edit_exit, MODE_MASK_EDIT},
    {N_("Enter edit mode"), do_play_edit_mode, MODE_MASK_PLAY},
    {NULL, NULL, 0},
    {N_("Insert piece…"), do_edit_insert, MODE_MASK_EDIT},
    {N_("Delete piece"), do_edit_delete, MODE_MASK_EDIT},
    {N_("Toggle castling"), do_edit_toggle_castle, MODE_MASK_EDIT},
    {N_("En passant square"), do_edit_enpassant, MODE_MASK_EDIT},
    {N_("Switch side to move"), do_edit_switch_turn, MODE_MASK_EDIT},
    {NULL, NULL, -2}};

static const struct mb_item view_items[] = {
    {N_("Board details"), do_global_toggle_board_details, MODE_MASK_ANY},
    {N_("Engine I/O window"), do_global_toggle_engine_window, MODE_MASK_ANY},
    {N_("Redraw"), do_global_redraw, MODE_MASK_ANY},
    {NULL, NULL, -2}};

static const struct mb_item tags_items[] = {
    {N_("View tags"), do_global_tag_view, MODE_MASK_ANY},
    {N_("Edit tags…"), do_global_tag_edit, MODE_MASK_ANY},
    {NULL, NULL, -2}};

static const struct mb_item help_items[] = {
    {N_("Keyboard help"), do_global_help, MODE_MASK_ANY},
    {N_("About"), do_global_about, MODE_MASK_ANY},
#ifdef WITH_LIBPERL
    {N_("PERL filter…"), do_global_perl, MODE_MASK_ANY},
#endif
    {NULL, NULL, -2}};

static const struct mb_item *const menu_tables[MB_COUNT] = {
    file_items, game_items, play_items, history_items,
    edit_items, view_items, tags_items, help_items};

static const char *const menu_titles[MB_COUNT] = {
    N_("File"), N_("Game"), N_("Play"), N_("History"),
    N_("Edit"), N_("View"), N_("Tags"), N_("Help")};

static int
current_mode_bit(void)
{
  struct userdata_s *d;

  if (!gp || !gp->data)
    return MODE_MASK_PLAY;

  d = gp->data;
  if (d->mode == MODE_HISTORY)
    return MODE_MASK_HISTORY;
  if (d->mode == MODE_EDIT)
    return MODE_MASK_EDIT;
  return MODE_MASK_PLAY;
}

static int
item_enabled(const struct mb_item *it)
{
  int bit;

  if (!it->func)
    return 0;
  if (it->mode_mask == MODE_MASK_ANY)
    return 1;
  bit = current_mode_bit();
  return (it->mode_mask & bit) != 0;
}

static void
close_dropdown(void)
{
  if (dropdown)
  {
    cboard_ui_window_destroy((cboard_widget_t *) dropdown);
    dropdown = NULL;
  }
  dropdown_idx = -1;
}

static int
on_dropdown_item(vk_widget_t *widget, void *anything)
{
  key_func *fn = (key_func *) anything;

  (void) widget;
  close_dropdown();
  focused = 0;
  if (menubar)
  {
    vk_menubar_set_focused(menubar, false);
    vk_menubar_update(menubar);
  }
  cboard_ui_refresh();

  if (fn)
    (*fn)();

  return 0;
}

static void
open_dropdown(int idx)
{
  const struct mb_item *table;
  vk_listbox_t *lb;
  vk_window_t *win;
  int i, n, max_w, max_h, item_x, bar_x, bar_y;
  int scr_h, scr_w;
  char caption[64];

  if (idx < 0 || idx >= MB_COUNT || !menubar)
    return;

  close_dropdown();

  table = menu_tables[idx];
  max_w = 12;
  n = 0;
  for (i = 0; table[i].mode_mask != -2; i++)
  {
    if (!table[i].func && !table[i].label)
    {
      n++;
      continue;
    }
    if (table[i].func && !item_enabled(&table[i]))
      continue;
    if (table[i].label)
    {
      int len = (int) strlen(_(table[i].label));

      if (len + 2 > max_w)
        max_w = len + 2;
      n++;
    }
  }

  if (n < 1)
    n = 1;
  max_h = n;
  if (max_h > LINES - 4)
    max_h = LINES - 4;
  if (max_w > COLS - 2)
    max_w = COLS - 2;
  if (max_w < 16)
    max_w = 16;

  lb = vk_listbox_create(max_w, max_h);
  vk_listbox_set_wrap(lb, true);
  vk_listbox_set_highlight(lb,
                           config.color[CONF_MENUS].fg,
                           config.color[CONF_MENUS].bg);
  vk_listbox_set_highlight_attrs(lb, config.color[CONF_MENUS].attrs);
  vk_widget_set_colors(VK_WIDGET(lb),
                       config.color[CONF_MENU].fg,
                       config.color[CONF_MENU].bg);
  vk_widget_set_attrs(VK_WIDGET(lb), config.color[CONF_MENU].attrs);

  for (i = 0; table[i].mode_mask != -2; i++)
  {
    if (!table[i].func && !table[i].label)
    {
      vk_listbox_add_separator(lb, VK_SEPARATOR_SINGLE);
      continue;
    }
    if (table[i].func && !item_enabled(&table[i]))
      continue;
    if (table[i].label && table[i].func)
      vk_listbox_add_item(lb, (char *) _(table[i].label),
                          on_dropdown_item, (void *) table[i].func);
  }

  snprintf(caption, sizeof(caption), " %s ", _(menu_titles[idx]));
  win = vk_window_create(max_w + 2, max_h + 2);
  vk_window_set_title(win, caption);
  vk_window_set_border_style(win, VK_BORDER_SINGLE);
  vk_window_set_border_colors(win,
                              config.color[CONF_MENU].fg,
                              config.color[CONF_MENU].bg);
  vk_window_set_border_attrs(win, config.color[CONF_MENU].attrs);
  vk_widget_set_colors(VK_WIDGET(win),
                       config.color[CONF_MENU].fg,
                       config.color[CONF_MENU].bg);
  vk_widget_set_attrs(VK_WIDGET(win), config.color[CONF_MENU].attrs);
  vk_window_set_child(win, VK_WIDGET(lb));

  vk_widget_get_position(VK_WIDGET(menubar), &bar_x, &bar_y);
  (void) bar_y;
  vk_menubar_get_item_position(menubar, idx, &item_x);
  getmaxyx(stdscr, scr_h, scr_w);
  (void) scr_h;
  if (bar_x + item_x + max_w + 2 > scr_w)
    item_x = scr_w - max_w - 2 - bar_x;
  if (item_x < 0)
    item_x = 0;

  cboard_ui_widget_attach((cboard_widget_t *) win,
                          CBOARD_MENUBAR_H, bar_x + item_x);

  vk_listbox_update(lb);
  vk_window_update(win);

  dropdown = win;
  dropdown_idx = idx;

  cboard_ui_front_clear();
  cboard_ui_front_push((cboard_widget_t *) menubar);
  cboard_ui_front_push((cboard_widget_t *) dropdown);
  cboard_ui_refresh();
}

static int
on_menubar_activate(vk_widget_t *widget, void *anything)
{
  int idx = (int) (intptr_t) anything;

  (void) widget;
  if (dropdown && dropdown_idx == idx)
  {
    close_dropdown();
    cboard_ui_refresh();
    return 0;
  }
  open_dropdown(idx);
  return 0;
}

void cboard_menubar_init(void)
{
  int i;
  int width = COLS > 0 ? COLS : 80;

  if (menubar)
    return;

  menubar = vk_menubar_create(width);
  /* Bar itself: classic black on light gray/white (not the cyan menus). */
  vk_widget_set_colors(VK_WIDGET(menubar), COLOR_BLACK, COLOR_WHITE);
  vk_widget_set_attrs(VK_WIDGET(menubar), A_NORMAL);
  vk_menubar_set_highlight(menubar, COLOR_WHITE, COLOR_BLUE);

  for (i = 0; i < MB_COUNT; i++)
    vk_menubar_add_item(menubar, (char *) _(menu_titles[i]),
                        on_menubar_activate, (void *) (intptr_t) i);

  vk_widget_move(VK_WIDGET(menubar), 0, 0);
  cboard_ui_widget_attach((cboard_widget_t *) menubar, 0, 0);
  vk_menubar_set_focused(menubar, false);
  vk_menubar_update(menubar);
  focused = 0;
  dropdown = NULL;
  dropdown_idx = -1;
}

void cboard_menubar_shutdown(void)
{
  close_dropdown();
  if (menubar)
  {
    cboard_ui_widget_destroy((cboard_widget_t *) menubar);
    menubar = NULL;
  }
  focused = 0;
}

void cboard_menubar_resize(void)
{
  if (!menubar)
    return;

  close_dropdown();
  vk_widget_resize(VK_WIDGET(menubar), COLS, 1);
  vk_widget_move(VK_WIDGET(menubar), 0, 0);
  vk_menubar_update(menubar);
  cboard_ui_widget_raise((cboard_widget_t *) menubar);
}

void cboard_menubar_refresh(void)
{
  if (!menubar)
    return;

  vk_menubar_update(menubar);

  if (dropdown)
  {
    vk_listbox_t *lb = VK_LISTBOX(vk_window_get_child(dropdown));

    if (lb)
      vk_listbox_update(lb);
    vk_window_update(dropdown);
  }

  /*
   * Register menubar (and open dropdown) as the front layer.  refresh()
   * raises them and redraws them after the full composite so the board
   * cannot occlude the menus.
   */
  cboard_ui_front_clear();
  cboard_ui_front_push((cboard_widget_t *) menubar);
  if (dropdown)
    cboard_ui_front_push((cboard_widget_t *) dropdown);
}

int cboard_menubar_active(void)
{
  return focused || dropdown != NULL;
}

static void
menubar_activate(void)
{
  if (!menubar)
    return;

  focused = 1;
  vk_menubar_set_focused(menubar, true);
  if (vk_menubar_get_curr(menubar) < 0)
    vk_menubar_set_curr(menubar, 0);
  vk_menubar_update(menubar);
  cboard_ui_front_clear();
  cboard_ui_front_push((cboard_widget_t *) menubar);
  if (dropdown)
    cboard_ui_front_push((cboard_widget_t *) dropdown);
  cboard_ui_refresh();
}

static void
menubar_deactivate(void)
{
  close_dropdown();
  focused = 0;
  if (menubar)
  {
    vk_menubar_set_focused(menubar, false);
    vk_menubar_update(menubar);
  }
  cboard_ui_refresh();
}

int cboard_menubar_key(wint_t c)
{
  vk_listbox_t *lb;

  /* F10 toggles the menubar. */
  if (c == KEY_F(10))
  {
    if (cboard_menubar_active())
      menubar_deactivate();
    else
      menubar_activate();
    return 1;
  }

  if (!cboard_menubar_active())
    return 0;

  /* Dropdown open: drive listbox. */
  if (dropdown)
  {
    lb = VK_LISTBOX(vk_window_get_child(dropdown));

    switch (c)
    {
    case KEY_ESCAPE:
      close_dropdown();
      cboard_ui_refresh();
      return 1;
    case KEY_UP:
      if (lb)
      {
        vk_listbox_set_prev(lb);
        vk_listbox_update(lb);
        vk_window_update(dropdown);
        cboard_ui_refresh();
      }
      return 1;
    case KEY_DOWN:
      if (lb)
      {
        vk_listbox_set_next(lb);
        vk_listbox_update(lb);
        vk_window_update(dropdown);
        cboard_ui_refresh();
      }
      return 1;
    case KEY_HOME:
      if (lb)
      {
        vk_listbox_set_curr(lb, 0);
        vk_listbox_update(lb);
        vk_window_update(dropdown);
        cboard_ui_refresh();
      }
      return 1;
    case KEY_END:
      if (lb)
      {
        int n = vk_listbox_get_item_count(lb);

        if (n > 0)
          vk_listbox_set_curr(lb, n - 1);
        vk_listbox_update(lb);
        vk_window_update(dropdown);
        cboard_ui_refresh();
      }
      return 1;
    case KEY_LEFT:
      close_dropdown();
      if (menubar)
      {
        vk_menubar_set_prev(menubar);
        vk_menubar_update(menubar);
        open_dropdown(vk_menubar_get_curr(menubar));
      }
      return 1;
    case KEY_RIGHT:
      close_dropdown();
      if (menubar)
      {
        vk_menubar_set_next(menubar);
        vk_menubar_update(menubar);
        open_dropdown(vk_menubar_get_curr(menubar));
      }
      return 1;
    case '\n':
    case KEY_ENTER:
      if (lb)
        vk_listbox_exec_curr(lb);
      return 1;
    default:
      return 1; /* swallow while menu open */
    }
  }

  /* Bar focused, no dropdown. */
  switch (c)
  {
  case KEY_ESCAPE:
    menubar_deactivate();
    return 1;
  case KEY_LEFT:
    if (menubar)
    {
      vk_menubar_set_prev(menubar);
      vk_menubar_update(menubar);
      cboard_ui_refresh();
    }
    return 1;
  case KEY_RIGHT:
    if (menubar)
    {
      vk_menubar_set_next(menubar);
      vk_menubar_update(menubar);
      cboard_ui_refresh();
    }
    return 1;
  case KEY_DOWN:
  case '\n':
  case KEY_ENTER:
    if (menubar)
      open_dropdown(vk_menubar_get_curr(menubar));
    return 1;
  default:
    return 1;
  }
}

static int
mb_left_press(mmask_t bstate)
{
  return (bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)) != 0;
}

int cboard_menubar_mouse(int x, int y, mmask_t bstate)
{
  int bar_x, bar_y, bar_w, bar_h;
  int idx;
  int lx;

  if (!menubar)
    return 0;

  /* Wheel over open dropdown → move selection. */
  if (dropdown && (bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED)))
  {
    int dx, dy, dw, dh;
    vk_listbox_t *lb;

    vk_widget_get_position(VK_WIDGET(dropdown), &dx, &dy);
    vk_widget_get_metrics(VK_WIDGET(dropdown), &dw, &dh);
    if (x >= dx && x < dx + dw && y >= dy && y < dy + dh)
    {
      lb = VK_LISTBOX(vk_window_get_child(dropdown));
      if (lb)
      {
        if (bstate & BUTTON4_PRESSED)
          vk_listbox_set_prev(lb);
        else
          vk_listbox_set_next(lb);
        vk_listbox_update(lb);
        vk_window_update(dropdown);
        cboard_ui_refresh();
      }
      return 1;
    }
  }

  if (!mb_left_press(bstate))
    return 0;

  /* Click on open dropdown list → select and activate. */
  if (dropdown)
  {
    int dx, dy, dw, dh;
    int ly, row;
    vk_listbox_t *lb;

    vk_widget_get_position(VK_WIDGET(dropdown), &dx, &dy);
    vk_widget_get_metrics(VK_WIDGET(dropdown), &dw, &dh);
    if (x >= dx && x < dx + dw && y >= dy && y < dy + dh)
    {
      lb = VK_LISTBOX(vk_window_get_child(dropdown));
      /* Interior of window frame: inset 1 for border. */
      ly = y - dy - 1;
      if (lb && ly >= 0)
      {
        int n = vk_listbox_get_item_count(lb);
        int scroll = vk_listbox_get_scroll_pos(lb);

        row = scroll + ly;
        if (row >= 0 && row < n)
        {
          vk_listbox_set_curr(lb, row);
          vk_listbox_update(lb);
          vk_window_update(dropdown);
          /* Activate via the same path as Enter. */
          vk_listbox_exec_curr(lb);
          return 1;
        }
      }
      return 1;
    }

    /* Click outside dropdown while open → close (and maybe open other). */
    close_dropdown();
  }

  vk_widget_get_position(VK_WIDGET(menubar), &bar_x, &bar_y);
  vk_widget_get_metrics(VK_WIDGET(menubar), &bar_w, &bar_h);
  if (y < bar_y || y >= bar_y + bar_h || x < bar_x || x >= bar_x + bar_w)
  {
    if (cboard_menubar_active())
    {
      menubar_deactivate();
      return 1;
    }
    return 0;
  }

  lx = x - bar_x;
  idx = vk_menubar_hit_test(menubar, lx);
  if (idx < 0)
  {
    if (!cboard_menubar_active())
      menubar_activate();
    return 1;
  }

  if (!cboard_menubar_active())
  {
    focused = 1;
    vk_menubar_set_focused(menubar, true);
  }
  vk_menubar_set_curr(menubar, idx);
  vk_menubar_update(menubar);
  open_dropdown(idx);
  return 1;
}
