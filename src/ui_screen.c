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

#include <stdio.h>
#include <stdlib.h>
#include <err.h>

#include <vdk.h>

#include "ui_screen.h"

static vk_screen_t *screen;

#define FRONT_MAX 8
static vk_widget_t *front_stack[FRONT_MAX];
static int front_count;

void
cboard_ui_init (void)
{
  if (screen)
    return;

  screen = vk_screen_create ();
  if (screen == NULL)
    errx (EXIT_FAILURE, "%s", "Could not initialize VDK screen.");

  vdk_color_init ();
  curs_set (0);
  cbreak ();
  noecho ();
  keypad (stdscr, TRUE);
  front_count = 0;
}

void
cboard_ui_shutdown (void)
{
  if (screen == NULL)
    return;

  front_count = 0;
  vk_screen_destroy (screen);
  screen = NULL;
}

void
cboard_ui_front_clear (void)
{
  front_count = 0;
}

void
cboard_ui_front_push (cboard_widget_t *widget)
{
  vk_widget_t *w = (vk_widget_t *) widget;
  int i;

  if (w == NULL)
    return;

  /* Avoid duplicates; keep latest push as topmost. */
  for (i = 0; i < front_count; i++)
    {
      if (front_stack[i] == w)
	{
	  int j;

	  for (j = i; j < front_count - 1; j++)
	    front_stack[j] = front_stack[j + 1];
	  front_count--;
	  break;
	}
    }

  if (front_count >= FRONT_MAX)
    return;

  front_stack[front_count++] = w;
}

static void
raise_widget (vk_widget_t *w)
{
  if (w == NULL || screen == NULL)
    return;

  /* Detach first (ignore not-found) so we never leave duplicates. */
  (void) vk_screen_detach_widget (screen, 0, w);
  vk_screen_attach_widget (screen, 0, w);
  vk_widget_show (w);
}

/*
 * wgetch/wget_wch auto-wrefresh a dirty WINDOW before reading.  VDK widget
 * canvases are real newwin()s that have been mvwin()'d to their screen
 * positions, so that auto-refresh paints one layer straight onto the
 * physical terminal — on top of the already-composited stdscr, and over any
 * menubar dropdown that overlaps it.  After a composite the canvases are
 * already represented on stdscr; mark them clean so the next input wait
 * does not re-blit them.
 */
static void
untouch_canvas (vk_widget_t *w)
{
  WINDOW *canvas;

  if (w == NULL)
    return;

  canvas = vk_widget_get_canvas (w);
  if (canvas != NULL)
    untouchwin (canvas);
}

void
cboard_ui_refresh (void)
{
  int i;
  WINDOW *surface = NULL;

  if (screen == NULL)
    return;

  /* 1) Put front-stack widgets last in the surface paint list. */
  for (i = 0; i < front_count; i++)
    raise_widget (front_stack[i]);

  /* 2) Full composite (wallpaper + all widgets → stdscr). */
  vk_screen_refresh (screen);

  /*
   * 3) Draw front widgets again on top of the surface, then push to stdscr.
   *    This guarantees menubar/dropdowns cannot stay under the board even if
   *    attach order was wrong or something re-attached chrome mid-frame.
   */
  for (i = 0; i < front_count; i++)
    {
      vk_widget_t *w = front_stack[i];
      WINDOW *ws;

      if (w == NULL || !(vk_widget_get_state (w) & VK_STATE_VISIBLE))
	continue;
      ws = vk_widget_get_surface (w);
      if (ws == NULL)
	continue;
      surface = ws;
      vk_widget_draw (w);
    }

  if (surface != NULL)
    {
      overwrite (surface, stdscr);
      wrefresh (stdscr);
    }

  /* 4) Prevent per-widget auto-wrefresh from burying the composite. */
  for (i = 0; i < front_count; i++)
    untouch_canvas (front_stack[i]);
  untouchwin (stdscr);
}

void
cboard_ui_resize (void)
{
  if (screen)
    vk_screen_resize (screen);
}

int
cboard_ui_active (void)
{
  return screen != NULL;
}

cboard_widget_t *
cboard_ui_widget_new (int height, int width, int y, int x)
{
  vk_widget_t *w;

  if (screen == NULL || height < 1 || width < 1)
    return NULL;

  w = vk_widget_create (width, height);
  if (w == NULL)
    return NULL;

  vk_widget_move (w, x, y);
  /* Drop any stale attach then attach once. */
  (void) vk_screen_detach_widget (screen, 0, w);
  vk_screen_attach_widget (screen, 0, w);
  return (cboard_widget_t *) w;
}

cboard_widget_t *
cboard_ui_frame_new (int height, int width, int y, int x,
		     const char *title, short body_fg, short body_bg,
		     short border_fg, short border_bg)
{
  vk_window_t *win;
  vk_widget_t *body;
  char titlebuf[128];
  int inner_w, inner_h;

  if (screen == NULL || height < 3 || width < 3)
    return NULL;

  win = vk_window_create (width, height);
  if (win == NULL)
    return NULL;

  if (title && title[0])
    {
      snprintf (titlebuf, sizeof (titlebuf), " %s ", title);
      vk_window_set_title (win, titlebuf);
    }

  vk_window_set_border_style (win, VK_BORDER_SINGLE);
  vk_window_set_border_colors (win, border_fg, border_bg);
  vk_widget_set_colors (VK_WIDGET (win), body_fg, body_bg);

  inner_w = width - 2;
  inner_h = height - 2;
  if (inner_w < 1)
    inner_w = 1;
  if (inner_h < 1)
    inner_h = 1;

  body = vk_widget_create (inner_w, inner_h);
  if (body == NULL)
    {
      vk_window_destroy (win);
      return NULL;
    }

  vk_widget_set_colors (body, body_fg, body_bg);
  vk_widget_set_expand (body);
  vk_window_set_child (win, body);

  vk_widget_move (VK_WIDGET (win), x, y);
  (void) vk_screen_detach_widget (screen, 0, VK_WIDGET (win));
  vk_screen_attach_widget (screen, 0, VK_WIDGET (win));
  vk_widget_show (VK_WIDGET (win));
  vk_window_update (win);

  return (cboard_widget_t *) win;
}

