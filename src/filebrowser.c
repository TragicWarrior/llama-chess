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

struct file_s **files;		/* kept for ABI with older code paths */
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
  struct input_s *in;		/* parent input dialog state */
  vk_filedialog_t *fd;
  int tab;			/* FB_TAB_* */
};

static void
fb_apply_selection (struct fb_state_s *st)
{
  const char *path;
  const char *selected;
  char fullpath[FILENAME_MAX];
  size_t len;

  if (!st || !st->fd || !st->in)
    return;

  path = vk_filedialog_get_path (st->fd);
  selected = vk_filedialog_get_selected (st->fd);
  if (!path || !selected || !selected[0])
    return;

  len = strlen (selected);
  if (selected[len - 1] == '/' || strcmp (selected, "..") == 0)
    return;

  if (strcmp (path, "/") == 0)
    snprintf (fullpath, sizeof (fullpath), "/%s", selected);
  else
    snprintf (fullpath, sizeof (fullpath), "%s/%s", path, selected);

  input_set_buf (st->in, fullpath);

  if (oldwd)
    free (oldwd);
  oldwd = strdup (path);
}

/* Focused control: bright yellow FG; idle: bright white (CONF_MENU). */
#define FB_FOCUS_FG	COLOR_YELLOW

static void
fb_paint_slot (vk_widget_t *w, short fg, short bg, attr_t attrs)
{
  if (!w)
    return;
  vk_widget_set_colors (w, fg, bg);
  vk_widget_set_attrs (w, attrs);
}

static void
fb_style_widgets (struct fb_state_s *st)
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

  vk_filedialog_set_colors (fd, base_fg, base_bg);
  vk_filedialog_set_button_colors (fd, base_fg, base_bg);
  vk_filedialog_set_button_attrs (fd, attrs);
  vk_widget_set_attrs (VK_WIDGET (fd), attrs);

  path_w = vk_box_get_widget (VK_BOX (fd), FB_BOX_PATH);
  fb_paint_slot (path_w, path_fg, base_bg, attrs);

  /* List body always bright white; only the selected row turns yellow. */
  list_frame = vk_box_get_widget (VK_BOX (fd), FB_BOX_LIST);
  fb_paint_slot (list_frame, base_fg, base_bg, attrs);

  lb = vk_filedialog_get_file_list (fd);
  if (lb)
    {
      fb_paint_slot (VK_WIDGET (lb), base_fg, base_bg, attrs);
      if (st->tab == FB_TAB_LIST)
	{
	  vk_listbox_set_highlight (lb, FB_FOCUS_FG, COLOR_BLUE);
	  vk_listbox_set_highlight_attrs (lb, A_BOLD);
	}
      else
	{
	  vk_listbox_set_highlight (lb,
				    config.color[CONF_MENUS].fg,
				    config.color[CONF_MENUS].bg);
	  vk_listbox_set_highlight_attrs (lb,
					  config.color[CONF_MENUS].attrs
					  | A_BOLD);
	}
    }

  bar = VK_BOX (vk_box_get_widget (VK_BOX (fd), FB_BOX_BUTTONS));
  if (bar)
    {
      btn0 = vk_box_get_widget (bar, 0);
      btn1 = vk_box_get_widget (bar, 1);
      fb_paint_slot (btn0, ok_fg, base_bg, attrs);
      fb_paint_slot (btn1, cancel_fg, base_bg, attrs);
      if (btn0)
	vk_button_update (VK_BUTTON (btn0));
      if (btn1)
	vk_button_update (VK_BUTTON (btn1));
      vk_box_update (bar);
    }
}

static void
fb_sync_box_focus (struct fb_state_s *st)
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

  vk_box_set_subfocus (VK_BOX (st->fd), box_slot);

  bar = VK_BOX (vk_box_get_widget (VK_BOX (st->fd), FB_BOX_BUTTONS));
  if (bar)
    vk_box_set_subfocus (bar, button_i);

  fb_style_widgets (st);
}

static void
fb_free (WIN * win)
{
  struct fb_state_s *st = win->data;

  if (st)
    {
      st->fd = NULL;		/* destroyed with WIN via WIN_VK_FILEDIALOG */
      free (st);
    }
  win->data = NULL;
}

