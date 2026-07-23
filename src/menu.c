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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include <vdk.h>

#include "common.h"
#include "conf.h"
#include "colors.h"
#include "strings.h"
#include "misc.h"
#include "window.h"
#include "menu.h"
#include "keys.h"
#include "rcfile.h"
#include "ui_screen.h"

/* Format one menu item for listbox display (name, value, selected mark). */
static char *
format_item_label (struct menu_input_s *m, struct menu_item_s *it, int width)
{
  char *label;
  char *name = it->name ? it->name : "";
  char *value = it->value;
  const char *empty = _("empty value");
  int mark = it->selected ? 1 : 0;

  if (m->name_only || !value)
    {
      if (asprintf (&label, "%s%s", mark ? "* " : "", name) < 0)
	label = strdup (name);
    }
  else
    {
      /* "name: value" with optional selection mark (tags, country codes). */
      if (asprintf (&label, "%s%s: %s", mark ? "* " : "", name,
		    value[0] ? value : empty) < 0)
	label = strdup (name);
    }

  (void) width;
  return label;
}

static void
menu_update_status (struct menu_input_s *m)
{
  vk_label_t *lab = (vk_label_t *) m->status_label;
  char buf[COLS];

  if (!lab)
    return;

  if (m->total < 1)
    snprintf (buf, sizeof (buf), _("No items"));
  else
    snprintf (buf, sizeof (buf), _("Item %i %s %i  Type %ls for help"),
	      m->selected + 1, _("of"), m->total,
	      key_lookup (global_keys, do_global_help));

  vk_label_set_text (lab, buf);
  vk_label_update (lab);
}

/*
 * Composite listbox/status into the box, then the box into the window.
 * vk_listbox_update / vk_label_update only paint each widget's own canvas;
 * without vk_box_update those canvases never reach the window frame (menus
 * would open with a border and an empty interior).
 */
static void
menu_composite (WIN * win)
{
  struct menu_input_s *m;

  if (!win || !win->vk)
    return;

  m = win->data;
  if (m && m->vbox)
    vk_box_update (VK_BOX (m->vbox));
  vk_window_update (VK_WINDOW (win->vk));
  cboard_ui_refresh ();
}

static void
menu_sync_selection (struct menu_input_s *m)
{
  vk_listbox_t *lb = (vk_listbox_t *) m->listbox;

  if (!lb || !m->items)
    return;

  m->selected = vk_listbox_get_curr (lb);
  if (m->selected < 0)
    m->selected = 0;
  if (m->total > 0 && m->selected >= m->total)
    m->selected = m->total - 1;

  m->top = vk_listbox_get_scroll_pos (lb);
  if (m->selected >= 0 && m->items[m->selected])
    m->item = m->items[m->selected];

  menu_update_status (m);
}

static void
menu_populate_listbox (WIN * win)
{
  struct menu_input_s *m = win->data;
  vk_listbox_t *lb = (vk_listbox_t *) m->listbox;
  int i, keep;
  int inner_w, inner_h;

  if (!lb)
    return;

  for (i = 0; m->items && m->items[i]; i++)
    ;
  m->total = i;

  keep = m->selected;
  if (keep < 0)
    keep = 0;
  if (m->total > 0 && keep >= m->total)
    keep = m->total - 1;

  vk_widget_get_metrics (VK_WIDGET (lb), &inner_w, &inner_h);
  if (inner_w < 1)
    inner_w = win->cols > 4 ? win->cols - 4 : 10;

  vk_listbox_reset (lb);

  for (i = 0; m->items && m->items[i]; i++)
    {
      char *label = format_item_label (m, m->items[i], inner_w);

      vk_listbox_add_item (lb, label, NULL, NULL);
      free (label);
    }

  if (m->total > 0)
    vk_listbox_set_curr (lb, keep);

  m->selected = keep;
  if (m->total > 0 && m->items[keep])
    m->item = m->items[keep];

  m->top = vk_listbox_get_scroll_pos (lb);
  menu_update_status (m);
  vk_listbox_update (lb);
  menu_composite (win);
}

