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

/*
 * Input via vk_kmio (keyboard + SGR mouse).  Caller sets wtimeout(stdscr).
 * Returns:
 *   0 — timeout / no event
 *   1 — key in *key (wint_t code from vk_kmio_fetch)
 *   2 — mouse; *mev filled (only if mev != NULL)
 */
int cboard_ui_poll_event (wint_t *key, MEVENT *mev);

/* Geometry uses ncurses-style (height, width, y, x). */
cboard_widget_t *cboard_ui_widget_new (int height, int width, int y, int x);
/*
 * Titled chrome panel: vk_window frame + expand child for app content.
 * Destroy with cboard_ui_window_destroy(). Interior canvas via
 * cboard_ui_frame_canvas / cboard_ui_frame_resize.
 */
cboard_widget_t *cboard_ui_frame_new (int height, int width, int y, int x,
				      const char *title,
				      short body_fg, short body_bg,
				      short border_fg, short border_bg);
WINDOW *cboard_ui_frame_canvas (cboard_widget_t *frame);
/* Resize frame; returns the (possibly new) interior canvas. */
WINDOW *cboard_ui_frame_resize (cboard_widget_t *frame, int height, int width);
/* Composite border/title + child into the frame canvas after painting. */
void cboard_ui_frame_paint (cboard_widget_t *frame);
/* Attach an already-built VDK widget (window/popup/filedialog/etc.). */
void cboard_ui_widget_attach (cboard_widget_t *w, int y, int x);
void cboard_ui_widget_destroy (cboard_widget_t *w);
/* Destroy a typed composite (window/popup/filedialog) after detach. */
void cboard_ui_window_destroy (cboard_widget_t *w);
void cboard_ui_popup_destroy (cboard_widget_t *w);
void cboard_ui_filedialog_destroy (cboard_widget_t *w);
WINDOW *cboard_ui_widget_canvas (cboard_widget_t *w);
void cboard_ui_widget_move (cboard_widget_t *w, int y, int x);
/* Resize may recreate the canvas; returns the new canvas. */
WINDOW *cboard_ui_widget_resize (cboard_widget_t *w, int height, int width);
void cboard_ui_widget_show (cboard_widget_t *w);
void cboard_ui_widget_hide (cboard_widget_t *w);
int cboard_ui_widget_hidden (cboard_widget_t *w);
/* Raise to top of paint order (detach + re-attach). */
void cboard_ui_widget_raise (cboard_widget_t *w);
void cboard_ui_push_key (cboard_widget_t *w, int key);

/*
 * Overlay stack: widgets that must paint above chrome (menubar, dropdowns).
 * Cleared at the start of each refresh; push in bottom-to-top order.
 * cboard_ui_refresh() raises them, composites the screen, then redraws
 * them once more so they cannot be occluded by board/status.
 */
void cboard_ui_front_clear (void);
void cboard_ui_front_push (cboard_widget_t *w);

int cboard_ui_active (void);

#endif
