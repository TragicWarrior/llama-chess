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
#include <stdarg.h>
#include <string.h>
#include <wctype.h>

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include <vdk.h>

#include "common.h"
#include "conf.h"
#include "colors.h"
#include "misc.h"
#include "window.h"
#include "message.h"
#include "ui_screen.h"

struct message_s
{
  int w;
  int h;
  char *title;
  char *prompt;
  char *extra;
  char *body;			/* UTF-8 body for textbox */
  vk_textbox_t *tb;
  int center;
  wint_t c;
  message_func *func;
  void *arg;
};

static void
wordwrap_lines (wchar_t *** olines, int *nlines, int *width)
{
  int i;
  wchar_t *buf = NULL, **lines = *olines;
  int total = *nlines, w = *width;

  for (i = 0; i < total; i++)
    {
      size_t len = wcslen (lines[i]);

      if (buf)
	{
	  size_t blen = wcslen (buf);

	  lines[i] = Realloc (lines[i], len + blen + 1 * sizeof (wchar_t *));
	  wmemmove (&lines[i][blen], lines[i], len);
	  wmemcpy (lines[i], buf, blen);
	  lines[i][blen + len] = 0;
	  free (buf);
	  buf = NULL;
	  len = wcslen (lines[i]);
	}

      if (len > w)
	w = len;

      if (len-- > MSG_WIDTH)
	{
	  wchar_t *p;

	  for (p = lines[i] + len; *p && len > 0; p--, len--)
	    {
	      if (iswspace (*p) && len <= MSG_WIDTH)
		{
		  *p++ = 0;
		  buf = wcsdup (p);
		  break;
		}
	    }

	  if (!buf)
	    {
	      wchar_t *t, c, *bp;
	      size_t l;

	      t = lines[i] + MSG_WIDTH;
	      c = *t;
	      *t++ = 0;
	      l = wcslen (t) + 2;
	      buf = Malloc (len * sizeof (wchar_t));
	      bp = buf;
	      *bp++ = c;
	      wmemcpy (bp, t, l - 1);
	    }
	}
    }

  *width = w;

  if (buf)
    {
      lines = Realloc (lines, (total + 2) * sizeof (wchar_t *));
      lines[total++] = buf;
      lines[total] = NULL;
      *nlines = total;
      *olines = lines;
      wordwrap_lines (olines, nlines, width);
    }
}

static char *
lines_to_body (wchar_t ** lines, const char *prompt, const char *extra)
{
  size_t len = 0;
  int i;
  char *body, *p;

  for (i = 0; lines && lines[i]; i++)
    {
      char *s = wchar_to_str (lines[i]);

      len += strlen (s) + 1;
      free (s);
    }

  if (extra)
    len += strlen (extra) + 1;
  if (prompt)
    len += strlen (prompt) + 1;

  body = Calloc (1, len + 2);
  p = body;

  for (i = 0; lines && lines[i]; i++)
    {
      char *s = wchar_to_str (lines[i]);

      if (i)
	*p++ = '\n';
      strcpy (p, s);
      p += strlen (s);
      free (s);
    }

  if (extra)
    {
      if (body[0])
	*p++ = '\n';
      strcpy (p, extra);
      p += strlen (extra);
    }

  if (prompt)
    {
      if (body[0])
	*p++ = '\n';
      strcpy (p, prompt);
    }

  return body;
}

