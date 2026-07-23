/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2024 Ben Kibbey <bjk@luxsci.net>
    Copyright (C) 2026 cboard VDK port

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
*/
#ifndef WINDOW_H
#define WINDOW_H

#define WINDOW_TIMEOUT 70
#define CTRL_KEY(x) ((x) & 0x1f)
#define KEY_ESCAPE CTRL_KEY('[')
#define CALCPOSY(y) ((y > LINES - 1) ? 0 : LINES / 2 - y / 2)
#define CALCPOSX(x) (COLS / 2 - x / 2)
#define CENTERX(x, str) abs((x / 2 - (int) wcslen(str) / 2))
#define CENTER_INT(x, n) (x / 2 - n / 2)

typedef struct window_s WIN;
typedef int(window_func)(WIN *);
typedef void(window_exit_func)(WIN *);
typedef void(window_resize_func)(WIN *);

/* How to free win->vk when the WIN is destroyed. */
enum
{
  WIN_VK_PLAIN = 0, /* bare canvas widget */
  WIN_VK_WINDOW,    /* vk_window_t */
  WIN_VK_POPUP,     /* vk_popup_t */
  WIN_VK_FILEDIALOG /* vk_filedialog_t */
};

/* App-level dialog class (do not cast win->data without this). */
enum
{
  WIN_APP_GENERIC = 0,
  WIN_APP_MENU,       /* construct_menu — data is menu_input_s */
  WIN_APP_MESSAGE,    /* construct_message — data is message_s */
  WIN_APP_INPUT,      /* construct_input — data is vdk_input_s */
  WIN_APP_CONFIRM,    /* construct_confirm — data is confirm_s */
  WIN_APP_FILEBROWSER /* file_browser — data is fb_state_s */
};

struct window_s
{
  WINDOW *w;
  /* VDK widget (opaque cboard_widget_t *); replaces ncurses PANEL. */
  void *vk;
  int vk_kind;
  int app_kind; /* WIN_APP_* — type of win->data */
  int rows;
  int cols;
  int posy;
  int posx;
  char *title;
  /*
   * Key handler for the top-of-stack modal.  game_loop always reads keys
   * from stdscr and dispatches here.  Return 0 to run efunc (if any) and
   * destroy this window.
   */
  window_func *func;
  window_exit_func *efunc;
  window_resize_func *rfunc;
  void *data;
  wint_t c;
  int freedata; /* free() .data when destroying */
};

/* Legacy alias of the modal stack (NULL-terminated). Prefer window_top(). */
extern WIN **wins;
extern wint_t pushkey;

WIN *window_create(const char *title, int h, int w, int y, int x,
                   window_func, void *data, window_exit_func,
                   window_resize_func);
/* Adopt a pre-built VDK widget already attached (or about to be). */
WIN *window_adopt(const char *title, void *vk, int vk_kind,
                  int h, int w, int y, int x, window_func, void *data,
                  window_exit_func, window_resize_func);
void window_destroy(WIN *);

/* Modal stack helpers for game_loop / compositor. */
WIN *window_top(void);
WIN *window_at(int index); /* 0 = bottom; NULL if out of range */
int window_depth(void);
/* Raise every live modal in bottom→top order (VDK paint order). */
void window_raise_all(void);

void window_draw_title(WINDOW *, const char *, int, chtype, chtype);
void window_draw_prompt(WINDOW *, int, int, const char *, chtype);
void window_resize_all(void);

#endif
