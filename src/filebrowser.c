/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2007-2024 Ben Kibbey <bjk@luxsci.net>
    Copyright (C) 2026 cboard VDK port

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <pwd.h>

#include <vdk.h>

#include "misc.h"
#include "common.h"
#include "window.h"
#include "message.h"
#include "input.h"
#include "filebrowser.h"
#include "conf.h"
#include "ui_screen.h"

struct file_s **files; /* kept for ABI with older code paths */
char *oldwd;

/* VDK filedialog vertical box slots. */
enum
{
  FB_BOX_PATH = 0,
  FB_BOX_LIST = 1,
  FB_BOX_BUTTONS = 2
};

/*
 * App-layer tab order: breadcrumb → picker → Okay → Cancel.
 * OK/Cancel share the button bar box slot; button_focus selects which.
 */
enum
{
  FB_TAB_PATH = 0,
  FB_TAB_LIST,
  FB_TAB_OK,
  FB_TAB_CANCEL,
  FB_TAB_COUNT
};

struct fb_state_s
{
  struct input_s *in; /* parent input dialog state */
  WIN *parent;        /* construct_input WIN under this dialog */
  vk_filedialog_t *fd;
  int tab; /* FB_TAB_* */
};

static int
fb_is_enter_key(int key)
{
  return key == '\n' || key == '\r' || key == KEY_ENTER;
}

/* Copy current list selection; 0 = empty/unavailable. */
static int
fb_copy_selected(struct fb_state_s *st, char *buf, size_t buflen)
{
  const char *selected;

  if (!st || !st->fd || !buf || buflen < 2)
    return 0;

  selected = vk_filedialog_get_selected(st->fd);
  if (!selected || !selected[0])
    return 0;

  strncpy(buf, selected, buflen - 1);
  buf[buflen - 1] = '\0';
  return 1;
}

static int
fb_is_dir_entry(const char *name)
{
  size_t len;

  if (!name || !name[0])
    return 0;
  if (strcmp(name, "..") == 0)
    return 1;
  len = strlen(name);
  return name[len - 1] == '/';
}

/*
 * Write a file name into the parent input.  Returns 1 on success.
 * name must already be a non-directory list entry (not ".." / "foo/").
 */
static int
fb_apply_name(struct fb_state_s *st, const char *name)
{
  const char *path;
  char fullpath[FILENAME_MAX];

  if (!st || !st->fd || !st->in || !name || !name[0])
    return 0;
  if (fb_is_dir_entry(name))
    return 0;

  path = vk_filedialog_get_path(st->fd);
  if (!path || !path[0])
    return 0;

  if (strcmp(path, "/") == 0)
    snprintf(fullpath, sizeof(fullpath), "/%s", name);
  else
    snprintf(fullpath, sizeof(fullpath), "%s/%s", path, name);

  input_set_buf(st->in, fullpath);
  if (st->parent)
    input_refresh_win(st->parent);

  if (oldwd)
    free(oldwd);
  oldwd = strdup(path);
  return 1;
}

/* Focused control: bright yellow FG; idle: bright white (CONF_MENU). */
#define FB_FOCUS_FG COLOR_YELLOW

static void
fb_paint_slot(vk_widget_t *w, short fg, short bg, attr_t attrs)
{
  if (!w)
    return;
  vk_widget_set_colors(w, fg, bg);
  vk_widget_set_attrs(w, attrs);
}