static void
build_message_lines (const char *title, const char *prompt,
		     int force_trim, const char *extra, int *h,
		     int *w, wchar_t *** str, const char *fmt, va_list ap)
{
  int n;
  char *line;
  wchar_t **lines = NULL;
  int width = 0, height = 0, len;
  int total = 0;
  wchar_t *wc, *wc_tmp, *wc_line, wc_delim[] = { '\n', 0 };

  (void) force_trim;

#ifdef HAVE_VASPRINTF
  if (vasprintf (&line, fmt, ap) < 0)
    line = NULL;
#else
  line = Malloc (LINE_MAX);
  vsnprintf (line, LINE_MAX, fmt, ap);
#endif

  if (!line)
    {
      *h = 0;
      *w = 0;
      *str = NULL;
      return;
    }

  wc = str_to_wchar (line);
  free (line);
  total = n = 0;
  for (wc_line = wcstok (wc, wc_delim, &wc_tmp); wc_line;
       wc_line = wcstok (NULL, wc_delim, &wc_tmp))
    {
      lines = Realloc (lines, (total + 2) * sizeof (wchar_t *));
      lines[total++] = wcsdup (wc_line);
      lines[total] = NULL;
    }

  free (wc);
  wordwrap_lines (&lines, &total, &width);

  if (width > MSG_WIDTH)
    width = MSG_WIDTH;

  if (prompt)
    {
      wc = str_to_wchar (prompt);
      len = wcslen (wc);
      width = len > width ? len : width;
      free (wc);
    }

  if (extra)
    {
      wc = str_to_wchar (extra);
      len = wcslen (wc);
      width = len > width ? len : width;
      free (wc);
    }

  if (title)
    {
      wc = str_to_wchar (title);
      len = wcslen (wc);
      width = len > width ? len : width;
      free (wc);
    }

  height = total;
  if (extra)
    height++;
  if (prompt)
    height++;
  if (title)
    height++;

  height += 4;			/* padding + borders + title bar */
  width += 4;
  *h = height;
  *w = width;
  *str = lines;
}

static void
message_free (WIN * w)
{
  struct message_s *m = w->data;
  void *p;

  free (m->body);
  free (m->prompt);
  free (m->extra);
  free (m->title);
  /* textbox is owned by the window child and destroyed with the window. */
  m->tb = NULL;
  p = m->arg;
  free (m);
  w->data = p;
}

static void
message_paint (struct message_s *m)
{
  if (m->tb)
    {
      vk_textbox_update (m->tb);
      vk_widget_draw (VK_WIDGET (m->tb));
    }
  cboard_ui_refresh ();
}

static int
display_message (WIN * win)
{
  struct message_s *m = win->data;

  if (m->func && win->c == m->c)
    {
      (*m->func) (m->arg);
      return 1;
    }

  if (win->c != 0)
    {
      if (win->c == KEY_DOWN || win->c == KEY_NPAGE)
	{
	  if (m->tb)
	    {
	      if (win->c == KEY_NPAGE)
		vk_textbox_scroll_pgdn (m->tb);
	      else
		vk_textbox_scroll_down (m->tb);
	      message_paint (m);
	    }
	  win->c = 0;
	  return 1;
	}

      if (win->c == KEY_UP || win->c == KEY_PPAGE)
	{
	  if (m->tb)
	    {
	      if (win->c == KEY_PPAGE)
		vk_textbox_scroll_pgup (m->tb);
	      else
		vk_textbox_scroll_up (m->tb);
	      message_paint (m);
	    }
	  win->c = 0;
	  return 1;
	}

      if (win->c == KEY_HOME && m->tb)
	{
	  vk_textbox_scroll_home (m->tb);
	  message_paint (m);
	  win->c = 0;
	  return 1;
	}

      if (win->c == KEY_END && m->tb)
	{
	  vk_textbox_scroll_end (m->tb);
	  message_paint (m);
	  win->c = 0;
	  return 1;
	}

      if (win->c == KEY_RESIZE)
	return 1;

      message_free (win);
      return 0;
    }

  return 1;
}

