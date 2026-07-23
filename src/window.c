/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2024 Ben Kibbey <bjk@luxsci.net>
    Copyright (C) 2026 cboard VDK port

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "misc.h"
#include "window.h"
#include "ui_screen.h"

/* Modal stack: bottom → top. Top receives keys from game_loop. */
static WIN **stack;
static int stack_n;

wint_t pushkey;

/* Legacy external name used by a few call sites; keep as a view of the stack. */
WIN **wins;

static void
sync_wins_alias (void)
{
  wins = stack;
}

static WINDOW *
canvas_for_vk (void *vk, int vk_kind)
{
  WINDOW *c;

  if (vk == NULL)
    return NULL;

  switch (vk_kind)
    {
    case WIN_VK_WINDOW:
      /* Prefer framed interior when present; else the window canvas. */
      c = cboard_ui_frame_canvas ((cboard_widget_t *) vk);
      if (c != NULL)
	return c;
      return cboard_ui_widget_canvas ((cboard_widget_t *) vk);
    case WIN_VK_FILEDIALOG:
    case WIN_VK_POPUP:
    case WIN_VK_PLAIN:
    default:
      return cboard_ui_widget_canvas ((cboard_widget_t *) vk);
    }
}

static void
window_free_vk (WIN * win)
{
  if (!win || !win->vk)
    return;

  switch (win->vk_kind)
    {
    case WIN_VK_WINDOW:
      cboard_ui_window_destroy (win->vk);
      break;
    case WIN_VK_POPUP:
      cboard_ui_popup_destroy (win->vk);
      break;
    case WIN_VK_FILEDIALOG:
      cboard_ui_filedialog_destroy (win->vk);
      break;
    case WIN_VK_PLAIN:
    default:
      cboard_ui_widget_destroy (win->vk);
      break;
    }

  win->vk = NULL;
  win->w = NULL;
}

static void
window_free_one (WIN * win)
{
  if (!win)
    return;

  free (win->title);
  window_free_vk (win);

  if (win->freedata && win->data)
    free (win->data);

  free (win);
}

static WIN *
window_push (const char *title, void *vk, int vk_kind, int h, int w,
	     int y, int x, window_func func, void *data,
	     window_exit_func efunc, window_resize_func rfunc)
{
  WIN *win;

  win = Calloc (1, sizeof (WIN));
  win->vk = vk;
  win->vk_kind = vk_kind;
  win->w = canvas_for_vk (vk, vk_kind);
  win->data = data;
  win->rows = h;
  win->cols = w;
  win->posy = y;
  win->posx = x;
  win->func = func;
  win->efunc = efunc;
  win->rfunc = rfunc;
  win->title = (title) ? strdup (title) : NULL;

  stack = Realloc (stack, (stack_n + 2) * sizeof (WIN *));
  stack[stack_n++] = win;
  stack[stack_n] = NULL;
  sync_wins_alias ();

  /* Top of paint order for this modal. */
  if (vk)
    {
      cboard_ui_widget_raise ((cboard_widget_t *) vk);
      cboard_ui_widget_show ((cboard_widget_t *) vk);
    }

  return win;
}

WIN *
window_create (const char *title, int h, int w, int y, int x,
	       window_func func, void *data, window_exit_func efunc,
	       window_resize_func rfunc)
{
  void *vk = cboard_ui_widget_new (h, w, y, x);

  return window_push (title, vk, WIN_VK_PLAIN, h, w, y, x, func, data,
		      efunc, rfunc);
}

WIN *
window_adopt (const char *title, void *vk, int vk_kind, int h, int w,
	      int y, int x, window_func func, void *data,
	      window_exit_func efunc, window_resize_func rfunc)
{
  return window_push (title, vk, vk_kind, h, w, y, x, func, data, efunc,
		      rfunc);
}

WIN *
window_top (void)
{
  if (stack_n < 1)
    return NULL;
  return stack[stack_n - 1];
}

WIN *
window_at (int index)
{
  if (index < 0 || index >= stack_n)
    return NULL;
  return stack[index];
}

int
window_depth (void)
{
  return stack_n;
}

void
window_raise_all (void)
{
  int i;

  for (i = 0; i < stack_n; i++)
    {
      if (stack[i] && stack[i]->vk)
	cboard_ui_widget_raise ((cboard_widget_t *) stack[i]->vk);
    }
}

void
window_destroy (WIN * win)
{
  int i, j;

  if (!win || stack_n < 1)
    return;

  for (i = 0; i < stack_n; i++)
    {
      if (stack[i] != win)
	continue;

      window_free_one (win);

      for (j = i; j < stack_n - 1; j++)
	stack[j] = stack[j + 1];
      stack_n--;
      if (stack_n == 0)
	{
	  free (stack);
	  stack = NULL;
	}
      else
	{
	  stack = Realloc (stack, (stack_n + 1) * sizeof (WIN *));
	  stack[stack_n] = NULL;
	}
      sync_wins_alias ();

      /* Re-raise remaining modals so z-order stays bottom→top. */
      window_raise_all ();
      return;
    }
}

void
window_draw_title (WINDOW * win, const char *title, int width, chtype attr,
		   chtype battr)
{
  int i;

  if (title)
    {
      wchar_t *p;

      wattron (win, attr);

      for (i = 1; i < width - 1; i++)
	mvwaddch (win, 1, i, ' ');

      if (mblen (title, strlen (title)) > width)
	{
	  p = str_etc (title, width - 2, 1);
	}
      else
	p = str_to_wchar (title);

      mvwaddwstr (win, 1, CENTERX (width, p), p);
      wattroff (win, attr);
      free (p);
    }

  wattron (win, battr);
  box (win, ACS_VLINE, ACS_HLINE);
  wattroff (win, battr);
}

void
window_draw_prompt (WINDOW * win, int y, int width, const char *str,
		    chtype attr)
{
  int i;

  wattron (win, attr);

  for (i = 1; i < width - 1; i++)
    mvwaddch (win, y, i, ' ');

  wchar_t *promptw = str_to_wchar (str);
  mvwprintw (win, y, CENTERX (width, promptw), "%ls", promptw);
  free (promptw);
  wattroff (win, attr);
}

void
window_resize_all (void)
{
  int i;

  for (i = 0; i < stack_n; i++)
    {
      WIN *w = stack[i];

      if (w && w->rfunc)
	w->rfunc (w);
    }
}