static int
fb_display (WIN * win)
{
  struct fb_state_s *st = win->data;
  int key = (int) win->c;

  if (!st || !st->fd)
    return 0;

  if (key == KEY_ESCAPE)
    {
      fb_free (win);
      return 0;
    }

  if (key == KEY_RESIZE)
    {
      fb_style_widgets (st);
      vk_filedialog_update (st->fd);
      cboard_ui_refresh ();
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
      fb_sync_box_focus (st);
      vk_filedialog_update (st->fd);
      cboard_ui_refresh ();
      return 1;
    }

  if (st->tab == FB_TAB_OK || st->tab == FB_TAB_CANCEL)
    {
      if (key == '\n' || key == KEY_ENTER)
	{
	  if (st->tab == FB_TAB_CANCEL)
	    {
	      fb_free (win);
	      return 0;
	    }
	  /* Okay — accept current list selection if it is a file. */
	  {
	    const char *selected = vk_filedialog_get_selected (st->fd);

	    if (selected && selected[0]
		&& selected[strlen (selected) - 1] != '/'
		&& strcmp (selected, "..") != 0)
	      {
		fb_apply_selection (st);
		fb_free (win);
		return 0;
	      }
	  }
	  return 1;
	}
      /* Swallow other keys on buttons so list/path do not steal them. */
      return 1;
    }

  /* Path or list: keep filedialog subfocus in sync, then drive kmio. */
  fb_sync_box_focus (st);
  cboard_ui_push_key ((cboard_widget_t *) st->fd, key);
  /* Re-read box focus if VDK moved path↔list ('/' or Enter on path). */
  {
    int slot = vk_box_get_subfocus (VK_BOX (st->fd));

    if (slot == FB_BOX_PATH && st->tab != FB_TAB_PATH)
      {
	st->tab = FB_TAB_PATH;
	fb_style_widgets (st);
      }
    else if (slot == FB_BOX_LIST && st->tab != FB_TAB_LIST)
      {
	st->tab = FB_TAB_LIST;
	fb_style_widgets (st);
      }
  }
  vk_filedialog_update (st->fd);
  cboard_ui_refresh ();

  if (st->tab == FB_TAB_LIST
      && (key == '\n' || key == KEY_ENTER))
    {
      const char *selected = vk_filedialog_get_selected (st->fd);

      if (selected && selected[0]
	  && selected[strlen (selected) - 1] != '/'
	  && strcmp (selected, "..") != 0)
	{
	  fb_apply_selection (st);
	  fb_free (win);
	  return 0;
	}
    }

  return 1;
}

static void
fb_resize (WIN * w)
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
  w->posy = CALCPOSY (nh);
  w->posx = CALCPOSX (nw);
  if (w->vk)
    {
      w->w = cboard_ui_widget_resize (w->vk, nh, nw);
      cboard_ui_widget_move (w->vk, w->posy, w->posx);
      if (st && st->fd)
	{
	  fb_sync_box_focus (st);
	  vk_filedialog_update (st->fd);
	}
      cboard_ui_refresh ();
    }
}

/*
 * Open a VDK file dialog as a modal over the current input field.
 * On accept, the selected path is written into the parent input buffer.
 */
void
file_browser (void *arg)
{
  struct input_s *in = arg;
  struct fb_state_s *st;
  vk_filedialog_t *fd;
  WIN *win;
  char path[FILENAME_MAX] = { 0 };
  char *p;
  int h, w, y, x;

  if (!in)
    return;

  if (!oldwd && config.savedirectory)
    {
      if ((p = pathfix (config.savedirectory)) == NULL)
	return;
      strncpy (path, p, sizeof (path) - 1);
      if (access (path, R_OK) == -1)
	{
	  cmessage (ERROR_STR, ANY_KEY_STR, "%s: %s", path, strerror (errno));
	  if (getcwd (path, sizeof (path)) == NULL)
	    path[0] = '\0';
	}
    }
  else if (!oldwd)
    {
      if (getcwd (path, sizeof (path) - 1) == NULL)
	path[0] = '\0';
    }
  else
    strncpy (path, oldwd, sizeof (path) - 1);

  h = LINES - 4;
  w = COLS - 4;
  if (h < 12)
    h = 12;
  if (w < 40)
    w = 40;
  y = CALCPOSY (h);
  x = CALCPOSX (w);

  fd = vk_filedialog_create (w, h, VK_BORDER_SINGLE, false);
  if (!fd)
    {
      cmessage (ERROR_STR, ANY_KEY_STR, "%s",
		_("Could not create file dialog"));
      return;
    }

  if (config.pattern && config.pattern[0]
      && config.pattern[0] != '*')
    {
      /* pattern is a glob like "*.pgn"; filedialog wants extensions w/o dots */
      const char *pat = config.pattern;
      const char *dot = strrchr (pat, '.');

      if (dot && dot[1])
	vk_filedialog_set_filter (fd, dot + 1);
    }
  if (path[0])
    vk_filedialog_set_path (fd, path);

  cboard_ui_widget_attach ((cboard_widget_t *) fd, y, x);
  cboard_ui_widget_raise ((cboard_widget_t *) fd);

  st = Calloc (1, sizeof (struct fb_state_s));
  st->in = in;
  st->fd = fd;
  st->tab = FB_TAB_LIST;
  fb_sync_box_focus (st);
  vk_filedialog_update (fd);

  win = window_adopt (_("File Browser"), (void *) fd, WIN_VK_FILEDIALOG,
		      h, w, y, x, fb_display, st, NULL, fb_resize);
  if (win->w)
    keypad (win->w, TRUE);
  cboard_ui_refresh ();
}