static void
fix_menu_geometry (WIN * win)
{
  struct menu_input_s *m = win->data;
  char buf[COLS - 4];
  int i = 0;
  wchar_t *wc;
  size_t len;
  int n, nlen = 0, vlen = 0;

  for (i = 0; m->items && m->items[i]; i++)
    ;
  m->total = i;

  snprintf (buf, sizeof (buf), _("Item %i %s %i  Type %ls for help"),
	    m->selected + 1, _("of"), m->total > 0 ? m->total : 0,
	    key_lookup (global_keys, do_global_help));

  if (!m->cstatic)
    {
      win->cols = 0;

      for (i = 0; m->items && m->items[i]; i++)
	{
	  wc = str_to_wchar (m->items[i]->name);
	  n = wcslen (wc);
	  free (wc);
	  if (nlen < n)
	    nlen = n;

	  if (m->items[i]->value)
	    {
	      wc = str_to_wchar (m->items[i]->value);
	      n = wcslen (wc);
	      if (vlen < n)
		vlen = n;
	      n = vlen + nlen + 4;	/* ": " and optional "* " */
	      free (wc);
	    }
	  else
	    n = nlen + 2;

	  if (win->cols < n)
	    win->cols = n;
	}
    }

  if (!m->rstatic)
    {
      win->rows = m->total + 4;	/* border + title + status */
      if (win->title)
	win->rows++;
    }

  if (!m->cstatic)
    win->cols += 4;

  if (!m->rstatic && win->rows > MAX_MENU_HEIGHT)
    win->rows = MAX_MENU_HEIGHT;

  wc = str_to_wchar (buf);
  len = wcslen (wc);
  free (wc);

  if (win->cols < (int) len + 2)
    win->cols = (int) len + 2;

  if (win->cols > MAX_MENU_WIDTH)
    win->cols = MAX_MENU_WIDTH;

  if (win->cols < 20)
    win->cols = 20;
  if (win->rows < 6)
    win->rows = 6;

  win->posy = (m->ystatic == -1) ? CALCPOSY (win->rows) : m->ystatic;
  win->posx = (m->xstatic == -1) ? CALCPOSX (win->cols) : m->xstatic;
}

static void
menu_apply_geometry (WIN * win)
{
  struct menu_input_s *m = win->data;
  vk_listbox_t *lb = (vk_listbox_t *) m->listbox;
  vk_label_t *lab = (vk_label_t *) m->status_label;
  int inner_w, list_h;

  if (!win->vk)
    return;

  win->w = cboard_ui_widget_resize (win->vk, win->rows, win->cols);
  cboard_ui_widget_move (win->vk, win->posy, win->posx);
  cboard_ui_widget_raise (win->vk);

  inner_w = win->cols - 2;
  list_h = win->rows - 4;	/* title/border + status line */
  if (inner_w < 1)
    inner_w = 1;
  if (list_h < 1)
    list_h = 1;

  /* Keep the box fill matching the window interior (border inset). */
  if (m->vbox)
    vk_widget_resize (VK_WIDGET (m->vbox), inner_w, win->rows - 2);

  if (lb)
    vk_widget_resize (VK_WIDGET (lb), inner_w, list_h);
  if (lab)
    vk_widget_resize (VK_WIDGET (lab), inner_w, 1);

  if (win->w)
    keypad (win->w, TRUE);
}

void
redraw_menu (WIN * win)
{
  struct menu_input_s *m = win->data;

  if (!m)
    return;

  fix_menu_geometry (win);
  menu_apply_geometry (win);
  menu_populate_listbox (win);
}

