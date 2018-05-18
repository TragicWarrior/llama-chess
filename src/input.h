/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2018 Ben Kibbey <bjk@luxsci.net>

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
#ifndef INPUT_H
#define INPUT_H

#define INPUT_WIDTH		((COLS / 4) * 3)

enum {
    INPUT_HIST_ENGINE,
    INPUT_HIST_CLOCK,
    INPUT_HIST_FILE,
    INPUT_HIST_MOVE_EXP,
    INPUT_HIST_GAME_EXP,
#ifdef WITH_LIBPERL
    INPUT_HIST_PERL,
#endif
    INPUT_HIST_MAX
};

enum {
    FIELD_TYPE_ALNUM, FIELD_TYPE_ALPHA, FIELD_TYPE_INTEGER,
    FIELD_TYPE_NUMERIC, FIELD_TYPE_REGEXP, FIELD_TYPE_IPV4, FIELD_TYPE_ENUM,
    FIELD_TYPE_PGN_TAG_NAME, FIELD_TYPE_PGN_DATE, FIELD_TYPE_PGN_ROUND,
    FIELD_TYPE_PGN_RESULT
};

typedef void (input_func)(void *);
struct input_s {
    char buf[MAX_PGN_LINE_LEN];
    WINDOW *sw;
    FIELD *fields[2];
    FORM *f;
    FIELDTYPE *ft;
    int w;
    int h;
    int lines;
    char *init;
    char **extra;
    input_func *func;
    char *arg;
    wint_t c;
    void *data;
    int reset;
    int hist;
};

struct input_data_s {
    void *data;
    void *moredata;
    char *str;
    window_exit_func *efunc;
};

WIN *construct_input(const char *title, const char *init, int lines, int reset,
	const char *extra_help, input_func *func, void *arg, wint_t key,
	struct input_data_s *id, int history, int type, ...);

#endif
