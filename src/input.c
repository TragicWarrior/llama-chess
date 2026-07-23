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
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <wctype.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <vdk.h>

#include "common.h"
#include "conf.h"
#include "colors.h"
#include "window.h"
#include "misc.h"
#include "message.h"
#include "input.h"
#include "keys.h"
#include "rcfile.h"
#include "ui_screen.h"

static struct input_history_s
{
  char *str;
  struct input_history_s *next;
  struct input_history_s *prev;
  struct input_history_s *head;
} *input_history[INPUT_HIST_MAX];

/* Extended state for VDK single-line input. */
struct vdk_input_s
{
  struct input_s base;		/* must be first — win->data cast to input_s */
  vk_input_t *field;
  vk_box_t *vbox;		/* field + help labels under the window frame */
  int (*char_ok) (int c);
};

/*
 * Paint field/labels into the box, then the box into the window.
 * Order matters: update leaf widgets first, then composite upward.
 */
/* Layout/update VDK tree only — no screen composite (for resize cascade). */
static void
input_layout (WIN * win)
{
  struct vdk_input_s *vin;

  if (!win || !win->vk)
    return;

  vin = win->data;
  if (vin)
    {
      if (vin->field)
	vk_input_update (vin->field);
      if (vin->vbox)
	vk_box_update (vin->vbox);
    }
  vk_window_update (VK_WINDOW (win->vk));
}

static void
input_composite (WIN * win)
{
  input_layout (win);
  cboard_ui_refresh ();
}

static void
add_input_history (int which, const char *str)
{
  struct input_history_s *new;
  struct input_history_s *p = NULL;

  if (!str || !*str || which < 0)
    return;

  if (input_history[which])
    for (p = input_history[which]->head; p->next; p = p->next);

  new = Calloc (1, sizeof (struct input_history_s));
  new->str = strdup (str);
  new->prev = p;

  if (p)
    {
      p->next = new;
      new->head = p->head;
    }
  else
    new->head = new;

  input_history[which] = p ? p->next : new;
}

static int
char_ok_any (int c)
{
  (void) c;
  return 1;
}

static int
char_ok_tag_name (int c)
{
  return isalnum (c) || c == '_';
}

static int
char_ok_pgn_date (int c)
{
  return isdigit (c) || c == '.' || c == '?';
}

static int
char_ok_pgn_round (int c)
{
  return isdigit (c) || c == '.' || c == '-' || c == '?';
}

static int
char_ok_pgn_result (int c)
{
  return c == '1' || c == '0' || c == '2' || c == '*' || c == '/' || c == '-';
}

static int
char_ok_alnum (int c)
{
  return isalnum (c);
}

static int
char_ok_alpha (int c)
{
  return isalpha (c);
}

static int
char_ok_integer (int c)
{
  return isdigit (c) || c == '-' || c == '+';
}

static void
input_finish (WIN * win, struct vdk_input_s *vin, const char *tmp)
{
  struct input_s *in = &vin->base;
  struct input_data_s *data = in->data;
  int i;

  if (tmp)
    {
      char *t = strdup (tmp);

      t = trim (t);
      if (t[0])
	{
	  strncpy (in->buf, t, sizeof (in->buf) - 1);
	  in->buf[sizeof (in->buf) - 1] = 0;
	}
      else
	in->buf[0] = 0;
      free (t);
    }
  else
    in->buf[0] = 0;

  if (in->extra)
    {
      for (i = 0; in->extra[i]; i++)
	free (in->extra[i]);
      free (in->extra);
    }

  data->str = (in->buf[0]) ? strdup (in->buf) : NULL;

  if (in->lines == 1)
    add_input_history (in->hist, in->buf);

  win->data = data;
  /* field is owned by the window child tree. */
  vin->field = NULL;
  free (vin);
  curs_set (0);
}