static void
message_resize_func (WIN * w)
{
  struct message_s *m = w->data;
  int inner_w, inner_h;

  m->h = w->rows = m->h >= LINES - 2 ? LINES - 2 : m->h;
  m->w = w->cols = m->w > COLS - 2 ? COLS - 2 : m->w;
  w->posy = CALCPOSY (w->rows);
  w->posx = CALCPOSX (w->cols);

  if (w->vk)
    {
      w->w = cboard_ui_widget_resize (w->vk, w->rows, w->cols);
      cboard_ui_widget_move (w->vk, w->posy, w->posx);
    }

  if (m->tb)
    {
      inner_w = w->cols - 2;
      inner_h = w->rows - 3;
      if (inner_w < 1)
	inner_w = 1;
      if (inner_h < 1)
	inner_h = 1;
      vk_widget_resize (VK_WIDGET (m->tb), inner_w, inner_h);
      vk_textbox_update (m->tb);
      vk_window_update (VK_WINDOW (w->vk));
    }

  message_paint (m);
}

/*
 * The force_trim parameter will trim whitespace reguardless if there is more
 * than one line or not (help text vs. tag viewing).
 */
WIN *
construct_message (const char *title, const char *prompt, int center,
		   int force_trim, const char *extra_help,
		   message_func * func, void *arg, window_exit_func * efunc,
		   wint_t ckey, int freedata, window_resize_func * rfunc,
		   const char *fmt, ...)
{
  wchar_t **lines = NULL;
  va_list ap;
  struct message_s *m = NULL;
  WIN *win = NULL;
  vk_window_t *vkw;
  vk_textbox_t *tb;
  int h, w, i;
  int y, x;
  int inner_w, inner_h;
  char titlebuf[256];

  va_start (ap, fmt);
  build_message_lines (title, prompt, force_trim, extra_help, &h, &w, &lines,
		       fmt, ap);
  va_end (ap);

  m = Calloc (1, sizeof (struct message_s));
  m->w = w > COLS - 2 ? COLS - 2 : w;
  m->h = h > LINES - 2 ? LINES - 2 : h;
  m->center = center;
  m->c = ckey;
  m->func = func;
  m->arg = arg;
  m->body = lines_to_body (lines, prompt, extra_help);

  if (prompt)
    m->prompt = strdup (prompt);
  if (extra_help)
    m->extra = strdup (extra_help);
  if (title)
    m->title = strdup (title);

  for (i = 0; lines && lines[i]; i++)
    free (lines[i]);
  free (lines);

  if (m->w < 20)
    m->w = 20;
  if (m->h < 6)
    m->h = 6;

  y = CALCPOSY (m->h);
  x = CALCPOSX (m->w);

  vkw = vk_window_create (m->w, m->h);
  if (title)
    {
      snprintf (titlebuf, sizeof (titlebuf), " %s ", title);
      vk_window_set_title (vkw, titlebuf);
    }
  else
    vk_window_set_title (vkw, " Message ");

  /* Match classic cboard message chrome: black body, cyan border. */
  vk_window_set_border_style (vkw, VK_BORDER_SINGLE);
  vk_window_set_border_colors (vkw,
			       config.color[CONF_MBORDER].fg,
			       config.color[CONF_MBORDER].bg);
  vk_widget_set_colors (VK_WIDGET (vkw),
			config.color[CONF_MWINDOW].fg,
			config.color[CONF_MWINDOW].bg);

  inner_w = m->w - 2;
  inner_h = m->h - 3;
  if (inner_w < 1)
    inner_w = 1;
  if (inner_h < 1)
    inner_h = 1;

  tb = vk_textbox_create (inner_w, inner_h);
  vk_textbox_set_word_wrap (tb, true);
  vk_textbox_set_text (tb, m->body ? m->body : "");
  vk_widget_set_colors (VK_WIDGET (tb),
			config.color[CONF_MWINDOW].fg,
			config.color[CONF_MWINDOW].bg);
  vk_widget_set_expand (VK_WIDGET (tb));
  vk_window_set_child (vkw, VK_WIDGET (tb));
  m->tb = tb;

  vk_textbox_update (tb);
  vk_window_update (vkw);

  cboard_ui_widget_attach ((cboard_widget_t *) vkw, y, x);
  cboard_ui_widget_raise ((cboard_widget_t *) vkw);

  win = window_adopt (title, (void *) vkw, WIN_VK_WINDOW, m->h, m->w, y, x,
		      display_message, m, efunc,
		      rfunc ? rfunc : message_resize_func);
  win->freedata = freedata;
  if (win->w)
    keypad (win->w, TRUE);

  message_paint (m);
  return win;
}