static void
fb_style_widgets(struct fb_state_s *st)
{
  vk_filedialog_t *fd;
  vk_listbox_t *lb;
  vk_widget_t *path_w;
  vk_widget_t *list_frame;
  vk_box_t *bar;
  vk_widget_t *btn0, *btn1;
  short base_fg, base_bg;
  attr_t attrs;
  short path_fg, ok_fg, cancel_fg;

  if (!st || !st->fd)
    return;

  fd = st->fd;
  base_fg = config.color[CONF_MENU].fg;
  base_bg = config.color[CONF_MENU].bg;
  attrs = config.color[CONF_MENU].attrs | A_BOLD;

  path_fg = (st->tab == FB_TAB_PATH) ? FB_FOCUS_FG : base_fg;
  ok_fg = (st->tab == FB_TAB_OK) ? FB_FOCUS_FG : base_fg;
  cancel_fg = (st->tab == FB_TAB_CANCEL) ? FB_FOCUS_FG : base_fg;

  vk_filedialog_set_colors(fd, base_fg, base_bg);
  vk_filedialog_set_button_colors(fd, base_fg, base_bg);
  vk_filedialog_set_button_attrs(fd, attrs);
  vk_widget_set_attrs(VK_WIDGET(fd), attrs);

  path_w = vk_box_get_widget(VK_BOX(fd), FB_BOX_PATH);
  fb_paint_slot(path_w, path_fg, base_bg, attrs);

  /* List body always bright white; only the selected row turns yellow. */
  list_frame = vk_box_get_widget(VK_BOX(fd), FB_BOX_LIST);
  fb_paint_slot(list_frame, base_fg, base_bg, attrs);

  lb = vk_filedialog_get_file_list(fd);
  if (lb)
  {
    fb_paint_slot(VK_WIDGET(lb), base_fg, base_bg, attrs);
    if (st->tab == FB_TAB_LIST)
    {
      vk_listbox_set_highlight(lb, FB_FOCUS_FG, COLOR_BLUE);
      vk_listbox_set_highlight_attrs(lb, A_BOLD);
    }
    else
    {
      vk_listbox_set_highlight(lb,
                               config.color[CONF_MENUS].fg,
                               config.color[CONF_MENUS].bg);
      vk_listbox_set_highlight_attrs(lb,
                                     config.color[CONF_MENUS].attrs | A_BOLD);
    }
  }

  bar = VK_BOX(vk_box_get_widget(VK_BOX(fd), FB_BOX_BUTTONS));
  if (bar)
  {
    btn0 = vk_box_get_widget(bar, 0);
    btn1 = vk_box_get_widget(bar, 1);
    fb_paint_slot(btn0, ok_fg, base_bg, attrs);
    fb_paint_slot(btn1, cancel_fg, base_bg, attrs);
    if (btn0)
      vk_button_update(VK_BUTTON(btn0));
    if (btn1)
      vk_button_update(VK_BUTTON(btn1));
    vk_box_update(bar);
  }
}

static void
fb_sync_box_focus(struct fb_state_s *st)
{
  vk_box_t *bar;
  int box_slot;
  int button_i;

  if (!st || !st->fd)
    return;

  if (st->tab < 0)
    st->tab = FB_TAB_LIST;
  if (st->tab >= FB_TAB_COUNT)
    st->tab = FB_TAB_PATH;

  switch (st->tab)
  {
  case FB_TAB_PATH:
    box_slot = FB_BOX_PATH;
    button_i = 0;
    break;
  case FB_TAB_LIST:
    box_slot = FB_BOX_LIST;
    button_i = 0;
    break;
  case FB_TAB_OK:
    box_slot = FB_BOX_BUTTONS;
    button_i = 0;
    break;
  case FB_TAB_CANCEL:
  default:
    box_slot = FB_BOX_BUTTONS;
    button_i = 1;
    break;
  }

  vk_box_set_subfocus(VK_BOX(st->fd), box_slot);

  bar = VK_BOX(vk_box_get_widget(VK_BOX(st->fd), FB_BOX_BUTTONS));
  if (bar)
    vk_box_set_subfocus(bar, button_i);

  fb_style_widgets(st);
}

static void
fb_free(WIN *win)
{
  struct fb_state_s *st = win->data;

  if (st)
  {
    st->fd = NULL; /* destroyed with WIN via WIN_VK_FILEDIALOG */
    free(st);
  }
  win->data = NULL;
}