static int
get_input_vdk (WIN * win)
{
  struct vdk_input_s *vin = win->data;
  struct input_s *in = &vin->base;
  const char *text;
  char *prompt = _("Type %ls for help");
  char buf[255];
  char str[MB_CUR_MAX + 1];
  int i, n;

  snprintf (buf, sizeof (buf), prompt,
	    key_lookup (global_keys, do_global_help));
  curs_set (1);

  if (in->func && in->c && win->c == in->c)
    {
      (*in->func) (in);
      input_composite (win);
      return 1;
    }

  if (key_matches (global_keys, do_global_help, win->c))
    {
      message (_("Line Editing Keys"), ANY_KEY_STR,
	       "%s",
	       _("         LEFT/RIGHT - position cursor\n"
		 "         UP/CTRL-P - previous input history\n"
		 "       DOWN/CTRL-N - next input history\n"
		 "       HOME/CTRL-A - beginning of line\n"
		 "        END/CTRL-E - end of line\n"
		 "            CTRL-U - clear entire input field\n"
		 "         BACKSPACE - delete previous character\n"
		 "            ESCAPE - quit without changes\n"
		 "             ENTER - quit with changes"));
      return 1;
    }

  switch (win->c)
    {
    case KEY_HOME:
    case CTRL_KEY ('A'):
      vk_input_home (vin->field);
      break;
    case KEY_END:
    case CTRL_KEY ('E'):
      vk_input_end (vin->field);
      break;
    case CTRL_KEY ('U'):
      vk_input_clear (vin->field);
      break;
    case KEY_LEFT:
      vk_input_move_cursor (vin->field, -1);
      break;
    case KEY_RIGHT:
      vk_input_move_cursor (vin->field, 1);
      break;
    case CTRL_KEY ('P'):
    case KEY_UP:
      if (in->hist >= 0 && input_history[in->hist])
	{
	  vk_input_set_text (vin->field, input_history[in->hist]->str);
	  vk_input_end (vin->field);
	  if (!input_history[in->hist]->prev)
	    input_history[in->hist] = input_history[in->hist]->head;
	  else
	    input_history[in->hist] = input_history[in->hist]->prev;
	}
      break;
    case CTRL_KEY ('N'):
    case KEY_DOWN:
      if (in->hist >= 0 && input_history[in->hist])
	{
	  if (!input_history[in->hist]->next)
	    {
	      vk_input_clear (vin->field);
	      break;
	    }
	  input_history[in->hist] = input_history[in->hist]->next;
	  vk_input_set_text (vin->field, input_history[in->hist]->str);
	  vk_input_end (vin->field);
	}
      break;
    case '\010':
    case KEY_BACKSPACE:
    case 127:
      vk_input_backspace (vin->field);
      break;
    case '\n':
    case KEY_ENTER:
      text = vk_input_get_text (vin->field);
      input_finish (win, vin, text);
      return 0;
    case KEY_ESCAPE:
      if (in->reset)
	input_finish (win, vin, NULL);
      else
	input_finish (win, vin, in->buf[0] ? in->buf : NULL);
      return 0;
    case KEY_RESIZE:
      return 1;
    default:
      if (!win->c)
	break;
      n = wctomb (str, win->c);
      if (n <= 0)
	break;
      for (i = 0; i < n; i++)
	{
	  unsigned char ch = (unsigned char) str[i];

	  if (vin->char_ok && !vin->char_ok (ch))
	    continue;
	  vk_input_insert_char (vin->field, ch);
	}
      break;
    }

  input_composite (win);
  return 1;
}

static void
input_resize_func (WIN * w)
{
  struct vdk_input_s *vin = w->data;

  w->posy = CALCPOSY (w->rows);
  w->posx = CALCPOSX (w->cols);
  if (w->vk)
    {
      w->w = cboard_ui_widget_resize (w->vk, w->rows, w->cols);
      cboard_ui_widget_move (w->vk, w->posy, w->posx);
      if (vin && vin->vbox)
	vk_widget_resize (VK_WIDGET (vin->vbox), w->cols - 2, w->rows - 2);
      /* Geometry only; do_window_resize ends with one composite. */
      input_layout (w);
    }
}

void
input_set_buf (struct input_s *in, const char *text)
{
  struct vdk_input_s *vin;

  if (!in)
    return;

  if (text)
    {
      strncpy (in->buf, text, sizeof (in->buf) - 1);
      in->buf[sizeof (in->buf) - 1] = 0;
    }
  else
    in->buf[0] = 0;

  /* vin is the container of base when constructed via construct_input. */
  vin = (struct vdk_input_s *) in;
  if (vin->field)
    {
      vk_input_set_text (vin->field, in->buf);
      vk_input_update (vin->field);
      if (vin->vbox)
	vk_box_update (vin->vbox);
    }
}

