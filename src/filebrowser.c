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

/* Filedialog is a 3-slot vertical box: path, list frame, button bar. */
enum
{
  FB_SLOT_PATH = 0,
  FB_SLOT_LIST = 1,
  FB_SLOT_BUTTONS = 2,
  FB_SLOT_COUNT = 3
};

struct fb_state_s
{
  struct input_s *in;		/* parent input dialog state */
  vk_filedialog_t *fd;
  int focus_slot;		/* app-layer tab stop (path / list / buttons) */
  int button_focus;		/* 0 = OK, 1 = Cancel when on button bar */
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

static void
fb_style_widgets (vk_filedialog_t *fd)
{
  vk_listbox_t *lb;
  vk_widget_t *path_w;
  vk_widget_t *list_frame;
  short fg = config.color[CONF_MENU].fg;
  short bg = config.color[CONF_MENU].bg;
  attr_t attrs = config.color[CONF_MENU].attrs;

  vk_filedialog_set_colors (fd, fg, bg);
  vk_filedialog_set_highlight (fd,
			       config.color[CONF_MENUS].fg,
			       config.color[CONF_MENUS].bg);
  vk_filedialog_set_button_colors (fd, fg, bg);
  vk_filedialog_set_button_attrs (fd, attrs);

  /* Bright white text on path breadcrumb and file list (public box slots). */
  path_w = vk_box_get_widget (VK_BOX (fd), FB_SLOT_PATH);
  if (path_w)
    {
      vk_widget_set_colors (path_w, fg, bg);
      vk_widget_set_attrs (path_w, attrs);
    }

  list_frame = vk_box_get_widget (VK_BOX (fd), FB_SLOT_LIST);
  if (list_frame)
    {
      vk_widget_set_colors (list_frame, fg, bg);
      vk_widget_set_attrs (list_frame, attrs);
    }

  lb = vk_filedialog_get_file_list (fd);
  if (lb)
    {
      vk_widget_set_colors (VK_WIDGET (lb), fg, bg);
      vk_widget_set_attrs (VK_WIDGET (lb), attrs);
      vk_listbox_set_highlight_attrs (lb, config.color[CONF_MENUS].attrs);
    }

  vk_widget_set_attrs (VK_WIDGET (fd), attrs);
}

static void
fb_sync_box_focus (struct fb_state_s *st)
{
  vk_box_t *bar;

  if (!st || !st->fd)
    return;

  if (st->focus_slot < 0)
    st->focus_slot = FB_SLOT_LIST;
  if (st->focus_slot >= FB_SLOT_COUNT)
    st->focus_slot = FB_SLOT_PATH;

  vk_box_set_subfocus (VK_BOX (st->fd), st->focus_slot);

  bar = VK_BOX (vk_box_get_widget (VK_BOX (st->fd), FB_SLOT_BUTTONS));
  if (bar)
    {
      if (st->button_focus < 0)
	st->button_focus = 0;
      if (st->button_focus > 1)
	st->button_focus = 1;
      vk_box_set_subfocus (bar, st->button_focus);
    }
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
      vk_filedialog_update (st->fd);
      cboard_ui_refresh ();
      return 1;
    }

  /*
   * Tab stops at the app layer (path → list → buttons).  VDK filedialog
   * only switches path↔list via '/' / Enter; we own TAB/BTAB and button
   * bar left/right here without changing libviper.
   */
  if (key == '\t' || key == KEY_BTAB)
    {
      if (key == '\t')
	st->focus_slot = (st->focus_slot + 1) % FB_SLOT_COUNT;
      else
	st->focus_slot = (st->focus_slot + FB_SLOT_COUNT - 1) % FB_SLOT_COUNT;
      fb_sync_box_focus (st);
      vk_filedialog_update (st->fd);
      cboard_ui_refresh ();
      return 1;
    }

  if (st->focus_slot == FB_SLOT_BUTTONS)
    {
      if (key == KEY_LEFT)
	{
	  st->button_focus = 0;
	  fb_sync_box_focus (st);
	  vk_filedialog_update (st->fd);
	  cboard_ui_refresh ();
	  return 1;
	}
      if (key == KEY_RIGHT)
	{
	  st->button_focus = 1;
	  fb_sync_box_focus (st);
	  vk_filedialog_update (st->fd);
	  cboard_ui_refresh ();
	  return 1;
	}
      if (key == '\n' || key == KEY_ENTER)
	{
	  if (st->button_focus == 1)
	    {
	      /* Cancel */
	      fb_free (win);
	      return 0;
	    }
	  /* OK — accept current list selection if it is a file. */
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
      /* Other keys while on buttons: ignore (do not steal list/path). */
      return 1;
    }

  /* Path or list: keep filedialog subfocus in sync, then drive kmio. */
  fb_sync_box_focus (st);
  cboard_ui_push_key ((cboard_widget_t *) st->fd, key);
  /* Re-read focus in case '/' or Enter moved path↔list inside VDK. */
  {
    int slot = vk_box_get_subfocus (VK_BOX (st->fd));

    if (slot >= 0 && slot < FB_SLOT_COUNT)
      st->focus_slot = slot;
  }
  vk_filedialog_update (st->fd);
  cboard_ui_refresh ();

  if (st->focus_slot == FB_SLOT_LIST
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
	  fb_style_widgets (st->fd);
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

  fb_style_widgets (fd);
  vk_filedialog_update (fd);

  cboard_ui_widget_attach ((cboard_widget_t *) fd, y, x);
  cboard_ui_widget_raise ((cboard_widget_t *) fd);

  st = Calloc (1, sizeof (struct fb_state_s));
  st->in = in;
  st->fd = fd;
  st->focus_slot = FB_SLOT_LIST;
  st->button_focus = 0;
  fb_sync_box_focus (st);

  win = window_adopt (_("File Browser"), (void *) fd, WIN_VK_FILEDIALOG,
		      h, w, y, x, fb_display, st, NULL, fb_resize);
  if (win->w)
    keypad (win->w, TRUE);
  cboard_ui_refresh ();
}
