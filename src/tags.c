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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <limits.h>
#include <errno.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "common.h"
#include "conf.h"
#include "colors.h"
#include "strings.h"
#include "window.h"
#include "input.h"
#include "misc.h"
#include "message.h"
#include "menu.h"
#include "keys.h"

static struct country_codes {
    char code[4];
    char country[64];
} *ccodes;

static int init_country_codes()
{
    FILE *fp;
    char line[LINE_MAX], *s;
    int cindex = 0;

    if ((fp = fopen(config.ccfile, "r")) == NULL) {
	cmessage(ERROR_STR, ANY_KEY_STR, "%s: %s", config.ccfile, strerror(errno));
	return 1;
    }

    while ((s = fgets(line, sizeof(line), fp)) != NULL) {
	char *tmp;

	if ((tmp = strsep(&s, " ")) == NULL)
	    continue;

	s = trim(s);
	tmp = trim(tmp);

	if (!s || !tmp)
	    continue;

	ccodes = Realloc(ccodes, (cindex + 2) * sizeof(struct country_codes));
	strncpy(ccodes[cindex].code, tmp, sizeof(ccodes[cindex].code));
	ccodes[cindex].code[sizeof(ccodes[cindex].code)-1] = 0;
	strncpy(ccodes[cindex].country, s, sizeof(ccodes[cindex].country));
	ccodes[cindex].country[sizeof(ccodes[cindex].country)-1] = 0;
	cindex++;
    }

    memset(&ccodes[cindex], '\0', sizeof(struct country_codes));
    fclose(fp);
    return 0;
}

static struct menu_item_s **get_cc_items(WIN *win)
{
    int i;
    struct menu_input_s *m = win->data;
    struct menu_item_s **items = m->items;

    if (items) {
	for (i = 0; items[i]; i++)
	    free(items[i]);
    }

    for (i = 0; ccodes[i].code[0]; i++) {
	items = Realloc(items, (i+2) * sizeof(struct menu_item_s *));
	items[i] = Malloc(sizeof(struct menu_item_s));
	items[i]->name = ccodes[i].code;
	items[i]->value = ccodes[i].country;
	items[i]->selected = 0;
    }

    items[i] = NULL;
    m->total = i;
    m->items = items;
    m->nofree = 1;
    return items;
}

static void do_cc_help(struct menu_input_s *m)
{
    message(_("Country Code Keys"), ANY_KEY_STR, "%s",
	    _ (
	       "    UP/DOWN - previous/next menu item\n"
	       "   HOME/END - first/last menu item\n"
	       "  PGDN/PGUP - next/previous page\n"
	       "  a-zA-Z0-9 - jump to item\n"
	       "      ENTER - select item\n"
	       "     ESCAPE - cancel"
	       ));
}

static void do_cc_abort(struct menu_input_s *m)
{
    pushkey = -1;
}

static void do_cc_finalize(WIN *win)
{
    struct input_s *in = win->data;
    TAG *t = (TAG *)in->arg;

    set_field_buffer(in->fields[0], 0, t->value);
}

static void do_cc_save(struct menu_input_s *m)
{
    struct input_s *in = m->data;
    TAG *t = (TAG *)in->arg;
    int len = strlen(m->items[m->selected]->name);

    t->value = Realloc(t->value, len + 1);
    strcpy(t->value, m->items[m->selected]->name);
    pushkey = -1;
}

static void cc_print(WIN *win)
{
    struct menu_input_s *m = win->data;

    mvwprintw(win->w, m->print_line, 1, "%s %-*s", m->item->name,
	    win->cols - 6, m->item->value);
}

static void country_codes(void *arg)
{
    struct menu_key_s **keys = NULL;

    if (!ccodes) {
	if (init_country_codes())
	    return;
    }

    add_menu_key(&keys, KEY_F(1), do_cc_help);
    add_menu_key(&keys, KEY_ESCAPE, do_cc_abort);
    add_menu_key(&keys, '\n', do_cc_save);
    construct_menu(0, 0, -1, -1, _("Country Codes"), 0, get_cc_items, keys, arg,
		   cc_print, do_cc_finalize);
    return;
}