void
input_refresh_win (WIN * win)
{
  if (!win || !win->data)
    return;

  /* win->data is vdk_input_s while the input dialog is open. */
  input_composite (win);
}

/*
 * Inputs use VDK vk_input (single-line field with long max length).
 */
WIN *
construct_input (const char *title, const char *init, int lines, int reset,
		 const char *extra_help, input_func * func, void *arg,
		 wint_t key, struct input_data_s * id, int history,
		 window_resize_func rfunc, int type, ...)
{
  struct vdk_input_s *vin;
  struct input_s *in;
  WIN *win;
  vk_window_t *vkw;
  vk_input_t *field;
  vk_box_t *vbox = NULL;
  int eh = 0;
  int h, w;
  int y, x;
  char titlebuf[256];
  va_list ap;
  int (*char_ok) (int) = char_ok_any;

  (void) lines;			/* multi-line uses same single-line field for now */

  va_start (ap, type);
  switch (type)
    {
    case FIELD_TYPE_PGN_ROUND:
      char_ok = char_ok_pgn_round;
      break;
    case FIELD_TYPE_PGN_RESULT:
      char_ok = char_ok_pgn_result;
      break;
    case FIELD_TYPE_PGN_DATE:
      char_ok = char_ok_pgn_date;
      break;
    case FIELD_TYPE_PGN_TAG_NAME:
      char_ok = char_ok_tag_name;
      break;
    case FIELD_TYPE_ALNUM:
      (void) va_arg (ap, int);
      char_ok = char_ok_alnum;
      break;
    case FIELD_TYPE_ALPHA:
      (void) va_arg (ap, int);
      char_ok = char_ok_alpha;
      break;
    case FIELD_TYPE_INTEGER:
      (void) va_arg (ap, int);
      (void) va_arg (ap, long);
      (void) va_arg (ap, long);
      char_ok = char_ok_integer;
      break;
    case FIELD_TYPE_NUMERIC:
      (void) va_arg (ap, int);
      (void) va_arg (ap, double);
      (void) va_arg (ap, double);
      char_ok = char_ok_integer;
      break;
    case FIELD_TYPE_ENUM:
      (void) va_arg (ap, char **);
      (void) va_arg (ap, int);
      (void) va_arg (ap, int);
      break;
    case FIELD_TYPE_REGEXP:
      (void) va_arg (ap, char *);
      break;
#ifdef HAVE_TYPE_IPV4
    case FIELD_TYPE_IPV4:
      break;
#endif
    default:
      break;
    }
  va_end (ap);

  vin = Calloc (1, sizeof (struct vdk_input_s));
  in = &vin->base;
  vin->char_ok = char_ok;
  in->w = INPUT_WIDTH;
  in->func = func;
  in->arg = arg;
  in->c = key;
  in->lines = 1;
  in->hist = history;
  in->data = id;
  in->reset = reset;

  if (extra_help)
    {
      char *tmp = strdup (extra_help);

      in->extra = split_str (tmp, "\n", &eh, &in->w, 0);
      free (tmp);
    }

  if (init)
    {
      strncpy (in->buf, init, sizeof (in->buf) - 1);
      in->buf[sizeof (in->buf) - 1] = 0;
    }

  /* title + border + input(3) + help lines */
  h = 2 + 3 + (eh ? eh : 1) + 1;
  w = in->w;
  if (w < 24)
    w = 24;
  if (w > COLS - 2)
    w = COLS - 2;
  in->h = h;
  in->w = w;

  y = CALCPOSY (h);
  x = CALCPOSX (w);

  vkw = vk_window_create (w, h);
  if (title)
    {
      snprintf (titlebuf, sizeof (titlebuf), " %s ", title);
      vk_window_set_title (vkw, titlebuf);
    }
  else
    vk_window_set_title (vkw, " Input ");

  /*
   * Match modal menus: bright white on cyan body; border is bright white
   * on cyan (CONF_MENU).
   */
  vk_window_set_border_style (vkw, VK_BORDER_SINGLE);
  vk_window_set_border_colors (vkw,
			       config.color[CONF_MENU].fg,
			       config.color[CONF_MENU].bg);
  vk_window_set_border_attrs (vkw, config.color[CONF_MENU].attrs);
  vk_widget_set_colors (VK_WIDGET (vkw),
			config.color[CONF_MENU].fg,
			config.color[CONF_MENU].bg);
  vk_widget_set_attrs (VK_WIDGET (vkw), config.color[CONF_MENU].attrs);

  field = vk_input_create (w - 2);
  vk_input_set_border_style (field, VK_BORDER_SINGLE);
  vk_input_set_max_length (field, (int) sizeof (in->buf) - 1);
  vk_input_show_cursor (field, true);
  if (in->buf[0])
    vk_input_set_text (field, in->buf);
  vk_widget_set_colors (VK_WIDGET (field),
			config.color[CONF_MENU].fg,
			config.color[CONF_MENU].bg);
  vk_widget_set_attrs (VK_WIDGET (field), config.color[CONF_MENU].attrs);
  vk_widget_set_relief_colors (VK_WIDGET (field),
			       config.color[CONF_MENU].fg,
			       config.color[CONF_MENU].bg);
  vin->field = field;

  if (eh > 0)
    {
      int slots = 1 + eh;
      int si;

      vbox = vk_box_create (w - 2, h - 2, VK_BOX_VERTICAL, slots);
      vk_widget_set_colors (VK_WIDGET (vbox),
			    config.color[CONF_MENU].fg,
			    config.color[CONF_MENU].bg);
      vk_widget_set_attrs (VK_WIDGET (vbox), config.color[CONF_MENU].attrs);
      vk_widget_set_expand (VK_WIDGET (vbox));
      vk_box_set_widget (vbox, 0, VK_WIDGET (field));
      for (si = 0; si < eh; si++)
	{
	  vk_label_t *lab = vk_label_create (w - 2);

	  vk_label_set_text (lab, in->extra[si]);
	  vk_widget_set_colors (VK_WIDGET (lab),
				config.color[CONF_MENU].fg,
				config.color[CONF_MENU].bg);
	  vk_widget_set_attrs (VK_WIDGET (lab), config.color[CONF_MENU].attrs);
	  vk_label_update (lab);
	  vk_box_set_widget (vbox, si + 1, VK_WIDGET (lab));
	}
      vk_window_set_child (vkw, VK_WIDGET (vbox));
    }
  else
    {
      vk_label_t *lab = vk_label_create (w - 2);
      char helpbuf[255];

      vbox = vk_box_create (w - 2, h - 2, VK_BOX_VERTICAL, 2);
      vk_widget_set_colors (VK_WIDGET (vbox),
			    config.color[CONF_MENU].fg,
			    config.color[CONF_MENU].bg);
      vk_widget_set_attrs (VK_WIDGET (vbox), config.color[CONF_MENU].attrs);
      vk_widget_set_expand (VK_WIDGET (vbox));
      vk_box_set_widget (vbox, 0, VK_WIDGET (field));
      snprintf (helpbuf, sizeof (helpbuf), _("Type %ls for help"),
		key_lookup (global_keys, do_global_help));
      vk_label_set_text (lab, helpbuf);
      vk_widget_set_colors (VK_WIDGET (lab),
			    config.color[CONF_MENU].fg,
			    config.color[CONF_MENU].bg);
      vk_widget_set_attrs (VK_WIDGET (lab), config.color[CONF_MENU].attrs);
      vk_label_update (lab);
      vk_box_set_widget (vbox, 1, VK_WIDGET (lab));
      vk_window_set_child (vkw, VK_WIDGET (vbox));
    }
  vin->vbox = vbox;

  cboard_ui_widget_attach ((cboard_widget_t *) vkw, y, x);
  cboard_ui_widget_raise ((cboard_widget_t *) vkw);

  win = window_adopt (title, (void *) vkw, WIN_VK_WINDOW, h, w, y, x,
		      get_input_vdk, vin, id->efunc,
		      rfunc ? rfunc : input_resize_func);
  win->app_kind = WIN_APP_INPUT;
  if (win->w)
    keypad (win->w, TRUE);
  curs_set (1);
  /* Leaves first, then box, then window — see input_composite(). */
  input_composite (win);
  return win;
}