static int
fb_display(WIN *win)
{
  struct fb_state_s *st = win->data;
  int key = (int) win->c;

  if (!st || !st->fd)
    return 0;

  if (key == KEY_ESCAPE)
  {
    fb_free(win);
    return 0;
  }

  if (key == KEY_RESIZE)
  {
    /* Geometry is applied by do_window_resize → fb_resize; absorb. */
    return 1;
  }

  /*
   * Tab order: breadcrumb → picker → Okay → Cancel.
   * VDK only knows path/list box slots; OK/Cancel are app-layer stops.
   */
  if (key == '\t' || key == KEY_BTAB)
  {
    if (key == '\t')
      st->tab = (st->tab + 1) % FB_TAB_COUNT;
    else
      st->tab = (st->tab + FB_TAB_COUNT - 1) % FB_TAB_COUNT;
    fb_sync_box_focus(st);
    vk_filedialog_update(st->fd);
    cboard_ui_refresh();
    return 1;
  }

  if (st->tab == FB_TAB_OK || st->tab == FB_TAB_CANCEL)
  {
    if (fb_is_enter_key(key))
    {
      char name[NAME_MAX + 1];

      if (st->tab == FB_TAB_CANCEL)
      {
        fb_free(win);
        return 0;
      }
      /* Okay — accept list selection if it is a regular file. */
      if (fb_copy_selected(st, name, sizeof(name)) && fb_apply_name(st, name))
      {
        fb_free(win);
        return 0;
      }
      return 1;
    }
    /* Swallow other keys on buttons so list/path do not steal them. */
    return 1;
  }

  /*
   * List Enter: accept a file, or navigate into a directory.  Do this
   * before kmio so we never race activate+repopulate with get_selected.
   * Normalize KEY_ENTER to '\\n' for VDK (it only treats KEY_CRLF).
   */
  if (st->tab == FB_TAB_LIST && fb_is_enter_key(key))
  {
    char name[NAME_MAX + 1];

    if (!fb_copy_selected(st, name, sizeof(name)))
      return 1;

    if (fb_is_dir_entry(name))
    {
      fb_sync_box_focus(st);
      cboard_ui_push_key((cboard_widget_t *) st->fd, '\n');
      vk_filedialog_update(st->fd);
      cboard_ui_refresh();
      return 1;
    }

    if (fb_apply_name(st, name))
    {
      fb_free(win);
      return 0;
    }
    return 1;
  }

  /* Path or list (non-accept keys): drive VDK kmio. */
  fb_sync_box_focus(st);
  if (fb_is_enter_key(key))
    key = '\n';
  cboard_ui_push_key((cboard_widget_t *) st->fd, key);
  /* Re-read box focus if VDK moved path↔list ('/' or Enter on path). */
  {
    int slot = vk_box_get_subfocus(VK_BOX(st->fd));

    if (slot == FB_BOX_PATH && st->tab != FB_TAB_PATH)
    {
      st->tab = FB_TAB_PATH;
      fb_style_widgets(st);
    }
    else if (slot == FB_BOX_LIST && st->tab != FB_TAB_LIST)
    {
      st->tab = FB_TAB_LIST;
      fb_style_widgets(st);
    }
  }
  vk_filedialog_update(st->fd);
  cboard_ui_refresh();
  return 1;
}

static void
fb_resize(WIN *w)
{
  int nh = LINES - 4;
  int nw = COLS - 4;
  struct fb_state_s *st = w->data;

  if (nh < 12)
    nh = 12;
  if (nw < 40)
    nw = 40;
  w->rows = nh;
  w->cols = nw;
  w->posy = CALCPOSY(nh);
  w->posx = CALCPOSX(nw);
  if (w->vk)
  {
    /* VDK geometry only; cascade ends with one composite refresh. */
    w->w = cboard_ui_widget_resize(w->vk, nh, nw);
    cboard_ui_widget_move(w->vk, w->posy, w->posx);
    if (st && st->fd)
    {
      fb_sync_box_focus(st);
      fb_style_widgets(st);
      vk_filedialog_update(st->fd);
    }
  }
}

static int
fb_left_press(mmask_t bstate)
{
  return (bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)) != 0;
}

/* After fb_free, inject a key so game_loop runs display → returns 0 → destroy. */
static void
fb_request_close(void)
{
  pushkey = KEY_ESCAPE;
}

