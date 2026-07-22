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
#include <time.h>
#include <pwd.h>

#ifdef HAVE_GLOB_H
#include <glob.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#include "misc.h"
#include "common.h"
#include "window.h"
#include "message.h"
#include "menu.h"
#include "input.h"
#include "filebrowser.h"
#include "common.h"
#include "conf.h"
#include "keys.h"
#include "rcfile.h"

struct file_s **files;
char *oldwd;

static void
free_file_browser ()
{
  int i;

  if (!files)
    return;

  for (i = 0; files[i]; i++)
    {
      free (files[i]->name);
      free (files[i]->path);
      free (files[i]->st);
      free (files[i]);
    }

  free (files);
  files = NULL;
}

static struct menu_item_s **
get_file_items (WIN * win)
{
  struct menu_input_s *m = win->data;
  struct input_s *in = m->data;
  char *path = in->arg;
  struct menu_item_s **items = m->items;
  char *p;
  char pattern[255];
  int i, n = 0;
  struct stat st;
  int which = 2;
  int len;
  int x = GLOB_ERR;
#ifdef HAVE_GLOB_NOMATCH
  int ret;
#endif
  glob_t g;

  /*
   * First find hidden directories in the working directory. Then non-hidden
   * directories, then apply the config.pattern to regular files.
   */
  snprintf (pattern, sizeof (pattern), "%s/.?*", path);

  if (items)
    {
      for (i = 0; items[i]; i++)
	free (items[i]);
    }

  free (items);
  items = m->items = NULL;
  free_file_browser ();
  m->nofree = 1;

new_glob:
#ifdef HAVE_GLOB_NOMATCH
  if ((ret = glob (pattern, x, NULL, &g)) != 0 && ret != GLOB_NOMATCH)
    {
#else
  if (glob (pattern, x, NULL, &g) != 0)
    {
#endif
      cmessage (ERROR_STR, ANY_KEY_STR, "glob() failed:\n%s", pattern);
      return NULL;
    }

  for (i = 0; i < g.gl_pathc; i++)
    {
      struct tm *tp;
      char tbuf[16];
      char sbuf[64];

      if (stat (g.gl_pathv[i], &st) == -1)
	continue;

      if ((p = strrchr (g.gl_pathv[i], '/')) != NULL)
	p++;
      else
	p = g.gl_pathv[i];

      if (which)
	{
	  if (!S_ISDIR (st.st_mode))
	    continue;

	  if (p[0] == '.' && p[1] == 0)
	    continue;
	}
      else
	{
	  if (S_ISDIR (st.st_mode))
	    continue;
	}

      files = Realloc (files, (n + 2) * sizeof (struct file_s *));
      files[n] = Malloc (sizeof (struct file_s));
      files[n]->path = strdup (g.gl_pathv[i]);
      len = strlen (p) + 2;
      files[n]->name = Malloc (len);
      strcpy (files[n]->name, p);

      if (S_ISDIR (st.st_mode))
	{
	  files[n]->name[len - 2] = '/';
	  files[n]->name[len - 1] = 0;
	  p = files[n]->path + strlen (files[n]->path) - 1;

	  if (*p == '.' && *(p - 1) == '.' && *(p - 2) == '/')
	    {
	      p -= 2;
	      *p = 0;

	      if (strlen (files[n]->path))
		{
		  while (*p != '/')
		    p--;

		  *p = 0;
		}

	      if (!strlen (files[n]->path))
		{
		  p = files[n]->path;
		  *p++ = '/';
		  *p = 0;
		}
	    }
	}

      tp = localtime (&st.st_mtime);
      strftime (tbuf, sizeof (tbuf), "%b %d %T", tp);
      snprintf (sbuf, sizeof (sbuf), "%s %6i", tbuf, (int) st.st_size);
      files[n]->st = strdup (sbuf);
      n++;
    }

  which--;
  files[n] = NULL;

  if (which > 0)
    {
      snprintf (pattern, sizeof (pattern), "%s/*", path);
      globfree (&g);
      goto new_glob;
    }
  else if (which == 0)
    {
      snprintf (pattern, sizeof (pattern), "%s/%s", path, config.pattern);
      globfree (&g);
      goto new_glob;
    }

  globfree (&g);

  for (i = 0; files[i]; i++)
    {
      items = Realloc (items, (i + 2) * sizeof (struct menu_item_s *));
      items[i] = Malloc (sizeof (struct menu_item_s));
      items[i]->name = files[i]->name;
      items[i]->value = files[i]->st;
      items[i]->selected = 0;
    }

  free (win->title);
  win->title = strdup (path);
  items[i] = NULL;
  m->items = items;
  return items;
}

static void
file_browser_help (struct menu_input_s *m)
{
  message (_("File Browser Keys"), ANY_KEY_STR, "%s",
	   _("    UP/DOWN - previous/next menu item\n"
	     "   HOME/END - first/last menu item\n"
	     "  PGDN/PGUP - next/previous page\n"
	     "  a-zA-Z0-9 - jump to item\n"
	     "      ENTER - select item\n"
	     "          ~ - change to home directory\n"
	     "     CTRL-e - change filename expression\n"
	     "     ESCAPE - abort"));
}

static void
file_browser_select (struct menu_input_s *m)
{
  struct input_s *in = m->data;
  char *path = in->arg;
  struct stat st;

  if (stat (files[m->selected]->path, &st) == -1)
    {
      message (ERROR_STR, ANY_KEY_STR, "%s", strerror (errno));
      return;
    }

  if (S_ISDIR (st.st_mode))
    {
      if (access (files[m->selected]->path, R_OK) != 0)
	{
	  cmessage (files[m->selected]->path, ANY_KEY_STR, "%s",
		    strerror (errno));
	  return;
	}

      free (path);
      path = strdup (files[m->selected]->path);
      in->arg = path;
      m->selected = 0;

      if (oldwd)
	free (oldwd);

      oldwd = strdup (path);
      return;
    }

  strncpy (in->buf, files[m->selected]->path, sizeof (in->buf) - 1);
  in->buf[sizeof (in->buf) - 1] = 0;
  set_field_buffer (in->fields[0], 0, in->buf);
  pushkey = -1;
}

static void
file_browser_finalize (WIN * win)
{
  struct input_s *in = win->data;

  free (in->arg);
  free_file_browser ();
}

static void
file_browser_home (struct menu_input_s *m)
{
  struct input_s *in = m->data;
  char *path = in->arg;
  struct passwd *pw;

  free (path);
  pw = getpwuid (getuid ());
  path = strdup (pw->pw_dir);
  in->arg = path;
  m->selected = 0;

  if (oldwd)
    free (oldwd);

  oldwd = strdup (path);
}

static void
do_file_browser_expression_finalize (WIN * win)
{
  struct input_data_s *in = win->data;

  if (!in->str)
    return;

  if (config.pattern)
    free (config.pattern);

  config.pattern = strdup (in->str);
  free (in->str);
  free (in);
  pushkey = REFRESH_MENU;
}

static void
file_browser_expression (struct menu_input_s *m)
{
  struct input_data_s *in;

  in = Calloc (1, sizeof (struct input_data_s));
  in->efunc = do_file_browser_expression_finalize;
  construct_input (_("Filename Expression"), config.pattern, 1, 1, NULL, NULL,
		   NULL, 0, in, -1, NULL, -1);
}

static void
file_browser_abort (struct menu_input_s *m)
{
  pushkey = -1;
}

static void
file_browser_print (WIN * win)
{
  int i, len = 0;
  struct menu_input_s *m = win->data;

  for (i = 0; m->items[i]; i++)
    {
      int n = strlen (m->items[i]->value);

      if (len < n)
	len = n;
    }

  mvwprintw (win->w, m->print_line, 1, "%*s %-*s", len, m->item->value,
	     win->cols - len - 3, m->item->name);
}

void
file_browser (void *arg)
{
  struct menu_key_s **keys = NULL;
  struct input_s *in = arg;
  char *p;
  char path[FILENAME_MAX] = { 0 };

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

  in->arg = strdup (path);
  add_menu_key (&keys, '\n', file_browser_select);
  add_menu_key (&keys, keycode_lookup (global_keys, do_global_help),
                file_browser_help);
  add_menu_key (&keys, '~', file_browser_home);
  add_menu_key (&keys, CTRL_KEY ('e'), file_browser_expression);
  add_menu_key (&keys, KEY_ESCAPE, file_browser_abort);
  construct_menu (LINES - 4, 0, -1, -1, NULL, 0, get_file_items, keys, in,
		  file_browser_print, file_browser_finalize, NULL);
  return;
}
