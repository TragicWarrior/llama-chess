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