/*
 * Screen-coordinate mouse for the open file dialog WIN.
 * Maps path / list / OK / Cancel zones (same as tab stops).
 * Returns 1 if the event was over the dialog (consumed).
 */
int file_browser_mouse(WIN *win, int x, int y, mmask_t bstate)
{
  struct fb_state_s *st;
  int fx, fy, fw, fh;
  int lx, ly;
  vk_listbox_t *lb;
  vk_widget_t *path_w;
  vk_widget_t *list_fr;
  vk_box_t *bar;
  int px, py, pw, ph;

  if (!win || !win->data || win->vk_kind != WIN_VK_FILEDIALOG)
    return 0;

  st = win->data;
  if (!st->fd)
    return 0;

  vk_widget_get_position(VK_WIDGET(st->fd), &fx, &fy);
  vk_widget_get_metrics(VK_WIDGET(st->fd), &fw, &fh);
  if (x < fx || y < fy || x >= fx + fw || y >= fy + fh)
    return 0;

  lx = x - fx;
  ly = y - fy;

  /* Wheel over dialog → move list selection. */
  if (bstate & (BUTTON4_PRESSED | BUTTON5_PRESSED))
  {
    lb = vk_filedialog_get_file_list(st->fd);
    if (lb)
    {
      st->tab = FB_TAB_LIST;
      fb_sync_box_focus(st);
      if (bstate & BUTTON4_PRESSED)
        vk_listbox_set_prev(lb);
      else
        vk_listbox_set_next(lb);
      vk_filedialog_update(st->fd);
      cboard_ui_refresh();
    }
    return 1;
  }

  if (!fb_left_press(bstate))
    return 1;

  path_w = vk_box_get_widget(VK_BOX(st->fd), FB_BOX_PATH);
  list_fr = vk_box_get_widget(VK_BOX(st->fd), FB_BOX_LIST);
  bar = VK_BOX(vk_box_get_widget(VK_BOX(st->fd), FB_BOX_BUTTONS));

  if (path_w)
  {
    vk_widget_get_position(path_w, &px, &py);
    vk_widget_get_metrics(path_w, &pw, &ph);
    if (lx >= px && lx < px + pw && ly >= py && ly < py + ph)
    {
      st->tab = FB_TAB_PATH;
      fb_sync_box_focus(st);
      vk_filedialog_update(st->fd);
      cboard_ui_refresh();
      return 1;
    }
  }

  if (list_fr)
  {
    vk_widget_get_position(list_fr, &px, &py);
    vk_widget_get_metrics(list_fr, &pw, &ph);
    if (lx >= px && lx < px + pw && ly >= py && ly < py + ph)
    {
      int row, scroll, n, ily;
      char name[NAME_MAX + 1];

      st->tab = FB_TAB_LIST;
      fb_sync_box_focus(st);
      lb = vk_filedialog_get_file_list(st->fd);
      ily = ly - py - 1; /* frame border inset */
      if (lb && ily >= 0)
      {
        scroll = vk_listbox_get_scroll_pos(lb);
        n = vk_listbox_get_item_count(lb);
        row = scroll + ily;
        if (row >= 0 && row < n)
        {
          vk_listbox_set_curr(lb, row);
          vk_filedialog_update(st->fd);
          if (bstate & BUTTON1_DOUBLE_CLICKED)
          {
            if (fb_copy_selected(st, name, sizeof(name)))
            {
              if (fb_is_dir_entry(name))
              {
                cboard_ui_push_key((cboard_widget_t *) st->fd,
                                   '\n');
                vk_filedialog_update(st->fd);
              }
              else if (fb_apply_name(st, name))
              {
                fb_free(win);
                fb_request_close();
                cboard_ui_refresh();
                return 1;
              }
            }
          }
        }
      }
      cboard_ui_refresh();
      return 1;
    }
  }

  if (bar)
  {
    vk_widget_t *btn0, *btn1;
    int bx, by, bw, bh;
    int rel_x;

    vk_widget_get_position(VK_WIDGET(bar), &px, &py);
    vk_widget_get_metrics(VK_WIDGET(bar), &pw, &ph);
    if (lx >= px && lx < px + pw && ly >= py && ly < py + ph)
    {
      rel_x = lx - px;
      btn0 = vk_box_get_widget(bar, 0);
      btn1 = vk_box_get_widget(bar, 1);
      if (btn0)
      {
        vk_widget_get_position(btn0, &bx, &by);
        vk_widget_get_metrics(btn0, &bw, &bh);
        if (rel_x >= bx && rel_x < bx + bw)
        {
          char name[NAME_MAX + 1];

          st->tab = FB_TAB_OK;
          fb_sync_box_focus(st);
          if (fb_copy_selected(st, name, sizeof(name)) && fb_apply_name(st, name))
          {
            fb_free(win);
            fb_request_close();
          }
          else
          {
            vk_filedialog_update(st->fd);
            cboard_ui_refresh();
          }
          return 1;
        }
      }
      if (btn1)
      {
        vk_widget_get_position(btn1, &bx, &by);
        vk_widget_get_metrics(btn1, &bw, &bh);
        if (rel_x >= bx && rel_x < bx + bw)
        {
          st->tab = FB_TAB_CANCEL;
          fb_sync_box_focus(st);
          fb_free(win);
          fb_request_close();
          return 1;
        }
      }
      st->tab = FB_TAB_OK;
      fb_sync_box_focus(st);
      vk_filedialog_update(st->fd);
      cboard_ui_refresh();
      return 1;
    }
  }

  return 1;
}