void add_custom_tags(TAG ***t)
{
    int i;
    int total = pgn_tag_total(config.tag);

    if (!config.tag)
	return;

    for (i = 0; i < total; i++)
	pgn_tag_add(t, config.tag[i]->name, config.tag[i]->value);

    pgn_tag_sort(*t);
}

static struct menu_item_s **get_tag_items(WIN *win)
{
    int i, n;
    struct menu_input_s *m = win->data;
    struct menu_item_s **items = m->items;
    TAG **t = (m->data) ? m->data : gp->tag;

    if (items) {
	for (i = 0; items[i]; i++)
	    free(items[i]);
    }

    n = pgn_tag_total(t);

    for (i = 0; i < n; i++) {
	items = Realloc(items, (i+2) * sizeof(struct menu_item_s *));
	items[i] = Malloc(sizeof(struct menu_item_s));
	items[i]->name = t[i]->name;
	items[i]->value = t[i]->value;
	items[i]->selected = 0;
    }

    items[i] = NULL;
    m->total = i;
    m->items = items;
    m->nofree = 1;
    return items;
}

static void edit_tag_add_fen(struct menu_input_s *m)
{
    TAG **t = m->data;
    struct userdata_s *d = gp->data;
    char *fen = pgn_game_to_fen(gp, d->b);

    pgn_tag_add(&t, "FEN", fen);
    free (fen);
    m->data = t;
}

static void edit_tag_abort(struct menu_input_s *m)
{
    TAG **t = m->data;

    pgn_tag_free(t);
    m->data = NULL;
    pushkey = -1;
    update_status_notify(gp, _("Tag edit aborted."));
}

static void edit_tag_add_finalize(WIN *w)
{
    struct input_data_s *in = w->data;
    char *name = in->moredata;
    char *value = in->str;
    struct menu_input_s *m = in->data;
    TAG **t = m->data;
    char buf[32];
    struct tm *tm;
    time_t now;
    char *tmp;
    int count;

    if (!value || !*value) {
	if (strcasecmp(name, "Round") == 0)
	    value = "-";
	else if (strcasecmp(name, "Result") == 0)
	    value = "*";
	else if (strcasecmp(name, "Date") == 0) {
	    time(&now);
	    tm = localtime(&now);
	    strftime(buf, sizeof(buf), PGN_TIME_FORMAT, tm);
	    value = buf;
	}
	else
	    value = "?";
    }

    tmp = trim_multi(value);
    count = pgn_tag_total(t);
    pgn_tag_add(&t, name, tmp);
    free(tmp);
    m->data = t;

    if (count != pgn_tag_total(t))
	m->selected = m->total;

    if (in->str)
	free(in->str);

    free(name);
    free(in);
    pushkey = REFRESH_MENU;
}

static void set_menu_stuff(TAG **t, char *name, char **init, int *type,
	int *lines, input_func **func, wint_t *key, char **eprompt, void **arg)
{
    int n;
    char *p;

    if ((n = pgn_tag_find(t, name)) != -1) {
	p = t[n]->value;
	*init = p;
    }

    if (strcasecmp(name, "Date") == 0) {
	*type = FIELD_TYPE_PGN_DATE;
	*lines = 1;
    }
    else if (strcasecmp(name, "Site") == 0) {
	*func = country_codes;
	*key = CTRL_KEY('t');
	*eprompt = _("Type CTRL-t for country codes");
	*arg = t[n];
    }
    else if (strcasecmp(name, "Round") == 0) {
	*type = FIELD_TYPE_PGN_ROUND;
	*lines = 1;
    }
    else if (strcasecmp(name, "Result") == 0) {
	*type = FIELD_TYPE_PGN_RESULT;
	*lines = 1;
    }
}