/* ---- Yes/No confirm (vk_popup, menu colors, keyboard + mouse) ---- */

struct confirm_s
{
  vk_popup_t *popup;
  vk_label_t *label;
  int focus;			/* 0 = Yes, 1 = No */
};

static wchar_t
confirm_yes_char (void)
{
  wchar_t *yw = str_to_wchar (_("y"));
  wchar_t c = (yw && yw[0]) ? yw[0] : L'y';

  free (yw);
  return c;
}

static void
confirm_style (struct confirm_s *c)
{
  short fg = config.color[CONF_MENU].fg;
  short bg = config.color[CONF_MENU].bg;
  attr_t attrs = config.color[CONF_MENU].attrs | A_BOLD;
  short yfg, nfg;
  vk_button_t *by, *bn;

  if (!c || !c->popup)
    return;

  yfg = (c->focus == 0) ? COLOR_YELLOW : fg;
  nfg = (c->focus == 1) ? COLOR_YELLOW : fg;

  vk_popup_set_colors (c->popup, fg, bg);
  vk_popup_set_button_colors (c->popup, fg, bg);
  vk_popup_set_button_attrs (c->popup, attrs);
  vk_window_set_border_colors (VK_WINDOW (c->popup), fg, bg);
  vk_window_set_border_attrs (VK_WINDOW (c->popup), attrs);
  vk_widget_set_colors (VK_WIDGET (c->popup), fg, bg);
  vk_widget_set_attrs (VK_WIDGET (c->popup), attrs);

  if (c->label)
    {
      vk_widget_set_colors (VK_WIDGET (c->label), fg, bg);
      vk_widget_set_attrs (VK_WIDGET (c->label), attrs);
      vk_label_update (c->label);
    }

  by = vk_popup_get_button (c->popup, 0);
  bn = vk_popup_get_button (c->popup, 1);
  if (by)
    {
      vk_widget_set_colors (VK_WIDGET (by), yfg, bg);
      vk_widget_set_attrs (VK_WIDGET (by), attrs);
      vk_button_update (by);
    }
  if (bn)
    {
      vk_widget_set_colors (VK_WIDGET (bn), nfg, bg);
      vk_widget_set_attrs (VK_WIDGET (bn), attrs);
      vk_button_update (bn);
    }

  {
    vk_box_t *bar = vk_popup_get_button_bar (c->popup);

    if (bar)
      {
	vk_box_set_subfocus (bar, c->focus);
	vk_box_update (bar);
      }
  }

  vk_popup_update (c->popup);
}

static void
confirm_free (WIN * w)
{
  struct confirm_s *c = w->data;

  if (c)
    {
      /* popup destroyed with WIN_VK_POPUP */
      c->popup = NULL;
      c->label = NULL;
      free (c);
    }
  w->data = NULL;
}

static int
display_confirm (WIN * win)
{
  struct confirm_s *c = win->data;
  wint_t k;

  if (!c || !c->popup)
    return 0;

  k = win->c;

  if (k == KEY_ESCAPE || k == 'n' || k == 'N')
    {
      win->c = 0;
      confirm_free (win);
      return 0;
    }

  if (k == 'y' || k == 'Y')
    {
      win->c = confirm_yes_char ();
      confirm_free (win);
      return 0;
    }

  if (k == KEY_LEFT || k == KEY_BTAB)
    {
      c->focus = 0;
      confirm_style (c);
      cboard_ui_refresh ();
      return 1;
    }

  if (k == KEY_RIGHT || k == '\t')
    {
      c->focus = 1;
      confirm_style (c);
      cboard_ui_refresh ();
      return 1;
    }

  if (k == '\n' || k == '\r' || k == KEY_ENTER)
    {
      if (c->focus == 0)
	win->c = confirm_yes_char ();
      else
	win->c = 0;
      confirm_free (win);
      return 0;
    }

  return 1;
}

