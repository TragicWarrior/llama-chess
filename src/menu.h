/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2013 Ben Kibbey <bjk@luxsci.net>

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
#ifndef MENU_H
#define MENU_H

#define MAX_MENU_HEIGHT (LINES - 4)
#define MAX_MENU_WIDTH  (COLS - 4)
#define REFRESH_MENU	-2

struct menu_item_s {
    char *name;
    char *value;
    int selected;
};

typedef struct menu_input_s MENU_INPUT;
typedef void (menu_key)(MENU_INPUT *);

struct menu_key_s {
    int c;
    menu_key *func;
    void *data;
};

typedef void (menu_print_func)(WIN *);
typedef struct menu_item_s **(menu_items)(WIN *);

struct menu_input_s {
    int update;
    int selected;
    int nofree;
    int top;
    int total;
    menu_items *func;
    menu_key *draw_exit_func;
    menu_print_func *print_func;
    int print_line;
    int name_only;
    struct menu_item_s **items;
    struct menu_item_s *item;
    struct menu_key_s **keys;
    void *data;
    char search[16];
    int rstatic;
    int cstatic;
    int ystatic;
    int xstatic;
};

void add_menu_key(struct menu_key_s ***dst, int c, menu_key func);
WIN *construct_menu(int rows, int cols, int y, int x, const char *title, 
	int name_only, menu_items *func, struct menu_key_s **keys, void *data, 
	menu_print_func *pfunc, window_exit_func *efunc);

#endif