WINDOW *
cboard_ui_frame_canvas (cboard_widget_t *frame)
{
  vk_widget_t *child;

  if (frame == NULL)
    return NULL;

  child = vk_window_get_child (VK_WINDOW (frame));
  if (child == NULL)
    return NULL;

  return vk_widget_get_canvas (child);
}

WINDOW *
cboard_ui_frame_resize (cboard_widget_t *frame, int height, int width)
{
  vk_window_t *win = (vk_window_t *) frame;
  vk_widget_t *child;
  int inner_w, inner_h;

  if (win == NULL || height < 3 || width < 3)
    return NULL;

  vk_widget_resize (VK_WIDGET (win), width, height);

  child = vk_window_get_child (win);
  if (child == NULL)
    return NULL;

  /* Expand handles most cases; force interior metrics after recreate. */
  inner_w = width - 2;
  inner_h = height - 2;
  if (inner_w < 1)
    inner_w = 1;
  if (inner_h < 1)
    inner_h = 1;
  vk_widget_resize (child, inner_w, inner_h);

  return vk_widget_get_canvas (child);
}

void
cboard_ui_frame_paint (cboard_widget_t *frame)
{
  if (frame == NULL)
    return;

  vk_window_update (VK_WINDOW (frame));
}

void
cboard_ui_widget_attach (cboard_widget_t *widget, int y, int x)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL || screen == NULL)
    return;

  vk_widget_move (w, x, y);
  (void) vk_screen_detach_widget (screen, 0, w);
  vk_screen_attach_widget (screen, 0, w);
  vk_widget_show (w);
}

static void
detach_widget (vk_widget_t *w)
{
  if (w == NULL || screen == NULL)
    return;

  vk_screen_detach_widget (screen, 0, w);
}

void
cboard_ui_widget_destroy (cboard_widget_t *widget)
{
  vk_widget_t *w = (vk_widget_t *) widget;
  int i;

  if (w == NULL)
    return;

  for (i = 0; i < front_count; i++)
    {
      if (front_stack[i] == w)
	{
	  int j;

	  for (j = i; j < front_count - 1; j++)
	    front_stack[j] = front_stack[j + 1];
	  front_count--;
	  break;
	}
    }

  detach_widget (w);
  vk_widget_destroy (w);
}

void
cboard_ui_window_destroy (cboard_widget_t *widget)
{
  vk_window_t *w = (vk_window_t *) widget;
  int i;

  if (w == NULL)
    return;

  for (i = 0; i < front_count; i++)
    {
      if (front_stack[i] == VK_WIDGET (w))
	{
	  int j;

	  for (j = i; j < front_count - 1; j++)
	    front_stack[j] = front_stack[j + 1];
	  front_count--;
	  break;
	}
    }

  detach_widget (VK_WIDGET (w));
  vk_window_destroy (w);
}

void
cboard_ui_popup_destroy (cboard_widget_t *widget)
{
  vk_popup_t *p = (vk_popup_t *) widget;

  if (p == NULL)
    return;

  detach_widget (VK_WIDGET (p));
  vk_popup_destroy (p);
}

void
cboard_ui_filedialog_destroy (cboard_widget_t *widget)
{
  vk_filedialog_t *d = (vk_filedialog_t *) widget;

  if (d == NULL)
    return;

  detach_widget (VK_WIDGET (d));
  vk_filedialog_destroy (d);
}

WINDOW *
cboard_ui_widget_canvas (cboard_widget_t *widget)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL)
    return NULL;

  return vk_widget_get_canvas (w);
}

void
cboard_ui_widget_move (cboard_widget_t *widget, int y, int x)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL)
    return;

  vk_widget_move (w, x, y);
}

WINDOW *
cboard_ui_widget_resize (cboard_widget_t *widget, int height, int width)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL || height < 1 || width < 1)
    return NULL;

  vk_widget_resize (w, width, height);
  return vk_widget_get_canvas (w);
}

void
cboard_ui_widget_show (cboard_widget_t *widget)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL)
    return;

  vk_widget_show (w);
}

void
cboard_ui_widget_hide (cboard_widget_t *widget)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL)
    return;

  vk_widget_hide (w);
}

int
cboard_ui_widget_hidden (cboard_widget_t *widget)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL)
    return 1;

  return !vk_widget_is_visible (w);
}

void
cboard_ui_widget_raise (cboard_widget_t *widget)
{
  raise_widget ((vk_widget_t *) widget);
}

void
cboard_ui_push_key (cboard_widget_t *widget, int key)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL)
    return;

  vk_object_push_keystroke (VK_OBJECT (w), key);
}
