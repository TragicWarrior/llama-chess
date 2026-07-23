/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2026 cboard VDK port

    Mouse dispatch using vk_kmio MEVENTs and existing VDK hit-test helpers.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <vdk.h>

#include "common.h"
#include "window.h"
#include "ui_screen.h"
#include "menubar.h"
#include "filebrowser.h"
#include "menu.h"
#include "message.h"
#include "mouse.h"

extern GAME gp;
void update_all(GAME g);
int cboard_board_mouse(int x, int y, mmask_t bstate);

static int
left_press(mmask_t b)
{
  return (b & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)) != 0;
}

static int
point_in_widget(vk_widget_t *w, int x, int y, int *lx, int *ly)
{
  int wx, wy, ww, wh;

  if (!w)
    return 0;
  vk_widget_get_position(w, &wx, &wy);
  vk_widget_get_metrics(w, &ww, &wh);
  if (x < wx || y < wy || x >= wx + ww || y >= wy + wh)
    return 0;
  if (lx)
    *lx = x - wx;
  if (ly)
    *ly = y - wy;
  return 1;
}

/* construct_menu only — win->app_kind must be WIN_APP_MENU. */
static int
mouse_modal_list(WIN *win, int x, int y, mmask_t bstate)
{
  struct menu_input_s *m;
  vk_listbox_t *lb;
  int ly, row, scroll, n;

  if (!win || win->app_kind != WIN_APP_MENU || !win->data)
    return 0;

  m = win->data;
  if (!m->listbox)
    return 0;

  if (!point_in_widget(VK_WIDGET(win->vk), x, y, NULL, &ly))
    return 0;

  ly -= 1; /* frame border */
  if (ly < 0)
    return 1;

  lb = (vk_listbox_t *) m->listbox;

  if (bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED))
  {
    if (bstate & BUTTON4_PRESSED)
      vk_listbox_set_prev(lb);
    else
      vk_listbox_set_next(lb);
    win->c = (bstate & BUTTON4_PRESSED) ? KEY_UP : KEY_DOWN;
    (*win->func)(win);
    return 1;
  }

  if (!left_press(bstate))
    return 1;

  scroll = vk_listbox_get_scroll_pos(lb);
  n = vk_listbox_get_item_count(lb);
  row = scroll + ly;
  if (row < 0 || row >= n)
    return 1;

  vk_listbox_set_curr(lb, row);
  if (bstate & BUTTON1_DOUBLE_CLICKED)
  {
    win->c = '\n';
    if ((*win->func)(win) == 0)
    {
      if (win->efunc)
        (*win->efunc)(win);
      window_destroy(win);
      if (gp)
        update_all(gp);
    }
  }
  else
  {
    /* Single click: refresh selection highlight only. */
    win->c = 0;
    (*win->func)(win);
    cboard_ui_refresh();
  }
  return 1;
}

/* Message dialog: wheel scrolls; click = any key (dismiss). */
static int
mouse_modal_message(WIN *win, int x, int y, mmask_t bstate)
{
  if (!win || win->app_kind != WIN_APP_MESSAGE || !win->vk)
    return 0;
  if (!point_in_widget(VK_WIDGET(win->vk), x, y, NULL, NULL))
    return 0;

  if (bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED))
  {
    win->c = (bstate & BUTTON4_PRESSED) ? KEY_PPAGE : KEY_NPAGE;
    (*win->func)(win);
    return 1;
  }

  if (left_press(bstate))
  {
    /* Same as any key: dismiss. */
    win->c = ' ';
    if ((*win->func)(win) == 0)
    {
      if (win->efunc)
        (*win->efunc)(win);
      window_destroy(win);
      if (gp)
        update_all(gp);
    }
    return 1;
  }

  return 1;
}

static int
mouse_modal_input(WIN *win, int x, int y, mmask_t bstate)
{
  if (!win || win->app_kind != WIN_APP_INPUT || !win->vk)
    return 0;
  if (!point_in_widget(VK_WIDGET(win->vk), x, y, NULL, NULL))
    return 0;
  /* Absorb clicks over the input dialog; keyboard still drives it. */
  (void) bstate;
  return 1;
}

int cboard_mouse_handle(const MEVENT *mev)
{
  WIN *win;
  int x, y;
  mmask_t b;

  if (!mev)
    return 0;

  x = mev->x;
  y = mev->y;
  b = mev->bstate;

  win = window_top();
  if (win)
  {
    switch (win->app_kind)
    {
    case WIN_APP_FILEBROWSER:
      if (file_browser_mouse(win, x, y, b))
        return 1;
      break;
    case WIN_APP_CONFIRM:
      if (confirm_dialog_mouse(win, x, y, b))
        return 1;
      break;
    case WIN_APP_MENU:
      if (mouse_modal_list(win, x, y, b))
        return 1;
      break;
    case WIN_APP_MESSAGE:
      if (mouse_modal_message(win, x, y, b))
        return 1;
      break;
    case WIN_APP_INPUT:
      if (mouse_modal_input(win, x, y, b))
        return 1;
      break;
    default:
      /* Fallback: filedialog by vk_kind only. */
      if (win->vk_kind == WIN_VK_FILEDIALOG && file_browser_mouse(win, x, y, b))
        return 1;
      if (win->vk_kind == WIN_VK_POPUP && confirm_dialog_mouse(win, x, y, b))
        return 1;
      break;
    }
  }

  if (cboard_menubar_mouse(x, y, b))
    return 1;

  if (cboard_board_mouse(x, y, b))
    return 1;

  return 0;
}