/*
 * Open a VDK file dialog as a modal over the current input field.
 * On accept, the selected path is written into the parent input buffer.
 */
void file_browser(void *arg)
{
  struct input_s *in = arg;
  struct fb_state_s *st;
  vk_filedialog_t *fd;
  WIN *win;
  char path[FILENAME_MAX] = {0};
  char *p;
  int h, w, y, x;

  if (!in)
    return;

  if (!oldwd && config.savedirectory)
  {
    if ((p = pathfix(config.savedirectory)) == NULL)
      return;
    strncpy(path, p, sizeof(path) - 1);
    if (access(path, R_OK) == -1)
    {
      cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s", path, strerror(errno));
      if (getcwd(path, sizeof(path)) == NULL)
        path[0] = '\0';
    }
  }
  else if (!oldwd)
  {
    if (getcwd(path, sizeof(path) - 1) == NULL)
      path[0] = '\0';
  }
  else
    strncpy(path, oldwd, sizeof(path) - 1);

  h = LINES - 4;
  w = COLS - 4;
  if (h < 12)
    h = 12;
  if (w < 40)
    w = 40;
  y = CALCPOSY(h);
  x = CALCPOSX(w);

  fd = vk_filedialog_create(w, h, VK_BORDER_SINGLE, false);
  if (!fd)
  {
    cmessage(ERROR_STR, ANY_KEY_STR, "%s",
             _("Could not create file dialog"));
    return;
  }

  if (config.pattern && config.pattern[0] && config.pattern[0] != '*')
  {
    /* pattern is a glob like "*.pgn"; filedialog wants extensions w/o dots */
    const char *pat = config.pattern;
    const char *dot = strrchr(pat, '.');

    if (dot && dot[1])
      vk_filedialog_set_filter(fd, dot + 1);
  }
  if (path[0])
    vk_filedialog_set_path(fd, path);

  cboard_ui_widget_attach((cboard_widget_t *) fd, y, x);
  cboard_ui_widget_raise((cboard_widget_t *) fd);

  st = Calloc(1, sizeof(struct fb_state_s));
  st->in = in;
  /* Top of stack is the open input dialog that invoked us. */
  st->parent = window_top();
  st->fd = fd;
  st->tab = FB_TAB_LIST;
  fb_sync_box_focus(st);
  vk_filedialog_update(fd);

  win = window_adopt(_("File Browser"), (void *) fd, WIN_VK_FILEDIALOG,
                     h, w, y, x, fb_display, st, NULL, fb_resize);
  win->app_kind = WIN_APP_FILEBROWSER;
  if (win->w)
    keypad(win->w, TRUE);
  cboard_ui_refresh();
}