WIN *
construct_confirm (const char *title, const char *text,
		   window_exit_func * efunc)
{
  WIN *win;
  vk_popup_t *popup;
  vk_label_t *lab;
  struct confirm_s *c;
  int w, h, y, x;
  char titlebuf[128];
  const char *body = text ? text : "";

  w = (int) strlen (body) + 6;
  if (w < 28)
    w = 28;
  if (w > COLS - 4)
    w = COLS - 4;
  h = 8;
  if (h > LINES - 2)
    h = LINES - 2;

  y = CALCPOSY (h);
  x = CALCPOSX (w);

  popup = vk_popup_create (w, h, VK_BORDER_SINGLE, _("Yes"), _("No"), NULL);
  if (!popup)
    return NULL;

  if (title && title[0])
    {
      snprintf (titlebuf, sizeof (titlebuf), " %s ", title);
      vk_popup_set_title (popup, titlebuf);
    }
  else
    vk_popup_set_title (popup, " Confirm ");

  lab = vk_label_create (w - 4);
  vk_label_set_text (lab, (char *) body);
  vk_popup_set_client (popup, VK_WIDGET (lab));

  c = Calloc (1, sizeof (struct confirm_s));
  c->popup = popup;
  c->label = lab;
  c->focus = 0;
  confirm_style (c);

  cboard_ui_widget_attach ((cboard_widget_t *) popup, y, x);
  cboard_ui_widget_raise ((cboard_widget_t *) popup);
  vk_popup_update (popup);

  win = window_adopt (title, (void *) popup, WIN_VK_POPUP, h, w, y, x,
		      display_confirm, c, efunc, NULL);
  cboard_ui_refresh ();
  return win;
}

/*
 * Close confirm after mouse: set win->c for efunc, free state, inject a
 * key so game_loop runs display_confirm (data NULL → return 0 → destroy).
 */
static void
confirm_mouse_close (WIN * win, wint_t result_c)
{
  win->c = result_c;
  confirm_free (win);
  pushkey = KEY_ESCAPE;
}

int
confirm_dialog_mouse (WIN * win, int x, int y, mmask_t bstate)
{
  struct confirm_s *c;
  int px, py, pw, ph;
  int lx, ly;
  int bar_top;
  int mid;

  if (!win || !win->data || win->vk_kind != WIN_VK_POPUP)
    return 0;

  c = win->data;
  if (!c->popup)
    return 0;

  vk_widget_get_position (VK_WIDGET (c->popup), &px, &py);
  vk_widget_get_metrics (VK_WIDGET (c->popup), &pw, &ph);
  if (x < px || y < py || x >= px + pw || y >= py + ph)
    return 0;

  if (!(bstate & (BUTTON1_PRESSED | BUTTON1_CLICKED | BUTTON1_DOUBLE_CLICKED)))
    return 1;

  lx = x - px;
  ly = y - py;

  /* Button bar occupies the lower interior of the popup (border inset). */
  bar_top = ph - 5;
  if (bar_top < 2)
    bar_top = ph / 2;

  if (ly < bar_top)
    {
      /* Click on message area — focus Yes by default. */
      c->focus = 0;
      confirm_style (c);
      cboard_ui_refresh ();
      return 1;
    }

  mid = pw / 2;
  if (lx < mid)
    {
      /* Yes */
      confirm_mouse_close (win, confirm_yes_char ());
      return 1;
    }

  /* No */
  confirm_mouse_close (win, 0);
  return 1;
}