static void edit_tag_value(struct menu_input_s *m)
{
    char buf[COLS - 4];
    struct input_data_s *in = Calloc(1, sizeof(struct input_data_s));
    char *init = NULL;
    TAG **t = m->data;
    char *name;
    int type = -1;
    input_func *func = NULL;
    wint_t key = 0;
    char *eprompt = NULL;
    void *arg = NULL;
    int lines = MAX_PGN_LINE_LEN / INPUT_WIDTH;
    wchar_t *wc;

    in->data = m;
    name = strdup(t[m->selected]->name);
    in->moredata = name;
    in->efunc = edit_tag_add_finalize;
    wc = str_to_wchar (name);
    snprintf(buf, sizeof(buf), "%s \"%ls\"", _("Editing Tag"), wc);
    free (wc);
    set_menu_stuff(t, name, &init, &type, &lines, &func, &key, &eprompt, &arg);
    construct_input(buf, init, lines, 0, eprompt, func, arg, key, in, -1, type);
}

static void edit_tag_add_name_finalize(WIN *w)
{
    struct input_data_s *in = w->data;
    struct input_data_s *inv;
    struct menu_input_s *m = in->data;
    TAG **t = m->data;
    char buf[COLS - 4];
    char *init = NULL;
    char *name;
    wint_t key = 0;
    int type = -1;
    input_func *func = NULL;
    char *eprompt = NULL;
    int lines = MAX_PGN_LINE_LEN / INPUT_WIDTH;
    void *arg = NULL;
    wchar_t *wc;

    if (!in->str)
	return;

    name = strdup(in->str);
    inv = Calloc(1, sizeof(struct input_data_s));
    inv->efunc = edit_tag_add_finalize;
    inv->data = in->data;
    inv->moredata = name;
    free(in->str);
    free(in);
    wc = str_to_wchar (name);
    snprintf(buf, sizeof(buf), "%s \"%ls\"", _("Editing Tag"), wc);
    free (wc);
    set_menu_stuff(t, name, &init, &type, &lines, &func, &key, &eprompt, &arg);
    construct_input(buf, init, lines, 0, eprompt, func, arg, key, inv, -1, type);
}

static void edit_tag_add(struct menu_input_s *m)
{
    struct input_data_s *in = Calloc(1, sizeof(struct input_s));

    in->data = m;
    in->efunc = edit_tag_add_name_finalize;
    construct_input(_("New Tag Name"), NULL, 1, 1, NULL, NULL, NULL, 0, in, -1,
	    FIELD_TYPE_PGN_TAG_NAME);
}

static void edit_tag_remove(struct menu_input_s *m)
{
    TAG **data = NULL;
    TAG **t = m->data;
    int i, n = pgn_tag_total(t);

    if (m->selected < 7) {
        cmessage(NULL, ANY_KEY_STR, "%s", _ ("Cannot remove the Seven Tag Roster"));
	return;
    }

    for (i = 0; i < n; i++) {
	if (i == m->selected)
	    continue;

	pgn_tag_add(&data, t[i]->name, t[i]->value);
    }

    pgn_tag_free(t);
    m->data = data;
    m->update = 1;
}

static void edit_tag_add_custom(struct menu_input_s *m)
{
    TAG **t = m->data;

    add_custom_tags(&t);
    m->data = t;
}

static void edit_tag_save(struct menu_input_s *m)
{
    TAG **t = m->data;
    struct userdata_s *d = gp->data;

    if (!m->data)
	return;

    pgn_tag_free(gp->tag);
    pgn_tag_sort(t);
    gp->tag = t;
    pushkey = -1;
    SET_FLAG(d->flags, CF_MODIFIED);

    /*
     * In case of editing a FEN tag. Must not be MODE_PLAY. Also updates the
     * games ply count for the fifty move draw rule.
     */
    if (d->mode != MODE_PLAY)
	pgn_board_update(gp, d->b, gp->hindex);
}