static int
display_menu (WIN * win)
{
  struct menu_input_s *m = win->data;
  vk_listbox_t *lb = (vk_listbox_t *) m->listbox;
  int i, n;
  char *p;
  int custom = 0;

  cbreak ();
  noecho ();
  if (win->w)
    keypad (win->w, TRUE);
  nl ();

  if (m->keys)
    {
      for (i = 0; m->keys[i]; i++)
	{
	  if (win->c == m->keys[i]->c)
	    {
	      (*m->keys[i]->func) (m);
	      m->items = (*m->func) (win);
	      m->search[0] = 0;
	      custom = 1;
	      break;
	    }
	}
    }

  switch (win->c)
    {
    case REFRESH_MENU:
      m->items = (*m->func) (win);
      pushkey = 0;
      custom = 1;
      break;
    case -1:
      pushkey = 0;
      goto done;
    case KEY_HOME:
      if (lb)
	vk_listbox_set_curr (lb, 0);
      m->search[0] = 0;
      break;
    case KEY_END:
      if (lb && m->total > 0)
	vk_listbox_set_curr (lb, m->total - 1);
      m->search[0] = 0;
      break;
    case KEY_UP:
      if (lb)
	vk_listbox_set_prev (lb);
      m->search[0] = 0;
      break;
    case KEY_DOWN:
      if (lb)
	vk_listbox_set_next (lb);
      m->search[0] = 0;
      break;
    case KEY_PPAGE:
      if (lb)
	{
	  int cur = vk_listbox_get_curr (lb);
	  int page = win->rows - 4;

	  if (page < 1)
	    page = 1;
	  cur -= page;
	  if (cur < 0)
	    cur = 0;
	  vk_listbox_set_curr (lb, cur);
	}
      m->search[0] = 0;
      break;
    case KEY_NPAGE:
      if (lb)
	{
	  int cur = vk_listbox_get_curr (lb);
	  int page = win->rows - 4;

	  if (page < 1)
	    page = 1;
	  cur += page;
	  if (m->total > 0 && cur >= m->total)
	    cur = m->total - 1;
	  vk_listbox_set_curr (lb, cur);
	}
      m->search[0] = 0;
      break;
    case KEY_RESIZE:
      return 1;
    default:
      if (!win->c || custom)
	break;

      if (strlen (m->search) + 1 > sizeof (m->search) - 1)
	m->search[0] = 0;

      p = m->search;
      while (*p)
	p++;
      *p++ = (char) win->c;
      *p = 0;
      n = m->selected;

      if (m->items)
	{
	  for (i = 0; m->items[i]; i++)
	    {
	      if (strncasecmp (m->search, m->items[i]->name,
			       strlen (m->search)) == 0)
		{
		  m->selected = i;
		  if (lb)
		    vk_listbox_set_curr (lb, i);
		  break;
		}
	    }
	}

      if (n == m->selected)
	m->search[0] = 0;
      break;
    }

  if (custom)
    {
      /* Item list may have changed (toggle, refresh, reload). */
      fix_menu_geometry (win);
      menu_apply_geometry (win);
      /* Keep current listbox cursor if possible after rebuild. */
      if (lb)
	{
	  int cur = vk_listbox_get_curr (lb);

	  menu_populate_listbox (win);
	  if (cur >= 0 && cur < m->total)
	    {
	      vk_listbox_set_curr (lb, cur);
	      menu_sync_selection (m);
	      vk_listbox_update (lb);
	      menu_composite (win);
	    }
	}
      else
	menu_populate_listbox (win);
    }
  else
    {
      menu_sync_selection (m);
      if (lb)
	vk_listbox_update (lb);
      menu_composite (win);
    }

  if (m->draw_exit_func)
    (*m->draw_exit_func) (m);

  /*
   * Do not call update_all() here: that re-raises the menubar front stack
   * and re-composites the board over this modal.  The menu already painted
   * itself via menu_composite().
   */
  return 1;

done:
  win->data = m->data;

  if (m->items)
    {
      for (i = 0; m->items[i]; i++)
	{
	  if (!m->nofree)
	    free (m->items[i]->name);

	  if (!m->nofree && m->items[i]->value)
	    free (m->items[i]->value);

	  free (m->items[i]);
	}

      free (m->items);
    }

  if (m->keys)
    {
      for (i = 0; m->keys[i]; i++)
	free (m->keys[i]);

      free (m->keys);
    }

  /* listbox/status are owned by the window tree. */
  m->listbox = NULL;
  m->status_label = NULL;
  free (m);
  update_all (gp);
  return 0;
}

static void
menu_resize_func (WIN * w)
{
  struct menu_input_s *m = w->data;

  if (!m->rstatic)
    w->rows = m->total >= LINES - 5 ? LINES - 1 : m->total + 5;
  if (!m->cstatic)
    w->cols = w->cols > COLS - 2 ? COLS - 2 : w->cols;

  if (m->ystatic == -1)
    w->posy = CALCPOSY (w->rows);
  else
    w->posy = m->ystatic;
  if (m->xstatic == -1)
    w->posx = CALCPOSX (w->cols);
  else
    w->posx = m->xstatic;

  m->ystatic = w->posy;
  m->xstatic = w->posx;

  menu_apply_geometry (w);
  menu_populate_listbox (w);
}

