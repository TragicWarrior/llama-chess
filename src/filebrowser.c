/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2007-2024 Ben Kibbey <bjk@luxsci.net>

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

struct fb_state_s
{
  struct input_s *in;		/* parent input dialog state */
  vk_filedialog_t *fd;
  int done;
  int accepted;
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

  /* Drive built-in filedialog kmio (list, path, OK/Cancel). */
  cboard_ui_push_key ((cboard_widget_t *) st->fd, key);
  vk_filedialog_update (st->fd);
  cboard_ui_refresh ();

  if (key == '\n' || key == KEY_ENTER)
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
      vk_filedialog_update ((vk_filedialog_t *) w->vk);
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

  /* Match menu/input: black body, cyan accents. */
  vk_filedialog_set_colors (fd,
			    config.color[CONF_MENU].fg,
			    config.color[CONF_MENU].bg);
  vk_filedialog_set_highlight (fd,
			       config.color[CONF_MENUS].fg,
			       config.color[CONF_MENUS].bg);
  vk_filedialog_set_button_colors (fd,
				   config.color[CONF_IBORDER].fg,
				   config.color[CONF_MENU].bg);
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
  vk_filedialog_update (fd);

  cboard_ui_widget_attach ((cboard_widget_t *) fd, y, x);
  cboard_ui_widget_raise ((cboard_widget_t *) fd);

  st = Calloc (1, sizeof (struct fb_state_s));
  st->in = in;
  st->fd = fd;

  win = window_adopt (_("File Browser"), (void *) fd, WIN_VK_FILEDIALOG,
		      h, w, y, x, fb_display, st, NULL, fb_resize);
  if (win->w)
    keypad (win->w, TRUE);
  cboard_ui_refresh ();
}
