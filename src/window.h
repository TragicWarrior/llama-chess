/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2015 Ben Kibbey <bjk@luxsci.net>

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
#ifndef WINDOW_H
#define WINDOW_H

#define WINDOW_TIMEOUT		70
#define CTRL_KEY(x)		((x) & 0x1f)
#define KEY_ESCAPE		CTRL_KEY('[')
#define CALCPOSY(y)		((y > LINES - 1) ? 0 : LINES / 2 - y / 2)
#define CALCPOSX(x)		(COLS / 2 - x / 2)
#define CENTERX(x, str)		abs((x / 2 - wcslen (str) / 2))
#define CENTER_INT(x, n)	abs((x / 2 - n / 2))

typedef struct window_s WIN;
typedef int (window_func) (WIN *);
typedef void (window_exit_func) (WIN *);

struct window_s {
    WINDOW *w;
    PANEL *p;
    int rows;
    int cols;
    int posy;
    int posx;
    char *title;
    /*
     * Function that is called when a key is pressed from game_loop(). This is
     * the only place where a key is gotten from. This is for the top window
     * (LIFO). When the function returns -1, the window is destroyed and the
     * top becomes top - 1.
     */
    window_func *func;
    window_exit_func *efunc;
    void *data;
    wint_t c;
    int keep;
    int freedata; // Whether or not to free() .data when destroying
};

WIN **wins;
wint_t pushkey;

WIN *window_create(const char *title, int h, int w, int y, int x, window_func,
	void *data, window_exit_func);
void window_destroy(WIN *);
void window_draw_title(WINDOW *, const char *, int, chtype, chtype);
void window_draw_prompt(WINDOW *, int, int, const char *, chtype);

#endif
