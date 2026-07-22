/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    VDK (libviper) screen / widget helpers for cboard.

    Owns the terminal via vk_screen and provides canvas-backed widgets used by
    the permanent chrome and the modal window stack.
*/
#ifndef UI_SCREEN_H
#define UI_SCREEN_H

#include "common.h"

/* Opaque to most call sites; cast to vk_widget_t * inside ui_screen.c. */
typedef void cboard_widget_t;

void cboard_ui_init (void);
void cboard_ui_shutdown (void);
void cboard_ui_refresh (void);
void cboard_ui_resize (void);

/* Geometry uses ncurses-style (height, width, y, x). */
cboard_widget_t *cboard_ui_widget_new (int height, int width, int y, int x);
void cboard_ui_widget_destroy (cboard_widget_t *w);
WINDOW *cboard_ui_widget_canvas (cboard_widget_t *w);
void cboard_ui_widget_move (cboard_widget_t *w, int y, int x);
/* Resize may recreate the canvas; returns the new canvas. */
WINDOW *cboard_ui_widget_resize (cboard_widget_t *w, int height, int width);
void cboard_ui_widget_show (cboard_widget_t *w);
void cboard_ui_widget_hide (cboard_widget_t *w);
int cboard_ui_widget_hidden (cboard_widget_t *w);
/* Raise to top of paint order (detach + re-attach). */
void cboard_ui_widget_raise (cboard_widget_t *w);

int cboard_ui_active (void);

#endif