WIN *
construct_menu (int rows, int cols, int y, int x, const char *title,
		int name_only, menu_items_fn * func,
		struct menu_key_s ** keys, void *data,
		menu_print_func * pfunc, window_exit_func * efunc,
		window_resize_func * rfunc)
{
  WIN *win;
  struct menu_input_s *m;
  vk_window_t *vkw;
  vk_listbox_t *lb;
  vk_label_t *lab;
  vk_box_t *vbox;
  char titlebuf[256];
  int h, w, posy, posx;
  int inner_w, list_h;

  m = Calloc (1, sizeof (struct menu_input_s));
  m->ystatic = y;
  m->xstatic = x;
  m->print_func = pfunc;
  m->func = func;
  m->keys = keys;
  m->data = data;
  m->name_only = name_only;

  if (rows > 0)
    m->rstatic = 1;
  if (cols > 0)
    m->cstatic = 1;

  h = (rows > 0) ? rows : 8;
  w = (cols > 0) ? cols : 40;
  if (h < 6)
    h = 6;
  if (w < 20)
    w = 20;

  posy = (y >= 0) ? y : CALCPOSY (h);
  posx = (x >= 0) ? x : CALCPOSX (w);

  vkw = vk_window_create (w, h);
  if (title)
    {
      snprintf (titlebuf, sizeof (titlebuf), " %s ", title);
      vk_window_set_title (vkw, titlebuf);
    }
  else
    vk_window_set_title (vkw, " Menu ");

  vk_window_set_border_style (vkw, VK_BORDER_SINGLE);
  vk_window_set_border_colors (vkw,
			       config.color[CONF_IBORDER].fg,
			       config.color[CONF_IBORDER].bg);
  vk_widget_set_colors (VK_WIDGET (vkw),
			config.color[CONF_MENU].fg,
			config.color[CONF_MENU].bg);

  inner_w = w - 2;
  list_h = h - 4;
  if (inner_w < 1)
    inner_w = 1;
  if (list_h < 1)
    list_h = 1;

  lb = vk_listbox_create (inner_w, list_h);
  vk_listbox_set_wrap (lb, true);
  vk_listbox_set_highlight (lb,
			    config.color[CONF_MENUS].fg,
			    config.color[CONF_MENUS].bg);
  vk_listbox_set_highlight_attrs (lb, config.color[CONF_MENUS].attrs);
  vk_widget_set_colors (VK_WIDGET (lb),
			config.color[CONF_MENU].fg,
			config.color[CONF_MENU].bg);
  vk_widget_set_expand (VK_WIDGET (lb));
  m->listbox = lb;

  lab = vk_label_create (inner_w);
  vk_widget_set_colors (VK_WIDGET (lab),
			config.color[CONF_IPROMPT].fg,
			config.color[CONF_IPROMPT].bg);
  m->status_label = lab;

  vbox = vk_box_create (inner_w, h - 2, VK_BOX_VERTICAL, 2);
  vk_widget_set_colors (VK_WIDGET (vbox),
			config.color[CONF_MENU].fg,
			config.color[CONF_MENU].bg);
  /* Expand with the frame so geometry changes keep a full interior. */
  vk_widget_set_expand (VK_WIDGET (vbox));
  vk_box_set_widget (vbox, 0, VK_WIDGET (lb));
  vk_box_set_widget (vbox, 1, VK_WIDGET (lab));
  vk_window_set_child (vkw, VK_WIDGET (vbox));
  m->vbox = vbox;

  cboard_ui_widget_attach ((cboard_widget_t *) vkw, posy, posx);
  cboard_ui_widget_raise ((cboard_widget_t *) vkw);

  win = window_adopt (title, (void *) vkw, WIN_VK_WINDOW, h, w, posy, posx,
		      display_menu, m, efunc,
		      rfunc ? rfunc : menu_resize_func);
  /* window_adopt overwrote m via data pointer — m is still our struct. */
  win->data = m;

  if (win->w)
    keypad (win->w, TRUE);

  cbreak ();
  noecho ();
  nl ();

  m->items = (*m->func) (win);
  fix_menu_geometry (win);
  menu_apply_geometry (win);
  menu_populate_listbox (win);

  return win;
}

void
add_menu_key (struct menu_key_s ***dst, wint_t c, menu_key func)
{
  int n = 0;
  struct menu_key_s **keys = *dst;

  if (keys)
    for (; keys[n]; n++);

  keys = Realloc (keys, (n + 2) * sizeof (struct menu_key_s *));
  keys[n] = Malloc (sizeof (struct menu_key_s));
  keys[n]->c = c;
  keys[n++]->func = func;
  keys[n] = NULL;
  *dst = keys;
}

void
add_menu_help_key (struct menu_key_s ***dst, menu_key func)
{
  int i;

  if (!global_keys)
    return;

  for (i = 0; global_keys[i]; i++)
    {
      if (global_keys[i]->f == do_global_help)
	add_menu_key (dst, global_keys[i]->c, func);
    }
}