static void edit_tag_help(struct menu_input_s *m)
{
    message(_("Tag Editing Keys"), ANY_KEY_STR,
	    _ (
	       "    UP/DOWN - previous/next menu item\n"
	       "   HOME/END - first/last menu item\n"
	       "  PGDN/PGUP - next/previous page\n"
	       "  a-zA-Z0-9 - jump to item\n"
	       "      ENTER - edit select item\n"
	       "     CTRL-a - add an entry\n"
	       "     CTRL-f - add FEN tag from current position\n"
	       "     CTRL-r - remove selected entry\n"
	       "     CTRL-t - add custom tags\n"
	       "     CTRL-x - quit with changes\n"
	       "     ESCAPE - quit without changes"
	       ));
}

static void view_tag_help(struct menu_input_s *m)
{
  message(_("Tag Viewing Keys"), ANY_KEY_STR,
	  _ (
	     "    UP/DOWN - previous/next menu item\n"
	     "   HOME/END - first/last menu item\n"
	     "  PGDN/PGUP - next/previous page\n"
	     "  a-zA-Z0-9 - jump to item\n"
	     "      ENTER - view selected item\n"
	     "     ESCAPE - cancel"
	     ));
}

static void view_tag_quit(struct menu_input_s *m)
{
    pushkey = -1;
}

static void view_tag_value(struct menu_input_s *m)
{
    struct menu_item_s *item = m->items[m->selected];
    char buf[COLS - 4];
    wchar_t *wc = str_to_wchar (item->name);

    snprintf(buf, sizeof(buf), "%s \"%ls\"", _("Viewing Tag"), wc);
    free (wc);
    construct_message(buf, ANY_KEY_STR, 0, 1, NULL,
		      NULL, NULL, NULL, 0, 0, item->value);
}

static void tag_print(WIN *win)
{
    int i, len = 0, n;
    struct menu_input_s *m = win->data;
    wchar_t *wc;

    for (i = 0; m->items[i]; i++) {
        wc = translate_tag_name (m->items[i]->name);
	n = wcslen (wc);
	free (wc);
	if (len < n)
	    len = n;
    }

    wc = translate_tag_name(m->item->name);
    mvwprintw(win->w, m->print_line, 1, "%ls", wc);

    for (n = wcslen(wc) + 1; n <= len; n++)
	mvwprintw(win->w, m->print_line, n, "%c", '.');

    free (wc);
    mvwprintw(win->w, m->print_line, n, ": ");
    i = win->cols - n - 3;
    wc = str_etc (m->item->value, i, 0);
    mvwprintw(win->w, m->print_line, n + 2, "%-*ls", i, wc);
    free (wc);

    if (m->update) {
	wmove(stdscr, 0, 0);
	wclrtobot(stdscr);
	update_all(gp);
	m->update = 0;
    }
}

void edit_tags(GAME g, BOARD b, int edit)
{
    struct menu_key_s **keys = NULL;
    TAG **data = NULL;
    int i;

    if (edit) {
	for (i = 0; gp->tag[i]; i++)
	    pgn_tag_add(&data, gp->tag[i]->name, gp->tag[i]->value);

	add_menu_key(&keys, '\n', edit_tag_value);
	add_menu_key(&keys, CTRL_KEY('f'), edit_tag_add_fen);
	add_menu_key(&keys, CTRL_KEY('a'), edit_tag_add);
	add_menu_key(&keys, CTRL_KEY('r'), edit_tag_remove);
	add_menu_key(&keys, CTRL_KEY('t'), edit_tag_add_custom);
	add_menu_key(&keys, CTRL_KEY('x'), edit_tag_save);
	add_menu_key(&keys, KEY_ESCAPE, edit_tag_abort);
	add_menu_key(&keys, KEY_F(1), edit_tag_help);
    }
    else {
	add_menu_key(&keys, '\n', view_tag_value);
	data = gp->tag;
	add_menu_key(&keys, KEY_ESCAPE, view_tag_quit);
	add_menu_key(&keys, KEY_F(1), view_tag_help);
    }

    construct_menu(0, 0, -1, -1,
		   (edit) ? _("Editing Roster Tags") : _("Viewing Roster Tags"),
		   0, get_tag_items, keys, data, tag_print, NULL);
}
