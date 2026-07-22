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
#include <err.h>

#include <vdk.h>

#include "ui_screen.h"

static vk_screen_t *screen;

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
}

void
cboard_ui_shutdown (void)
{
  if (screen == NULL)
    return;

  vk_screen_destroy (screen);
  screen = NULL;
}

void
cboard_ui_refresh (void)
{
  if (screen)
    vk_screen_refresh (screen);
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
  vk_screen_attach_widget (screen, 0, w);
  return (cboard_widget_t *) w;
}

void
cboard_ui_widget_attach (cboard_widget_t *widget, int y, int x)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL || screen == NULL)
    return;

  vk_widget_move (w, x, y);
  vk_screen_attach_widget (screen, 0, w);
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

  if (w == NULL)
    return;

  detach_widget (w);
  vk_widget_destroy (w);
}

void
cboard_ui_window_destroy (cboard_widget_t *widget)
{
  vk_window_t *w = (vk_window_t *) widget;

  if (w == NULL)
    return;

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
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL || screen == NULL)
    return;

  /* Detach + re-attach puts the widget last in paint order (on top). */
  vk_screen_detach_widget (screen, 0, w);
  vk_screen_attach_widget (screen, 0, w);
  vk_widget_show (w);
}

void
cboard_ui_push_key (cboard_widget_t *widget, int key)
{
  vk_widget_t *w = (vk_widget_t *) widget;

  if (w == NULL)
    return;

  vk_object_push_keystroke (VK_OBJECT (w), key);
}
