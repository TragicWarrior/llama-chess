/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2006 Ben Kibbey <bjk@luxsci.net>

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
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <string.h>
#include <panel.h>
#include <errno.h>
#include <ctype.h>
#include <pwd.h>
#include <signal.h>
#include <time.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif

#ifdef HAVE_MENU_H
#include <menu.h>
#endif

#ifdef HAVE_REGEX_H
#include <regex.h>
#endif

#include "chess.h"
#include "conf.h"
#include "window.h"
#include "colors.h"
#include "input.h"
#include "misc.h"
#include "engine.h"
#include "rcfile.h"
#include "strings.h"
#include "cboard.h"

#ifdef DEBUG
#include "debug.h"
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

static char *str_etc(const char *str, int maxlen, int rev)
{
    int len = strlen(str);
    static char buf[80], *p = buf;
    int i;

    strncpy(buf, str, sizeof(buf));

    if (len > maxlen) {
	if (rev) {
	    p = buf;
	    *p++ = '.';
	    *p++ = '.';
	    *p++ = '.';

	    for (i = 0; i < maxlen + 3; i++)
		*p++ = buf[(len - maxlen) + i + 3]; 
	}
	else {
	    p = buf + maxlen - 4;
	    *p++ = '.';
	    *p++ = '.';
	    *p++ = '.';
	}

	*p = '\0';
    }

    return buf;
}

static char *real_filename(char *path)
{
    char *tmp;
    static char buf[FILENAME_MAX];
    int slash = 0;

    if (!path[0])
	return NULL;

    strncpy(buf, path, sizeof(buf));
    tmp = buf;

    if (tmp[strlen(tmp) - 1] == '/') {
	tmp[strlen(tmp) - 1] = 0;
	slash = 1;
    }

    if ((tmp = strrchr(tmp, '/')) == NULL)
	return path;

    if (slash)
	buf[strlen(tmp)] = '/';

    return ++tmp;
}

/* FIXME castling */
static void update_cursor(GAME g, int idx, int *r, int *c)
{
    char *p;
    int len;
    int t = history_total(g.hp);

    /*
     * If not deincremented then r and c would be the next move.
     */
    idx--;

    if (idx > t || idx < 0 || !t || !g.hp[idx]->move) {
	*r = *c = 0;
	return;
    }

    p = g.hp[idx]->move;
    len = strlen(p);

    if (*p == 'O') {
	if (len <= 4)
	    *c = 7;
	else
	    *c = 3;

	*r = (g.turn == WHITE) ? 8 : 1;
	return;
    }

    p += len;

    while (!isdigit(*p))
	p--;

    *r = ROWTOINT(*p--);
    *c = COLTOINT(*p);
}

static int init_nag()
{
    FILE *fp;
    char line[LINE_MAX];
    int i = 0;

    if ((fp = fopen(config.nagfile, "r")) == NULL) {
	cmessage(ERROR, ANYKEY, "%s: %s", config.nagfile, strerror(errno));
	return 1;
    }

    while (!feof(fp)) {
	if (fscanf(fp, " %[^\n] ", line) == 1) {
	    nags = Realloc(nags, (i + 2) * sizeof(struct nag_s));
	    nags[i].line = strdup(line);
	    i++;
	}
    }

    if (nags)
	nags[i].line = NULL;
    return 0;
}

char *history_edit_nag(void *arg)
{
    WINDOW *win, *subw;
    PANEL *panel;
    ITEM **mitems = NULL;
    MENU *menu;
    int i = 0, n;
    int itemcount = 0;
    int rows, cols;
    char *mbuf = NULL;
    HISTORY *anno = (HISTORY *)arg;

    if (!nags) {
	if (init_nag())
	    return NULL;
    }

    i = 0;
    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
    mitems[i++] = new_item(NONE, NULL);

    for (n = 0; nags[n].line; n++, i++) {
	mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	mitems[i] = new_item(nags[n].line, NULL);
    }

    mitems[i] = NULL;
    menu = new_menu(mitems);
    scale_menu(menu, &rows, &cols);

    win = newwin(rows + 4, cols + 2, CALCPOSY(rows) - 2, CALCPOSX(cols));
    set_menu_win(menu, win);
    subw = derwin(win, rows, cols, 2, 1);
    set_menu_sub(menu, subw);
    set_menu_fore(menu, A_REVERSE);
    set_menu_grey(menu, A_NORMAL);
    set_menu_mark(menu, NULL);
    set_menu_spacing(menu, 0, 0, 0);
    menu_opts_off(menu, O_NONCYCLIC|O_SHOWDESC|O_ONEVALUE);
    post_menu(menu);
    panel = new_panel(win);
    cbreak();
    noecho();
    keypad(win, TRUE);
    set_menu_pattern(menu, mbuf);
    wbkgd(win, CP_MESSAGE_WINDOW);
    draw_window_title(win, NAG_EDIT_TITLE, cols + 2, CP_HISTORY_TITLE,
	    CP_HISTORY_BORDER);

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (anno->nag[i] && anno->nag[i] <= item_count(menu)) {
	    set_item_value(mitems[anno->nag[i]], TRUE);
	    set_current_item(menu, mitems[anno->nag[i]]);
	    itemcount++;
	}
    }

    while (1) {
	int c;
	char *tmp;
	char buf[cols - 4];

	wattron(win, A_REVERSE);

	for (c = 1; c < (cols + 2) - 1; c++)
	    mvwprintw(win, rows + 2, c, " ");

	c = item_index(current_item(menu)) + 1;

	snprintf(buf, sizeof(buf), "Item %i of %i (%i of %i selected) %s", c, 
		item_count(menu), itemcount, MAX_PGN_NAG, NAG_EDIT_PROMPT);
	draw_prompt(win, rows + 2, cols + 2, buf, CP_MESSAGE_PROMPT);

	wattroff(win, A_REVERSE);

	if (!itemcount) {
	    for (i = 0; mitems[i]; i++)
		set_item_value(mitems[i], FALSE);

	    set_item_value(mitems[0], TRUE);
	}
	else
	    set_item_value(mitems[0], FALSE);

	/* This nl() statement needs to be here because NL is recognized
	 * for some reason after the first selection.
	 */
	nl();
	update_panels();
	doupdate();

	c = wgetch(win);

	switch (c) {
	    int found;

	    case KEY_F(1):
		help(NAG_EDIT_HELP, ANYKEY, naghelp);
		break;
	    case KEY_RIGHT:
		if (!itemcount)
		    break;

		found = 0;

		for (i = item_index(current_item(menu)) + 1; mitems[i]; i++) {
		    if (item_value(mitems[i]) == TRUE) {
			found = i;
			break;
		    }
		}

		if (!found) {
		    for (i = 0; mitems[i]; i++) {
			if (item_value(mitems[i]) == TRUE) {
			    found = i;
			    break;
			}
		    }
		}

		set_current_item(menu, mitems[found]);
		break;
	    case KEY_LEFT:
		if (!itemcount)
		    break;

		found = 0;

		for (i = item_index(current_item(menu)) - 1; i > 0; i--) {
		    if (item_value(mitems[i]) == TRUE) {
			found = i;
			break;
		    }
		}

		if (!found) {
		    for (i = item_count(menu) - 1; i > 0; i--) {
			if (item_value(mitems[i]) == TRUE) {
			    found = i;
			    break;
			}
		    }
		}

		set_current_item(menu, mitems[found]);
		break;
	    case KEY_HOME:
		menu_driver(menu, REQ_FIRST_ITEM);
		break;
	    case KEY_END:
		menu_driver(menu, REQ_LAST_ITEM);
		break;
	    case KEY_UP:
		menu_driver(menu, REQ_UP_ITEM);
		break;
	    case KEY_DOWN:
		menu_driver(menu, REQ_DOWN_ITEM);
		break;
	    case KEY_PPAGE:
	    case CTRL('P'):
		if (menu_driver(menu, REQ_SCR_UPAGE) == E_REQUEST_DENIED)
		    menu_driver(menu, REQ_FIRST_ITEM);
		break;
	    case KEY_NPAGE:
	    case CTRL('N'):
		if (menu_driver(menu, REQ_SCR_DPAGE) == E_REQUEST_DENIED)
		    menu_driver(menu, REQ_LAST_ITEM);
		break;
	    case ' ':
		if (item_index(current_item(menu)) == 0 && 
			item_value(current_item(menu)) == FALSE) {
		    itemcount = 0;
		    break;
		}

		if (item_value(current_item(menu)) == TRUE) {
		    set_item_value(current_item(menu), FALSE);
		    itemcount--;
		}
		else {
		    if (itemcount + 1 > MAX_PGN_NAG)
			break;

		    set_item_value(current_item(menu), TRUE);
		    itemcount++;
		}

		SET_FLAG(game[gindex].flags, GF_MODIFIED);
		break;
	    case '\n':
		goto gotitem;
		break;
	    case KEY_ESCAPE:
		goto done;
		break;
	    default:
		tmp = menu_pattern(menu);

		if (tmp && tmp[strlen(tmp) - 1] != c) {
		    menu_driver(menu, REQ_CLEAR_PATTERN);
		    menu_driver(menu, c);
		}
		else {
		    if (menu_driver(menu, REQ_NEXT_MATCH) == E_NO_MATCH)
			menu_driver(menu, c);
		}

		break;
	}
    }

gotitem:
    for (i = 0; i < MAX_PGN_NAG; i++)
	anno->nag[i] = 0;

    for (i = 0, n = 0; mitems[i] && n < MAX_PGN_NAG; i++) {
	if (item_value(mitems[i]) == TRUE)
	    anno->nag[n++] = i;
    }

done:
    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; mitems[i]; i++)
	free_item(mitems[i]);

    free(mitems);
    del_panel(panel);
    delwin(subw);
    delwin(win);
    return NULL;
}

static void view_nag(void *arg)
{
    HISTORY *h = (HISTORY *)arg;
    char buf[80];
    char line[LINE_MAX] = {0};
    int i = 0;

    snprintf(buf, sizeof(buf), "Viewing NAG for \"%s\"", h->move);

    if (!nags) {
	if (init_nag())
	    return;
    }

    for (i = 0; i < MAX_PGN_NAG; i++) {
	if (!h->nag[i])
	    break;

	strncat(line, nags[h->nag[i] - 1].line, sizeof(line));
	strncat(line, "\n", sizeof(line));
    }

    line[strlen(line) - 1] = 0;
    message(buf, ANYKEY, "%s", line);
}

void view_annotation(HISTORY h)
{
    char buf[MAX_SAN_MOVE_LEN + strlen(ANNOTATION_VIEW_TITLE) + 4];
    int nag = 0, comment = 0;

    if (h.comment && h.comment[0])
        comment++;
 
    if (h.nag[0])
 	nag++;

    if (!nag && !comment)
	return;

    snprintf(buf, sizeof(buf), "%s \"%s\"", ANNOTATION_VIEW_TITLE, h.move);

    if (comment)
	show_message(buf, (nag) ? "Any other key to continue" : ANYKEY,
		(nag) ? "Press 'n' to view NAG" : NULL, 
		(nag) ? view_nag : NULL, (nag) ? (void *)&h : NULL,
		(nag) ? 'n' : 0, "%s", h.comment);
    else
	show_message(buf, "Any other key to continue", "Press 'n' to view NAG",
		view_nag, (void *)&h, 'n', "%s", "No annotations for this move");
}

static void cleanup(WINDOW *win, WINDOW *subw, PANEL *panel, MENU *menu, 
	ITEM **items, struct d_entries *entries)
{
    int i;

    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; items[i]; i++)
	free_item(items[i]);

    free(items);

    if (entries) {
	for (i = 0; entries[i].name; i++) {
	    free(entries[i].name);
	    free(entries[i].fancy);
	}

	free(entries);
    }

    del_panel(panel);
    delwin(subw);
    delwin(win);
}

static int sort_entries(const void *s1, const void *s2)
{
    const struct d_entries *ss1 = s1;
    const struct d_entries *ss2 = s2;

    return strcmp(ss1->name, ss2->name);
}

static struct d_entries *get_directory_entries(const char *path)
{
    DIR *dp;
    struct dirent *entry;
    struct d_entries *entries = NULL;
    int n = 0;

    if ((dp = opendir(path)) == NULL)
	return NULL;

    while ((entry = readdir(dp)) != NULL) {
	struct stat st;
	int len;
	char tbuf[64 + 1] = {0}; //FIXME
	struct tm *tp;
	char buf[FILENAME_MAX];
	char *tmp;
	size_t size;

	if (entry->d_name[0] == '.' && entry->d_name[1] == 0)
	    continue;

	snprintf(buf, sizeof(buf), "%s/%s", path, entry->d_name);

	if (stat(buf, &st) == -1)
	    continue;

	size = st.st_size ;
	entries = Realloc(entries, (n + 2) * sizeof(struct d_entries));
	entries[n].name = strdup(buf);
	tmp = real_filename(buf);
	len = strlen(tmp) + 2;
	entries[n].fancy = Malloc(len);
	strncpy(entries[n].fancy, tmp, len);

	if (S_ISDIR(st.st_mode))
	    entries[n].fancy[len - 2] = '/';

	tp = localtime(&st.st_mtime);
	strftime(tbuf, sizeof(tbuf), "%b %d %T", tp);

	snprintf(entries[n].desc, sizeof(entries[n].desc), "%-7i %s", 
		size, tbuf);

	memset(&entries[++n], '\0', sizeof(struct d_entries));
    }

    closedir(dp);
    qsort(entries, n, sizeof(struct d_entries), sort_entries);
    return entries;
}

char *browse_directory(void *arg)
{
    int i;
    char path[FILENAME_MAX] = {0};
    static char file[FILENAME_MAX];
    char *oldwd = getcwd(NULL, 0);
    DIR *dp;
    char *inputstr = (char *)arg;
    int initkey = (inputstr) ? inputstr[0] : 0;

    if (config.savedirectory) {
	if ((dp = opendir(config.savedirectory)) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
		    strerror(errno));
	    getcwd(path, sizeof(path));
	}
	else {
	    closedir(dp);
	    strncpy(path, config.savedirectory, sizeof(path));
	}
    }
    else
	getcwd(path, sizeof(path));

again:
    while (1) {
	WINDOW *win, *subw;
	PANEL *panel;
	ITEM **mitems = NULL;
	MENU *menu;
	char *tmp = NULL;
	int rows, cols;
	int selected = -1;
	char *mbuf = NULL;
	struct d_entries *entries = NULL;
	struct stat st;
	int idx = 0;
	int len = strlen(path);

	/* /some/path/blah/../ */
	if (path[len - 1] == '.' && path[len - 2] == '.' &&
		path[len - 3] == '/') {
	    tmp = path;
	    tmp += strlen(path) - 5;

	    /* /some/path/ */
	    while (*--tmp != '/')
		*tmp = '\0';

	    if (!*path) {
		path[0] = '/';
		path[1] = '\0';
	    }
	}

	if (path[1] && path[strlen(path) - 1] == '/')
	    path[strlen(path) - 1] = '\0';

	if ((entries = get_directory_entries(path)) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", path, strerror(errno));
	    return NULL;
	}

	for (i = 0; entries[i].name; i++) {
	    mitems = Realloc(mitems, (idx + 2) * sizeof(ITEM));
	    mitems[idx++] = new_item(entries[i].fancy, entries[i].desc);
	}

	mitems[idx] = NULL;
	menu = new_menu(mitems);
	scale_menu(menu, &rows, &cols);

	if (cols < strlen(path))
	    cols = strlen(path);

	if (cols < strlen(HELP_PROMPT))
	    cols = strlen(HELP_PROMPT);

	rows = (LINES / 5) * 4;
	cols += 2;

	win = newwin(rows + 4, cols, CALCPOSY(rows) - 2, CALCPOSX(cols));
	set_menu_format(menu, rows, 0);
	set_menu_win(menu, win);
	subw = derwin(win, rows, cols - 2, 2, 1);
	set_menu_sub(menu, subw);
	set_menu_fore(menu, A_REVERSE);
	set_menu_grey(menu, A_NORMAL);
	set_menu_mark(menu, NULL);
	set_menu_spacing(menu, 2, 0, 0);
	menu_opts_off(menu, O_NONCYCLIC);
	post_menu(menu);
	panel = new_panel(win);

	draw_window_title(win, path, cols, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);
	draw_prompt(win, rows + 2, cols, HELP_PROMPT, CP_MESSAGE_PROMPT);

	cbreak();
	noecho();
	keypad(win, TRUE);
	set_menu_pattern(menu, mbuf);

	if (isgraph(initkey)) {
	    menu_driver(menu, initkey);
	    initkey = '\0';
	}

	while (1) {
	    int c;

	    /* This nl() statement needs to be here because NL is recognized
	     * for some reason after the first selection.
	     */
	    nl();
	    update_panels();
	    doupdate();

	    c = wgetch(win);

	    switch (c) {
		case CTRL('P'):
		case KEY_PPAGE:
		    menu_driver(menu, REQ_SCR_UPAGE);
		    break;
		case ' ':
		case CTRL('N'):
		case KEY_NPAGE:
		    menu_driver(menu, REQ_SCR_DPAGE);
		    break;
		case KEY_UP:
		    menu_driver(menu, REQ_UP_ITEM);
		    break;
		case KEY_DOWN:
		    menu_driver(menu, REQ_DOWN_ITEM);
		    break;
		case '\n':
		    selected = item_index(current_item(menu));
		    goto gotitem;
		    break;
		case KEY_ESCAPE:
		    cleanup(win, subw, panel, menu, mitems, entries);
		    file[0] = '\0';
		    goto done;
		    break;
		case KEY_F(1):
		    help(BROWSER_HELP, ANYKEY, file_browser_help);
		    break;
		case '~':
		    if ((tmp = getenv("HOME")) == NULL) {
			cmessage(ERROR, ANYKEY, "%s", E_HOME_ENV);
			break;
		    }

		    strncpy(path, tmp, sizeof(path));
		    cleanup(win, subw, panel, menu, mitems, entries);
		    goto again;
		    break;
		case CTRL('X'):
		    if ((tmp = get_input_str_clear(BROWSER_CHDIR_TITLE, NULL)) 
			    == NULL)
			break;

		    tmp = tilde_expand(tmp);
		    strncpy(path, tmp, sizeof(path));
		    cleanup(win, subw, panel, menu, mitems, entries);
		    goto again;
		    break;
		default:
		    tmp = menu_pattern(menu);

		    if (tmp && tmp[strlen(tmp) - 1] != c) {
			menu_driver(menu, REQ_CLEAR_PATTERN);
			menu_driver(menu, c);
		    }
		    else {
			if (menu_driver(menu, REQ_NEXT_MATCH) == E_NO_MATCH)
			    menu_driver(menu, c);
		    }

		    break;
	    }
	}

gotitem:
	snprintf(file, sizeof(file), "%s", entries[selected].name);

	if (stat(file, &st) == -1) {
	    cmessage(ERROR, ANYKEY, "%s", strerror(errno));
	    cleanup(win, subw, panel, menu, mitems, entries);
	    continue;
	}

	cleanup(win, subw, panel, menu, mitems, entries);

	if (S_ISDIR(st.st_mode)) {
	    strncpy(path, file, sizeof(path));
	    continue;
	}

	if (S_ISREG(st.st_mode))
	    break;

	cmessage(ERROR, ANYKEY, "%s", E_NOTAREGFILE);
    }

done:
    chdir(oldwd);
    free(oldwd);
    return (*file) ? file : NULL;
}
static int init_country_codes()
{
    FILE *fp;
    char line[LINE_MAX], *s;
    int cindex = 0;

    if ((fp = fopen(config.ccfile, "r")) == NULL) {
	cmessage(ERROR, ANYKEY, "%s: %s", config.ccfile, strerror(errno));
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
	strncpy(ccodes[cindex].country, s, sizeof(ccodes[cindex].country));
	cindex++;
    }

    memset(&ccodes[cindex], '\0', sizeof(struct country_codes));
    fclose(fp);

    return 0;
}

char *country_codes(void *arg)
{
    WINDOW *win, *subw;
    PANEL *panel;
    ITEM **mitems = NULL;
    MENU *menu;
    int i = 0, n;
    int rows, cols;
    char *mbuf = NULL;
    char *tmp = NULL;

    if (!ccodes) {
	if (init_country_codes())
	    return NULL;
    }

    for (n = i = 0; ccodes[n].code[0]; n++, i++) {
	mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));
	mitems[i] = new_item(ccodes[n].country, ccodes[n].code);
    }

    mitems[i] = NULL;
    menu = new_menu(mitems);
    scale_menu(menu, &rows, &cols);

    if (cols < strlen(HELP_PROMPT) + 21)
	cols = strlen(HELP_PROMPT) + 21;

    win = newwin(rows + 4, cols + 4, CALCPOSY(rows) - 2, CALCPOSX(cols));
    set_menu_win(menu, win);
    subw = derwin(win, rows, cols + 2, 2, 1);
    set_menu_sub(menu, subw);
    set_menu_fore(menu, A_REVERSE);
    set_menu_grey(menu, A_NORMAL);
    set_menu_mark(menu, NULL);
    set_menu_spacing(menu, 0, 0, 0);
    menu_opts_off(menu, O_NONCYCLIC);
    post_menu(menu);
    panel = new_panel(win);
    cbreak();
    noecho();
    keypad(win, TRUE);
    set_menu_pattern(menu, mbuf);
    wbkgd(win, CP_MESSAGE_WINDOW);
    draw_window_title(win, CC_TITLE, cols + 4, CP_MESSAGE_TITLE,
	    CP_MESSAGE_BORDER);

    while (1) {
	int c;
	char buf[cols - 4];

	wattron(win, A_REVERSE);

	for (c = 1; c < (cols + 2) - 1; c++)
	    mvwprintw(win, rows + 2, c, " ");

	c = item_index(current_item(menu)) + 1;

	snprintf(buf, sizeof(buf), "%s %i %s %i %s", MENU_ITEM_STR, c, 
		N_OF_N_STR, item_count(menu), HELP_PROMPT);
	draw_prompt(win, rows + 2, cols + 2, buf, CP_MESSAGE_PROMPT);

	wattroff(win, A_REVERSE);

	/* This nl() statement needs to be here because NL is recognized
	 * for some reason after the first selection.
	 */
	nl();
	update_panels();
	doupdate();

	c = wgetch(win);

	switch (c) {
	    case KEY_F(1):
		help(CC_KEY_HELP, ANYKEY, cc_help);
		break;
	    case KEY_HOME:
		menu_driver(menu, REQ_FIRST_ITEM);
		break;
	    case KEY_END:
		menu_driver(menu, REQ_LAST_ITEM);
		break;
	    case KEY_UP:
		menu_driver(menu, REQ_UP_ITEM);
		break;
	    case KEY_DOWN:
		menu_driver(menu, REQ_DOWN_ITEM);
		break;
	    case KEY_PPAGE:
	    case CTRL('P'):
		if (menu_driver(menu, REQ_SCR_UPAGE) == E_REQUEST_DENIED)
		    menu_driver(menu, REQ_FIRST_ITEM);
		break;
	    case ' ':
	    case KEY_NPAGE:
	    case CTRL('N'):
		if (menu_driver(menu, REQ_SCR_DPAGE) == E_REQUEST_DENIED)
		    menu_driver(menu, REQ_LAST_ITEM);
		break;
	    case '\n':
		tmp = (char *)item_description(current_item(menu));
		goto done;
		break;
	    case KEY_ESCAPE:
		tmp = NULL;
		goto done;
		break;
	    default:
		tmp = menu_pattern(menu);

		if (tmp && tmp[strlen(tmp) - 1] != c) {
		    menu_driver(menu, REQ_CLEAR_PATTERN);
		    menu_driver(menu, c);
		}
		else {
		    if (menu_driver(menu, REQ_NEXT_MATCH) == E_NO_MATCH)
			menu_driver(menu, c);
		}

		break;
	}
    }

done:
    unpost_menu(menu);
    free_menu(menu);

    for (i = 0; mitems[i]; i++)
	free_item(mitems[i]);

    del_panel(panel);
    delwin(subw);
    delwin(win);
    return tmp;
}

static void add_custom_tags(TAG ***t)
{
    int i;

    if (!config.tag)
	return;

    for (i = 0; config.tag[i]; i++)
	pgn_add_tag(t, config.tag[i]->name, config.tag[i]->value);

    pgn_sort_tags(*t);
}

TAG **edit_tags(GAME g, BOARD b, int edit)
{
    TAG **data = NULL;
    struct tm tp;
    unsigned char data_index = 0;
    int n, lastindex = 0;
    int len;

    /* Edit the backup copy, not the original in case the save fails. */
    for (n = 0; g.tag[n]; n++)
	pgn_add_tag(&data, g.tag[n]->name, g.tag[n]->value);

    data_index = pgn_tag_total(data);

    while (1) {
	WINDOW *win, *subw;
	PANEL *panel;
	ITEM **mitems = NULL;
	MENU *menu;
	int i;
	char buf[76] = {0};
	char *tmp = NULL;
	int rows, cols;
	int selected = -1;
	char *mbuf = NULL;
	int nlen = 0, vlen = 0;

	data_index = pgn_tag_total(data);

	for (i = 0; i < data_index; i++) {
	    mitems = Realloc(mitems, (i + 2) * sizeof(ITEM));

	    if (data[i]->value) {
		nlen = strlen(data[i]->name);
		vlen = strlen(data[i]->value);

		/* The +6 is for the menu padding. */
		mitems[i] = new_item(data[i]->name,
			(nlen + vlen + 6 >= MAX_VALUE_WIDTH)
			? PRESS_ENTER : data[i]->value);
	    }
	    else
		mitems[i] = new_item(data[i]->name, UNKNOWN);
	}

	mitems[i] = NULL;
	menu = new_menu(mitems);
	scale_menu(menu, &rows, &cols);

	/* +14 for the extra prompt info. */
	if (cols < strlen(HELP_PROMPT) + 14)
	    cols = strlen(HELP_PROMPT) + 14;

	win = newwin(rows + 4, cols + 4, CALCPOSY(rows) - 2, CALCPOSX(cols));
	set_menu_win(menu, win);
	subw = derwin(win, rows, cols + 2, 2, 1);
	set_menu_sub(menu, subw);
	set_menu_fore(menu, A_REVERSE);
	set_menu_grey(menu, A_NORMAL);
	set_menu_mark(menu, NULL);
	set_menu_pad(menu, '-');
	set_menu_spacing(menu, 3, 0, 0);
	menu_opts_off(menu, O_NONCYCLIC);
	post_menu(menu);
	panel = new_panel(win);
	cbreak();
	noecho();
	nl();
	keypad(win, TRUE);
	set_menu_pattern(menu, mbuf);
	wbkgd(win, CP_MESSAGE_WINDOW);
	draw_window_title(win, (edit) ? TAG_EDIT_TITLE : TAG_VIEW_TITLE, 
		cols + 4, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);

	while (1) {
	    int c;
	    TAG **tmppgn = NULL;
	    char *newtag = NULL;

	    if (set_current_item(menu, mitems[lastindex]) != E_OK) {
		lastindex = item_count(menu) - 1;
		continue;
	    }

	    snprintf(buf, sizeof(buf), "%s %i %s %i  %s", MENU_TAG_STR,
		    item_index(current_item(menu)) + 1, N_OF_N_STR,
		    item_count(menu), HELP_PROMPT);
	    draw_prompt(win, rows + 2, cols + 4, buf, CP_MESSAGE_PROMPT);

	    update_panels();
	    doupdate();

	    c = wgetch(win);

	    switch (c) {
		case CTRL('T'):
		    add_custom_tags(&data);
		    goto cleanup;
		    break;
		case KEY_F(1):
		    if (edit)
			help(TAG_EDIT_HELP, ANYKEY, pgn_edit_help);
		    else
			help(TAG_VIEW_HELP, ANYKEY, pgn_info_help);
		    break;
		case CTRL('R'):
		    if (!edit)
			break;

		    selected = item_index(current_item(menu));

		    if (selected <= 6) {
			cmessage(NULL, ANYKEY, "%s", E_REMOVE_STR);
			goto cleanup;
		    }

		    data_index = pgn_tag_total(data);

		    for (i = 0; i < data_index; i++) {
			if (i == selected)
			    continue;

			pgn_add_tag(&tmppgn, data[i]->name, data[i]->value);
		    }

		    pgn_tag_free(data);
		    data = NULL;

		    for (i = 0; tmppgn[i]; i++)
			pgn_add_tag(&data, tmppgn[i]->name, tmppgn[i]->value);

		    pgn_tag_free(tmppgn);
		    goto cleanup;
		    break;
		case CTRL('A'):
		    if (!edit)
			break;

		    if ((newtag = get_input(TAG_NEW_TITLE, NULL, 1, 1, NULL,
				    NULL, NULL, 0, FIELD_TYPE_PGN_TAG_NAME))
			    == NULL)
			break;

		    newtag[0] = toupper(newtag[0]);

		    if (strlen(newtag) > MAX_VALUE_WIDTH - 6 - 
			    strlen(PRESS_ENTER)) {
			cmessage(ERROR, ANYKEY, "%s", E_TAG_NAMETOOLONG);
			break;
		    }

		    for (i = 0; i < data_index; i++) {
			if (strcasecmp(data[i]->name, newtag) == 0) {
			    selected = i;
			    goto gotitem;
			}
		    }

		    pgn_add_tag(&data, newtag, NULL);
		    data_index = pgn_tag_total(data);
		    selected = data_index - 1;
		    goto gotitem;
		    break;
		case KEY_HOME:
		    menu_driver(menu, REQ_FIRST_ITEM);
		    break;
		case KEY_END:
		    menu_driver(menu, REQ_LAST_ITEM);
		    break;
		case CTRL('F'):
		    if (!edit)
			break;

		    pgn_add_tag(&data, "FEN", pgn_game_to_fen(g, b));
		    data_index = pgn_tag_total(data);
		    selected = data_index - 1;
		    goto gotitem;
		    break;
		case KEY_NPAGE:
		case CTRL('N'):
		    if (menu_driver(menu, REQ_SCR_DPAGE) == E_REQUEST_DENIED)
			menu_driver(menu, REQ_LAST_ITEM);
		    break;
		case KEY_PPAGE:
		case CTRL('P'):
		    if (menu_driver(menu, REQ_SCR_UPAGE) == E_REQUEST_DENIED)
			menu_driver(menu, REQ_FIRST_ITEM);
		    break;
		case KEY_UP:
		    menu_driver(menu, REQ_UP_ITEM);
		    break;
		case KEY_DOWN:
		    menu_driver(menu, REQ_DOWN_ITEM);
		    break;
		case '\n':
		    selected = item_index(current_item(menu));
		    goto gotitem;
		    break;
		case KEY_ESCAPE:
		    cleanup(win, subw, panel, menu, mitems, NULL);
		    goto done;
		    break;
		default:
		    tmp = menu_pattern(menu);

		    if (tmp && tmp[strlen(tmp) - 1] != c) {
			menu_driver(menu, REQ_CLEAR_PATTERN);
			menu_driver(menu, c);
		    }
		    else {
			if (menu_driver(menu, REQ_NEXT_MATCH) == E_NO_MATCH)
			    menu_driver(menu, c);
		    }

		    break;
	    }

	    lastindex = item_index(current_item(menu));
	}

gotitem:
	lastindex = selected;
	nlen = strlen(data[selected]->name) + 3;
	nlen += (edit) ? strlen(TAG_EDIT_TAG_TITLE) : strlen(TAG_VIEW_TAG_TITLE);

	if (nlen > MAX_VALUE_WIDTH)
	    snprintf(buf, sizeof(buf), "%s", data[selected]->name);
	else
	    snprintf(buf, sizeof(buf), "%s \"%s\"",
		    (edit) ? TAG_EDIT_TAG_TITLE : TAG_VIEW_TAG_TITLE,
		    data[selected]->name);

	if (!edit) {
	    if (strcmp(item_description(mitems[selected]), UNKNOWN) == 0)
		goto cleanup;

	    cmessage(buf, ANYKEY, "%s", data[selected]->value);
	    goto cleanup;
	}

	if (strcmp(data[selected]->name, "Date") == 0) {
	    tmp = get_input(buf, data[selected]->value, 0, 0, NULL, NULL, NULL,
		    0, FIELD_TYPE_PGN_DATE);

	    if (tmp) {
		if (strptime(tmp, PGN_TIME_FORMAT, &tp) == NULL) {
		    cmessage(ERROR, ANYKEY, "%s", E_TAG_DATE_FMT);
		    goto cleanup;
		}
	    }
	    else
		goto cleanup;
	}
	else if (strcmp(data[selected]->name, "Site") == 0) {
	    tmp = get_input(buf, data[selected]->value, 1, 1, CC_PROMPT,
		    country_codes, NULL, CTRL('t'), -1);

	    if (!tmp)
		tmp = "?";
	}
	else if (strcmp(data[selected]->name, "Round") == 0) {
	    tmp = get_input(buf, NULL, 1, 1, NULL, NULL, NULL, 0,
		    FIELD_TYPE_PGN_ROUND);

	    if (!tmp) {
		if (gtotal > 1)
		    tmp = "?";
		else
		    tmp = "-";
	    }
	}
	else if (strcmp(data[selected]->name, "Result") == 0) {
	    tmp = get_input(buf, data[selected]->value, 1, 1, NULL, NULL, NULL, 
		    0, -1);

	    if (!tmp)
		tmp = "*";
	}
	else {
	    if (item_description(mitems[selected]) && 
		    strcmp(item_description(mitems[selected]), UNKNOWN) == 0)
		tmp = NULL;
	    else
		tmp = data[selected]->value;

	    tmp = get_input(buf, tmp, 0, 0, NULL, NULL, NULL, 0, -1);
	}

	len = (tmp) ? strlen(tmp) + 1 : 1;
	data[selected]->value = Realloc(data[selected]->value, len);
	strncpy(data[selected]->value, (tmp) ? tmp : "", len);

cleanup:
	cleanup(win, subw, panel, menu, mitems, NULL);
    }

done:
    if (!edit) {
	pgn_tag_free(data);
	return NULL;
    }

    return data;
}

/* If the saveindex argument is -1, all games will be saved. Otherwise it's a
 * game index number.
 */
int save_pgn(const char *filename, int isfifo, int saveindex)
{
    FILE *fp;
    char *mode = NULL;
    int c;
    char buf[FILENAME_MAX];
    struct stat st;
    int i;
    char *command = NULL;
    int saveindex_max = (saveindex == -1) ? gtotal : saveindex + 1;

    if (filename[0] != '/' && config.savedirectory && !isfifo) {
	if (stat(config.savedirectory, &st) == -1) {
	    if (errno == ENOENT) {
		if (mkdir(config.savedirectory, 0755) == -1) {
		    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			    strerror(errno));
		    return 1;
		}
	    }
	    else {
		cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory,
			strerror(errno));
		return 1;
	    }
	}

	stat(config.savedirectory, &st);

	if (!S_ISDIR(st.st_mode)) {
	    cmessage(ERROR, ANYKEY, "%s: %s", config.savedirectory, E_NOTADIR);
	    return 1;
	}

	snprintf(buf, sizeof(buf), "%s/%s", config.savedirectory, filename);
	filename = buf;
    }

    if (!isfifo)
	command = compression_cmd(filename, 0);

    /* This is a hack to resume an existing game when more than one game is
     * available. Also resuming a saved game and a game from history.
     */
    // FIXME: may not need this when a FEN tag is supported (by the engine).
    if (isfifo)
	mode = "w";
    else {
	if (access(filename, W_OK) == 0) {
	    c = cmessage(NULL, GAME_SAVE_OVERWRITE_PROMPT,
		    "%s \"%s\"", E_FILEEXISTS, filename);

	    switch (c) {
		case 'a':
		    if (command) {
			cmessage(NULL, ANYKEY, "%s", E_SAVE_COMPRESS);
			return 1;
		    }

		    mode = "a";
		    break;
		case 'o':
		    mode = "w+";
		    break;
		default:
		    return 1;
	    }
	}
	else
	    mode = "a";
    }

    if (command) {
	if ((fp = popen(command, "w")) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	    return 1;
	}
    }
    else {
	if ((fp = fopen(filename, mode)) == NULL) {
	    cmessage(ERROR, ANYKEY, "%s: %s", filename, strerror(errno));
	    return 1;
	}
    }

    if (isfifo)
	pgn_write(fp, game[saveindex], config.mpl, isfifo);
    else {
	for (i = (saveindex == -1) ? 0 : saveindex; i < saveindex_max; i++)
	    pgn_write(fp, game[i], config.mpl, isfifo);
    }

    if (command)
	pclose(fp);
    else
	fclose(fp);

    if (!isfifo && saveindex == -1)
	strncpy(loadfile, filename, sizeof(loadfile));

    return 0;
}

char *random_agony(GAME g)
{
    static int n;
    FILE *fp;
    char line[LINE_MAX];

    if (n == -1 || !config.agony || !curses_initialized ||
	    (g.mode == MODE_HISTORY && !config.historyagony))
	return NULL;

    if (!agony) {
	if ((fp = fopen(config.agonyfile, "r")) == NULL) {
	    n = -1;
	    cmessage(ERROR, ANYKEY, "%s: %s", config.agonyfile, strerror(errno));
	    return NULL;
	}

	while (!feof(fp)) {
	    if (fscanf(fp, " %[^\n] ", line) == 1) {
		agony = Realloc(agony, (n + 2) * sizeof(char *));
		agony[n++] = strdup(trim(line));
	    }
	}

	agony[n] = NULL;
	fclose(fp);

	if (agony[0] == NULL || !n) {
	    n = -1;
	    return NULL;
	}
    }

    return agony[random() % n];
}

static int castling_state(GAME *g, BOARD b, int row, int col, int piece, int mod)
{
    if (pgn_piece_to_int(piece) == ROOK && col == 7
	    && row == 7 &&
	    (TEST_FLAG(g->flags, GF_WK_CASTLE) || mod) &&
	    pgn_piece_to_int(b[7][4].icon) == KING && isupper(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_WK_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 0
	    && row == 7 &&
	    (TEST_FLAG(g->flags, GF_WQ_CASTLE) || mod) &&
	    pgn_piece_to_int(b[7][4].icon) == KING && isupper(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_WQ_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 7
	    && row == 0 &&
	    (TEST_FLAG(g->flags, GF_BK_CASTLE) || mod) &&
	    pgn_piece_to_int(b[0][4].icon) == KING && islower(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_BK_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == ROOK && col == 0
	    && row == 0 &&
	    (TEST_FLAG(g->flags, GF_BQ_CASTLE) || mod) &&
	    pgn_piece_to_int(b[0][4].icon) == KING && islower(piece)) {
	if (mod)
	    TOGGLE_FLAG(g->flags, GF_BQ_CASTLE);
	return 1;
    }
    else if (pgn_piece_to_int(piece) == KING && col == 4
	    && row == 7 && 
	    (mod || (pgn_piece_to_int(b[7][7].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_WK_CASTLE))
	      ||
	     (pgn_piece_to_int(b[7][0].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_WQ_CASTLE))) && isupper(piece)) {
	if (mod) {
	    if (TEST_FLAG(g->flags, GF_WK_CASTLE) ||
		    TEST_FLAG(g->flags, GF_WQ_CASTLE))
		CLEAR_FLAG(g->flags, GF_WK_CASTLE|GF_WQ_CASTLE);
	    else
		SET_FLAG(g->flags, GF_WK_CASTLE|GF_WQ_CASTLE);
	}
	return 1;
    }
    else if (pgn_piece_to_int(piece) == KING && col == 4
	    && row == 0 &&
	    (mod || (pgn_piece_to_int(b[0][7].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_BK_CASTLE))
	      ||
	     (pgn_piece_to_int(b[0][0].icon) == ROOK &&
	      TEST_FLAG(g->flags, GF_BQ_CASTLE))) && islower(piece)) {
	if (mod) {
	    if (TEST_FLAG(g->flags, GF_BK_CASTLE) ||
		    TEST_FLAG(g->flags, GF_BQ_CASTLE))
		CLEAR_FLAG(g->flags, GF_BK_CASTLE|GF_BQ_CASTLE);
	    else
		SET_FLAG(g->flags, GF_BK_CASTLE|GF_BQ_CASTLE);
	}
	return 1;
    }

    return 0;
}

static void draw_board(GAME *g, int details, int crow, int ccol)
{
    int row, col;
    int bcol = 0, brow = 0;
    int maxy = BOARD_HEIGHT, maxx = BOARD_WIDTH;
    int ncols = 0, offset = 1;
    unsigned coords_y = 8;

    for (row = 0; row < maxy; row++) {
	bcol = 0;

	for (col = 0; col < maxx; col++) {
	    int attrwhich = -1;
	    chtype attrs = 0;
	    unsigned char piece;

	    if (row == 0 || row == maxy - 2) {
		if (col == 0)
		    mvwaddch(boardw, row, col, 
			    LINE_GRAPHIC((row) ? 
				ACS_LLCORNER | CP_BOARD_GRAPHICS : 
				ACS_ULCORNER | CP_BOARD_GRAPHICS));
		else if (col == maxx - 2)
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC((row) ?
				ACS_LRCORNER | CP_BOARD_GRAPHICS : 
				ACS_URCORNER | CP_BOARD_GRAPHICS));
		else if (!(col % 4))
		    mvwaddch(boardw, row, col, 
			    LINE_GRAPHIC((row) ? 
				ACS_BTEE | CP_BOARD_GRAPHICS : 
				ACS_TTEE | CP_BOARD_GRAPHICS));
		else {
		    if (col != maxx - 1)
			mvwaddch(boardw, row, col,
				LINE_GRAPHIC(ACS_HLINE | CP_BOARD_GRAPHICS));
		}

		continue;
	    }

	    if ((row % 2) && col == maxx - 1 && coords_y) {
		wattron(boardw, CP_BOARD_COORDS);
		mvwprintw(boardw, row, col, "%d", coords_y--);
		wattroff(boardw, CP_BOARD_COORDS);
		continue;
	    }

	    if ((col == 0 || col == maxx - 2) && row != maxy - 1) {
		if (!(row % 2))
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC((col) ?
				ACS_RTEE | CP_BOARD_GRAPHICS : 
				ACS_LTEE | CP_BOARD_GRAPHICS));
		else
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC(ACS_VLINE | CP_BOARD_GRAPHICS));

		continue;
	    }

	    if ((row % 2) && !(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col,
			LINE_GRAPHIC(ACS_VLINE | CP_BOARD_GRAPHICS));
		continue;
	    }

	    if (!(col % 4) && row != maxy - 1) {
		mvwaddch(boardw, row, col,
			LINE_GRAPHIC(ACS_PLUS | CP_BOARD_GRAPHICS));
		continue;
	    }

	    if ((row % 2)) {
		if ((col % 4)) {
		    if (ncols++ == 8) {
			offset++;
			ncols = 1;
		    }

		    if (((ncols % 2) && !(offset % 2)) || (!(ncols % 2) 
				&& (offset % 2)))
			attrwhich = BLACK;
		    else
			attrwhich = WHITE;

		    if (config.validmoves && g->b[brow][bcol].valid) {
			attrs = (attrwhich == WHITE) ? CP_BOARD_MOVES_WHITE :
			    CP_BOARD_MOVES_BLACK;
		    }
		    else
			attrs = (attrwhich == WHITE) ? CP_BOARD_WHITE :
			    CP_BOARD_BLACK;

		    if (row == ROWTOMATRIX(crow) && col == COLTOMATRIX(ccol)) {
			attrs = CP_BOARD_CURSOR;
		    }

		    if (row == ROWTOMATRIX(sp.row) && 
			    col == COLTOMATRIX(sp.col)) {
			attrs = CP_BOARD_SELECTED;
		    }

		    if (row == maxy - 1)
			attrs = 0;

		    mvwaddch(boardw, row, col, ' ' | attrs);

		    if (row == maxy - 1)
			waddch(boardw, x_grid_chars[bcol] | CP_BOARD_COORDS);
		    else {
			if (details && g->b[row / 2][bcol].enpassant)
			    piece = 'x';
			else
			    piece = g->b[row / 2][bcol].icon;

			if (details && castling_state(g, g->b, brow, bcol,
				    piece, 0))
			    attrs |= A_REVERSE;

			if (g->side == WHITE && isupper(piece))
			    attrs |= A_BOLD;
			else if (g->side == BLACK && islower(piece))
			    attrs |= A_BOLD;

			waddch(boardw, (pgn_piece_to_int(piece) != OPEN_SQUARE) ? piece | attrs : ' ' | attrs);

			CLEAR_FLAG(attrs, A_BOLD);
			CLEAR_FLAG(attrs, A_REVERSE);
		    }

		    waddch(boardw, ' ' | attrs);
		    col += 2;
		    bcol++;
		}
	    }
	    else {
		if (col != maxx - 1)
		    mvwaddch(boardw, row, col,
			    LINE_GRAPHIC(ACS_HLINE | CP_BOARD_GRAPHICS));
	    }
	}

	brow = row / 2;
    }
}

void invalid_move(int n, const char *m)
{
    if (curses_initialized)
	cmessage(ERROR, ANYKEY, "%s \"%s\" (round #%i)", E_INVALID_MOVE, m, n);
    else
	warnx("%s: %s \"%s\" (round #%i)", loadfile, E_INVALID_MOVE, m, n);
}

/* Convert the selected piece to SAN format and validate it. */
static char *board_to_san(GAME *g, BOARD b)
{
    static char str[MAX_SAN_MOVE_LEN + 1], *p;
    int piece;
    int promo;
    BOARD oldboard;

    snprintf(str, sizeof(str), "%c%i%c%i", x_grid_chars[sp.col - 1], 
	    sp.row, x_grid_chars[sp.destcol - 1], sp.destrow);

    p = str;
    piece = pgn_piece_to_int(b[ROWTOBOARD(sp.row)][COLTOBOARD(sp.col)].icon);

    if (piece == PAWN && ((sp.destrow == 8 && g->turn == WHITE) ||
		    (sp.destrow == 1 && g->turn == BLACK))) {
	promo = cmessage(PROMOTION_TITLE, PROMOTION_PROMPT, PROMOTION_TEXT);
	
	if (pgn_piece_to_int(promo) == -1)
	    return NULL;

	p = str + strlen(str);
	*p++ = toupper(promo);
	*p = '\0';
    }

    memcpy(oldboard, b, sizeof(BOARD));

    if ((p = pgn_a2a4tosan(g, b, str)) == NULL) {
	cmessage(p, ANYKEY, "%s", E_A2A4_PARSE);
	memcpy(b, oldboard, sizeof(BOARD));
	return NULL;
    }

    if (pgn_validate_move(g, b, p)) {
	invalid_move(gindex + 1, p);
	memcpy(b, oldboard, sizeof(BOARD));
	return NULL;
    }

    return p;
}

static int move_to_engine(GAME *g, BOARD b)
{
    char *p;

    if ((p = board_to_san(g, b)) == NULL)
	return 0;

    sp.row = sp.col = sp.icon = 0;

    if (noengine) {
	history_add(g, p);
	pgn_switch_turn(g);
	SET_FLAG(g->flags, GF_MODIFIED);
	update_all(*g);
	return 1;
    }

    SEND_TO_ENGINE("%s\n", p);
    return 1;
}

static void update_clock(int n, int *h, int *m, int *s)
{
    *h = n / 3600;
    *m = (n % 3600) / 60;
    *s = (n % 3600) % 60;

    return;
}

void update_status_window(GAME g)
{
    int i = 0;
    char *buf;
    char tmp[15], *engine, *mode;
    int w;
    int h, m, s;
    char *p;
    int maxy, maxx;
    int len;

    getmaxyx(statusw, maxy, maxx);
    w = maxx - 2 - 8;
    len = maxx - 2;
    buf = Malloc(len);

    *tmp = '\0';
    p = tmp;

    if (TEST_FLAG(g.flags, GF_DELETE)) {
	*p++ = '(';
	*p++ = 'x';
	i++;
    }

    if (TEST_FLAG(g.flags, GF_PERROR)) {
	if (!i)
	    *p++ = '(';
	else
	    *p++ = '/';

	*p++ = '!';
	i++;
    }

    if (TEST_FLAG(g.flags, GF_MODIFIED)) {
	if (!i)
	    *p++ = '(';
	else
	    *p++ = '/';

	*p++ = '*';
	i++;
    }

    if (*tmp != '\0')
	*p++ = ')';

    *p = '\0';

    mvwprintw(statusw, 2, 1, "%*s %-*s", 7, STATUS_FILE_STR, w,
	    (loadfile[0]) ? str_etc(loadfile, w, 1) : UNAVAILABLE);
    snprintf(buf, len, "%i %s %i %s", gindex + 1, N_OF_N_STR, gtotal, 
	    (*tmp) ? tmp : "");
    mvwprintw(statusw, 3, 1, "%*s %-*s", 7, STATUS_GAME_STR, w, buf);

    switch (g.mode) {
	case MODE_HISTORY:
	    mode = MODE_HISTORY_STR;
	    break;
	case MODE_EDIT:
	    mode = MODE_EDIT_STR;
	    break;
	case MODE_PLAY:
	    mode = MODE_PLAY_STR;
	    break;
	default:
	    mode = UNKNOWN;
	    break;
    }

    mvwprintw(statusw, 4, 1, "%*s %-*s", 7, STATUS_MODE_STR, w, mode);

    switch (status.engine) {
	case ENGINE_THINKING:
	    engine = ENGINE_THINKING_STR;
	    break;
	case ENGINE_READY:
	    engine = ENGINE_READY_STR;
	    break;
	case ENGINE_INITIALIZING:
	    engine = ENGINE_INITIALIZING_STR;
	    break;
	case ENGINE_OFFLINE:
	    engine = ENGINE_OFFLINE_STR;
	    break;
	default:
	    engine = UNKNOWN;
	    break;
    }

    mvwprintw(statusw, 5, 1, "%*s %-*s", 7, STATUS_ENGINE_STR, w, " ");
    wattron(statusw, CP_STATUS_ENGINE);
    mvwaddstr(statusw, 5, 9, engine);
    wattroff(statusw, CP_STATUS_ENGINE);

    mvwprintw(statusw, 6, 1, "%*s %-*s", 7, STATUS_TURN_STR, w,
	    (g.turn == WHITE) ? WHITE_STR : BLACK_STR);

    strncpy(tmp, WHITE_STR, sizeof(tmp));
    tmp[0] = toupper(tmp[0]);
    update_clock(g.moveclock, &h, &m, &s);
    snprintf(buf, len, "%.2i:%.2i:%.2i", h, m, s);
    mvwprintw(statusw, 7, 1, "%*s: %-*s", 6, tmp, w, buf);

    strncpy(tmp, BLACK_STR, sizeof(tmp));
    tmp[0] = toupper(tmp[0]);
    update_clock(g.moveclock, &h, &m, &s);
    snprintf(buf, len, "%.2i:%.2i:%.2i", h, m, s);
    mvwprintw(statusw, 8, 1, "%*s: %-*s", 6, tmp, w, buf);
    free(buf);

    for (i = 1; i < maxx - 4; i++)
	mvwprintw(statusw, maxy - 2, i, " ");

    if (!status.notify)
	status.notify = strdup(GAME_HELP_PROMPT);

    wattron(statusw, CP_STATUS_NOTIFY);
    mvwprintw(statusw, maxy - 2, CENTERX(maxx, status.notify), "%s",
	    status.notify);
    wattroff(statusw, CP_STATUS_NOTIFY);
}

void update_history_window(GAME g)
{
    char buf[HISTORY_WIDTH - 1];
    HISTORY *h = NULL;
    int n, total;
    int t = history_total(g.hp);

    n = (g.hindex + 1) / 2;

    if (t % 2)
	total = (t + 1) / 2;
    else
	total = t / 2;

    if (t)
	snprintf(buf, sizeof(buf), "%u %s %u%s", n, N_OF_N_STR, total,
		(movestep == 1) ? HISTORY_PLY_STEP : "");
    else
	strncpy(buf, UNAVAILABLE, sizeof(buf));

    mvwprintw(historyw, 2, 1, "%*s %-*s", 10, HISTORY_MOVE_STR,
	    HISTORY_WIDTH - 13, buf);

    h = history_by_n(g.hp, g.hindex);
    snprintf(buf, sizeof(buf), "%s", (h && h->move) ? h->move : UNAVAILABLE);
    n = 0;

    if (h && ((h->comment) || h->nag[0])) {
	strncat(buf, " (v", sizeof(buf));
	n++;
    }

    if (h && h->rav) {
	strncat(buf, (n) ? ",+" : " (+", sizeof(buf));
	n++;
    }

    if (g.ravlevel) {
	strncat(buf, (n) ? ",-" : " (-", sizeof(buf));
	n++;
    }

    if (n)
	strncat(buf, ")", sizeof(buf));

    mvwprintw(historyw, 3, 1, "%s %-*s", HISTORY_MOVE_NEXT_STR,
	    HISTORY_WIDTH - 13, buf);

    h = history_by_n(g.hp, game[gindex].hindex - 1);
    snprintf(buf, sizeof(buf), "%s", (h && h->move) ? h->move : UNAVAILABLE);
    n = 0;

    if (h && ((h->comment) || h->nag[0])) {
	strncat(buf, " (V", sizeof(buf));
	n++;
    }

    if (h && h->rav) {
	strncat(buf, (n) ? ",+" : " (+", sizeof(buf));
	n++;
    }

    if (g.ravlevel) {
	strncat(buf, (n) ? ",-" : " (-", sizeof(buf));
	n++;
    }

    if (n)
	strncat(buf, ")", sizeof(buf));

    mvwprintw(historyw, 4, 1, "%s %-*s", HISTORY_MOVE_PREV_STR,
	    HISTORY_WIDTH - 13, buf);
}

void update_tag_window(TAG **t)
{
    int i;
    int w = TAG_WIDTH - 10;

    for (i = 0; i < 7; i++)
	mvwprintw(tagw, (i + 2), 1, "%*s: %-*s", 6, t[i]->name, w, t[i]->value);
}

void draw_prompt(WINDOW *win, int y, int width, const char *str, chtype attr)
{
    int i;

    wattron(win, attr);

    for (i = 1; i < width - 1; i++)
	mvwaddch(win, y, i, ' ');

    mvwprintw(win, y, CENTERX(width, str), "%s", str);
    wattroff(win, attr);
}

void draw_window_title(WINDOW *win, const char *title, int width, chtype attr,
	chtype battr)
{
    int i;

    if (title) {
	wattron(win, attr);

	for (i = 1; i < width - 1; i++)
	    mvwaddch(win, 1, i, ' ');

	mvwprintw(win, 1, CENTERX(width, title), "%s", title);
	wattroff(win, attr);
    }

    wattron(win, battr);
    box(win, ACS_VLINE, ACS_HLINE);
    wattroff(win, battr);
}

void update_all(GAME g)
{
    update_status_window(g);
    update_history_window(g);
    update_tag_window(g.tag);
}

static void game_next_prev(GAME g, int n, int count)
{
    if (gtotal < 2)
	return;

    if (n == 1) {
	if (gindex + count > gtotal - 1) {
	    if (count != 1)
		gindex = gtotal - 1;
	    else
		gindex = 0;
	}
	else
	    gindex += count;
    }
    else {
	if (gindex - count < 0) {
	    if (count != 1)
		gindex = 0;
	    else
		gindex = gtotal - 1;
	}
	else
	    gindex -= count;
    }
}

static void delete_game(int which)
{
    GAME *g = NULL;
    int gi = 0;
    int i;

    for (i = 0; i < gtotal; i++) {
	if (i == which || TEST_FLAG(game[i].flags, GF_DELETE)) {
	    pgn_free(game[i]);
	    continue;
	}

	g = Realloc(g, (gi + 1) * sizeof(GAME));
	memcpy(&g[gi], &game[i], sizeof(GAME));
	g[gi].tag = game[i].tag;
	g[gi].history = game[i].history;
	g[gi].hp = game[i].hp;
	gi++;
    }

    game = g;
    gtotal = gi;

    if (which != -1) {
	if (which + 1 >= gtotal)
	    gindex = gtotal - 1;
	else
	    gindex = which;
    }
    else
	gindex = gtotal - 1;

    game[gindex].hp = game[gindex].history;
}

static int find_move_exp(GAME g, const char *str, int init, int which,
	int count)
{
    int i;
    int ret;
    static regex_t r;
    static int firstrun = 1;
    char errbuf[255];
    int incr;
    int found;

    if (init) {
	if (!firstrun)
	    regfree(&r);

	if ((ret = regcomp(&r, str, REG_EXTENDED|REG_NOSUB)) != 0) {
	    regerror(ret, &r, errbuf, sizeof(errbuf));
	    cmessage(E_REGCOMP_TITLE, ANYKEY, "%s", errbuf);
	    return -1;
	}

	firstrun = 1;
    }

    incr = (which == 0) ? -1 : 1;

    for (i = g.hindex + incr - 1, found = 0; ; i += incr) {
	if (i == g.hindex - 1)
	    break;

	if (i >= history_total(g.hp))
	    i = 0;
	else if (i < 0)
	    i = history_total(g.hp) - 1;

	// FIXME RAV
	ret = regexec(&r, g.hp[i]->move, 0, 0, 0);

	if (ret == 0) {
	    if (count == ++found) {
		return i + 1;
	    }
	}
	else {
	    if (ret != REG_NOMATCH) {
		regerror(ret, &r, errbuf, sizeof(errbuf));
		cmessage(E_REGEXEC_TITLE, ANYKEY, "%s", errbuf);
		return -1;
	    }
	}
    }

    return -1;
}

static int toggle_delete_flag(int n)
{
    int i, x;

    TOGGLE_FLAG(game[n].flags, GF_DELETE);

    for (i = x = 0; i < gtotal; i++) {
	if (TEST_FLAG(game[i].flags, GF_DELETE))
	    x++;
    }

    if (x == gtotal) {
	cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
	CLEAR_FLAG(game[n].flags, GF_DELETE);
	return 1;
    }

    return 0;
}

static void edit_save_tags(GAME *g)
{
    TAG **t;

    if ((t = edit_tags(*g, g->b, 1)) == NULL)
	return;

    pgn_tag_free(g->tag);
    g->tag = t;
    SET_FLAG(g->flags, GF_MODIFIED);
    pgn_sort_tags(g->tag);
}

static int find_game_exp(char *str, int which, int count)
{
    char *nstr = NULL, *exp = NULL;
    regex_t nexp, vexp;
    int ret = -1;
    int g = 0;
    char buf[255], *tmp;
    char errbuf[255];
    int found = 0;
    int incr = (which == 0) ? -(1) : 1;

    strncpy(buf, str, sizeof(buf));
    tmp = buf;

    if (strstr(tmp, ":") != NULL) {
	nstr = strsep(&tmp, ":");

	if ((ret = regcomp(&nexp, nstr,
			REG_ICASE|REG_EXTENDED|REG_NOSUB)) != 0) {
	    regerror(ret, &nexp, errbuf, sizeof(errbuf));
	    cmessage(E_REGCOMP_TITLE, ANYKEY, "%s", errbuf);
	    ret = g = -1;
	    goto cleanup;
	}
    }

    exp = tmp;

    if (exp == NULL)
	goto cleanup;

    if ((ret = regcomp(&vexp, exp, REG_EXTENDED|REG_NOSUB)) != 0) {
	regerror(ret, &vexp, errbuf, sizeof(errbuf));
	cmessage(E_REGCOMP_TITLE, ANYKEY, "%s", errbuf);
	ret = -1;
	goto cleanup;
    }

    ret = -1;

    for (g = gindex + incr, found = 0; ; g += incr) {
	int t;

	if (g == gindex)
	    break;

	if (g == gtotal)
	    g = 0;
	else if (g < 0)
	    g = gtotal - 1;

	for (t = 0; game[g].tag[t]; t++) {
	    if (nstr) {
		if (regexec(&nexp, game[g].tag[t]->name, 0, 0, 0) == 0) {
		    if (regexec(&vexp, game[g].tag[t]->value, 0, 0, 0) == 0) {
			if (count == ++found) {
			    ret = g;
			    goto cleanup;
			}
		    }
		}
	    }
	    else {
		if (regexec(&vexp, game[g].tag[t]->value, 0, 0, 0) == 0) {
		    if (count == ++found) {
			ret = g;
			goto cleanup;
		    }
		}
	    }
	}

	ret = -1;
    }

cleanup:
    if (nstr)
	regfree(&nexp);

    if (g != -1)
	regfree(&vexp);

    return ret;
}

/*
 * Updates the notification line in the status window then refreshes the
 * status window.
 */
void update_status_notify(GAME g, char *fmt, ...)
{
    va_list ap;
#ifdef HAVE_VASPRINTF
    char *line;
#else
    char line[COLS];
#endif

    if (!fmt) {
	if (status.notify) {
	    free(status.notify);
	    status.notify = NULL;

	    if (curses_initialized)
		update_status_window(g);
	}

	return;
    }

    va_start(ap, fmt);
#ifdef HAVE_VASPRINTF
    vasprintf(&line, fmt, ap);
#else
    vsnprintf(line, sizeof(line), fmt, ap);
#endif
    va_end(ap);

    if (status.notify)
	free(status.notify);

    status.notify = strdup(line);

#ifdef HAVE_VASPRINTF
    free(line);
#endif
    if (curses_initialized)
	update_status_window(g);
}

static void switch_side(GAME *g)
{
    g->side = (g->side == WHITE) ? BLACK : WHITE;
}

int rav_next_prev(GAME *g, BOARD b, int n)
{
    // Next RAV.
    if (n) {
	if (g->hp[g->hindex]->rav == NULL)
	    return 1;

	g->rav = Realloc(g->rav, (g->ravlevel + 1) * sizeof(RAV));
	g->rav[g->ravlevel].hp = g->hp;
	g->rav[g->ravlevel].flags = g->flags;
	g->rav[g->ravlevel].fen = strdup(pgn_game_to_fen(*g, b));
	g->rav[g->ravlevel].hindex = g->hindex;
	g->hp = g->hp[g->hindex]->rav;
	g->hindex = 0;
	g->ravlevel++;
	return 0;
    }

    if (g->ravlevel - 1 < 0)
	return 1;

    // Previous RAV.
    g->ravlevel--;
    pgn_init_fen_board(g, b, g->rav[g->ravlevel].fen);
    free(g->rav[g->ravlevel].fen);
    g->hp = g->rav[g->ravlevel].hp;
    g->flags = g->rav[g->ravlevel].flags;
    g->hindex = g->rav[g->ravlevel].hindex;
    return 0;
}

static void draw_window_decor()
{
    move_panel(historyp, LINES - HISTORY_HEIGHT, COLS - HISTORY_WIDTH);
    move_panel(boardp, 0, COLS - BOARD_WIDTH);
    wbkgd(boardw, CP_BOARD_WINDOW);
    wbkgd(statusw, CP_STATUS_WINDOW);
    draw_window_title(statusw, STATUS_WINDOW_TITLE, STATUS_WIDTH,
	    CP_STATUS_TITLE, CP_STATUS_BORDER);
    wbkgd(tagw, CP_TAG_WINDOW);
    draw_window_title(tagw, TAG_WINDOW_TITLE, TAG_WIDTH, CP_TAG_TITLE, 
	    CP_TAG_BORDER);
    wbkgd(historyw, CP_HISTORY_WINDOW);
    draw_window_title(historyw, HISTORY_WINDOW_TITLE, HISTORY_WIDTH,
	    CP_HISTORY_TITLE, CP_HISTORY_BORDER);
}

static void do_window_resize()
{
    if (LINES < 24 || COLS < 80)
	return;

    resizeterm(LINES, COLS);
    wresize(historyw, HISTORY_HEIGHT, HISTORY_WIDTH);
    wresize(statusw, STATUS_HEIGHT, STATUS_WIDTH);
    wresize(tagw, TAG_HEIGHT, TAG_WIDTH);
    wmove(historyw, 0, 0);
    wclrtobot(historyw);
    wmove(tagw, 0, 0);
    wclrtobot(tagw);
    wmove(statusw, 0, 0);
    wclrtobot(statusw);
    draw_window_decor();
    update_all(game[gindex]);
}

static void historymode_keys(int);
static void playmode_keys(int c)
{
    // More keys in MODE_EDIT share keys with MODE_PLAY than don't.
    int editmode = (game[gindex].mode == MODE_EDIT) ? 1 : 0;
    chtype p;
    int w, x, y, z;
    char *tmp;

    switch (c) {
	case 'c':
	    if (status.engine == ENGINE_THINKING || status.engine ==
		    ENGINE_OFFLINE)
		break;

	    if ((tmp = get_input_str_clear(ENGINE_CMD_TITLE, NULL)) != NULL)
		SEND_TO_ENGINE("%s\n", tmp);
	    break;
	case '\015':
	case '\n':
	    pushkey = keycount = 0;
	    update_status_notify(game[gindex], NULL);

	    if (status.engine == ENGINE_THINKING) {
		beep();
		break;
	    }

	    if (!sp.icon)
		break;

	    sp.destrow = c_row;
	    sp.destcol = c_col;

	    if (editmode) {
		p = game[gindex].b[ROWTOBOARD(sp.row)][COLTOBOARD(sp.col)].icon;
		game[gindex].b[ROWTOBOARD(sp.destrow)][COLTOBOARD(sp.destcol)].icon = p;
		game[gindex].b[ROWTOBOARD(sp.row)][COLTOBOARD(sp.col)].icon = pgn_int_to_piece(game[gindex].turn, OPEN_SQUARE);
		sp.icon = sp.row = sp.col = 0;
		break;
	    }

	    if (move_to_engine(&game[gindex], game[gindex].b)) {
		if (config.validmoves)
		    board_reset_valid_moves(game[gindex].b);

		if (TEST_FLAG(game[gindex].flags, GF_GAMEOVER)) {
		    CLEAR_FLAG(game[gindex].flags, GF_GAMEOVER);
		    SET_FLAG(game[gindex].flags, GF_MODIFIED);
		}
	    }

	    break;
	case ' ':
	    if (!noengine && (status.engine == ENGINE_OFFLINE ||
			!engine_initialized) && !editmode) {
		if (start_chess_engine() < 0) {
		    sp.icon = 0;
		    break;
		}

	    }

	    if (!editmode)
		wtimeout(boardw, 70);

	    if (sp.icon || (!editmode && status.engine == ENGINE_THINKING)) {
		beep();
		break;
	    }

	    sp.icon = mvwinch(boardw, ROWTOMATRIX(c_row), 
		    COLTOMATRIX(c_col)+1) & A_CHARTEXT;

	    if (sp.icon == ' ') {
		sp.icon = 0;
		break;
	    }

	    if (!editmode && ((islower(sp.icon) && game[gindex].turn != BLACK)
			|| (isupper(sp.icon) && game[gindex].turn != WHITE))) {
		message(NULL, ANYKEY, "%s", E_SELECT_TURN);
		sp.icon = 0;
		break;
	    }

	    sp.row = c_row;
	    sp.col = c_col;

	    if (!editmode && config.validmoves)
		board_get_valid_moves(&game[gindex], game[gindex].b,
			pgn_piece_to_int(sp.icon), sp.row, sp.col, &w, &x, &y, &z);

	    paused = 0;
	    break;
	case 'w':
	    SEND_TO_ENGINE("\nswitch\n");
	    switch_side(&game[gindex]);
	    update_status_window(game[gindex]);
	    break;
	case 'u':
	    /* FIXME dies reading FIFO sometimes. */
	    if (!history_total(game[gindex].hp))
		break;

	    history_previous(&game[gindex], game[gindex].b, (keycount) ? keycount * 2 :
		    2);

#if 0
	    if (status.engine == CRAFTY)
		SEND_TO_ENGINE("read %s\n", config.fifo);
	    else
		SEND_TO_ENGINE("\npgnload %s\n", config.fifo);
#endif

	    update_history_window(game[gindex]);
	    break;
	case 'a':
	    historymode_keys(c);
	    break;
	case 'd':
	    board_details = (board_details) ? 0 : 1;
	    break;
	case 'p':
	    paused = (paused) ? 0 : 1;
	    break;
	default:
	    break;
    }
}

static void editmode_keys(int c)
{
    switch (c) {
	case '\015':
	case '\n':
	case ' ':
	    playmode_keys(c);
	    break;
	case 'd':
	    if (sp.icon)
		game[gindex].b[ROWTOBOARD(sp.row)][COLTOBOARD(sp.col)].icon = pgn_int_to_piece(game[gindex].turn, OPEN_SQUARE);
	    else
		game[gindex].b[ROWTOBOARD(c_row)][COLTOBOARD(c_col)].icon = pgn_int_to_piece(game[gindex].turn, OPEN_SQUARE);

	    sp.icon = sp.row = sp.col = 0;
	    break;
	case 'w':
	    pgn_switch_turn(&game[gindex]);
	    switch_side(&game[gindex]);
	    update_all(game[gindex]);
	    break;
	case 'c':
	    castling_state(&game[gindex], game[gindex].b, ROWTOBOARD(c_row), 
		    COLTOBOARD(c_col),
		    game[gindex].b[ROWTOBOARD(c_row)][COLTOBOARD(c_col)].icon, 1);
	    break;
	case 'i':
	    c = message(GAME_EDIT_TITLE, GAME_EDIT_PROMPT, "%s",
		    GAME_EDIT_TEXT);

	    if (pgn_piece_to_int(c) == -1)
		break;

	    game[gindex].b[ROWTOBOARD(c_row)][COLTOBOARD(c_col)].icon = c;
	    break;
	case 'p':
	    if (c_row == 6 || c_row == 3) {
		pgn_reset_enpassant(game[gindex].b);
		game[gindex].b[ROWTOBOARD(c_row)][COLTOBOARD(c_col)].enpassant = 1;
	    }
	    break;
	default:
	    break;
    }
}

static void historymode_keys(int c)
{
    int n, len;
    char *tmp, *buf;
    static char moveexp[255] = {0};

    switch (c) {
	case ' ':
	    movestep = (movestep == 1) ? 2 : 1;
	    update_history_window(game[gindex]);
	    break;
	case KEY_UP:
	    history_next(&game[gindex], game[gindex].b, (keycount > 0) ?
		    config.jumpcount * keycount * movestep : 
		    config.jumpcount * movestep);
	    update_cursor(game[gindex], game[gindex].hindex, &c_row, &c_col);
	    update_all(game[gindex]);
	    break;
	case KEY_DOWN:
	    history_previous(&game[gindex], game[gindex].b, (keycount) ?
		    config.jumpcount * keycount * movestep : 
		    config.jumpcount * movestep);
	    update_cursor(game[gindex], game[gindex].hindex, &c_row, &c_col);
	    update_all(game[gindex]);
	    break;
	case KEY_LEFT:
	    history_previous(&game[gindex], game[gindex].b, (keycount) ?
		    keycount * movestep : movestep);
	    update_cursor(game[gindex], game[gindex].hindex, &c_row, &c_col);
	    update_all(game[gindex]);
	    break;
	case KEY_RIGHT:
	    history_next(&game[gindex], game[gindex].b, (keycount) ? 
		    keycount * movestep : movestep);
	    update_cursor(game[gindex], game[gindex].hindex, &c_row, &c_col);
	    update_all(game[gindex]);
	    break;
	case 'a':
	    n = game[gindex].hindex;

	    if (n && game[gindex].hp[n - 1]->move)
		n--;
	    else
		break;

	    buf = Malloc(COLS);
	    snprintf(buf, COLS - 1, "%s \"%s\"", ANNOTATION_EDIT_TITLE,
		    game[gindex].hp[n]->move);

	    tmp = get_input(buf, game[gindex].hp[n]->comment, 0, 0, NAG_PROMPT,
		    history_edit_nag, (void *)game[gindex].hp[n], CTRL('T'),
		    -1);
	    free(buf);

	    if (!tmp && (!game[gindex].hp[n]->comment ||
			!*game[gindex].hp[n]->comment))
		break;
	    else if (tmp && game[gindex].hp[n]->comment) {
		if (strcmp(tmp, game[gindex].hp[n]->comment) == 0)
		    break;
	    }

	    len = (tmp) ? strlen(tmp) + 1 : 1;
	    game[gindex].hp[n]->comment = Realloc(game[gindex].hp[n]->comment,
		    len);
	    strncpy(game[gindex].hp[n]->comment, (tmp) ? tmp : "", len);
	    SET_FLAG(game[gindex].flags, GF_MODIFIED);
	    update_all(game[gindex]);
	    break;
	case ']':
	case '[':
	case '/':
	    if (history_total(game[gindex].hp) < 2)
		break;

	    n = 0;

	    if (!*moveexp || c == '/') {
		if ((tmp = get_input(FIND_REGEXP, moveexp, 1, 1, NULL, NULL, NULL, 0, -1)) == NULL)
		    break;

		strncpy(moveexp, tmp, sizeof(moveexp));
		n = 1;
	    }

	    if ((n = find_move_exp(game[gindex], moveexp, n, 
			    (c == '[') ? 0 : 1, (keycount) ? keycount : 1)) 
		    == -1)
		break;

	    game[gindex].hindex = n;
	    history_update_board(&game[gindex], game[gindex].b, game[gindex].hindex);
	    update_all(game[gindex]);
	    break;
	case 'v':
	    view_annotation(*game[gindex].hp[game[gindex].hindex]);
	    break;
	case 'V':
	    if (game[gindex].hindex - 1 >= 0)
		view_annotation(*game[gindex].hp[game[gindex].hindex - 1]);
	    break;
	case '-':
	case '+':
	    rav_next_prev(&game[gindex], game[gindex].b, (c == '-') ? 0 : 1);
	    update_all(game[gindex]);
	    break;
	case 'j':
	    if (history_total(game[gindex].hp) < 2)
		break;

	    /* FIXME field validation
	       if ((tmp = get_input(GAME_HISTORY_JUMP_TITLE, NULL, 1, 1, 
	       NULL, NULL, NULL, 0, FIELD_TYPE_INTEGER, 1, 0, 
	       game[gindex].htotal)) == NULL)
	       break;
	       */

	    if (!keycount) {
		if ((tmp = get_input(GAME_HISTORY_JUMP_TITLE, NULL, 1, 1, 
				NULL, NULL, NULL, 0, -1)) == NULL)
		    break;

		if (!isinteger(tmp))
		    break;

		n = atoi(tmp);
	    }
	    else
		n = keycount;

	    if (n > (history_total(game[gindex].hp) / 2) || n < 0)
		break;

	    game[gindex].hindex = n * 2;
	    history_update_board(&game[gindex], game[gindex].b, history_total(game[gindex].hp));
	    update_all(game[gindex]);
	    break;
	default: 
	    break;
    }
}

void game_loop()
{  
    int error_recover = 0;
    char gameexp[255] = {0};
    int delete_count = 0;
    int markstart = -1, markend = -1;

    c_row = 2, c_col = 5;
    gindex = gtotal - 1;

    if (history_total(game[gindex].hp))
	game[gindex].mode = MODE_HISTORY;
    else
	game[gindex].mode = MODE_PLAY;

    if (game[gindex].mode == MODE_HISTORY) {
	history_update_board(&game[gindex], game[gindex].b,
		history_total(game[gindex].hp));
	update_cursor(game[gindex], game[gindex].hindex, &c_row, &c_col);
    }

    update_status_notify(game[gindex], "%s", GAME_HELP_PROMPT);
    movestep = 2;
    paused = 1; //FIXME clock
    flushinp();
    update_all(game[gindex]);
    update_tag_window(game[gindex].tag);

    while (!quit) {
	int c = 0;
	int i, x, n = 0;
	/*
	fd_set fds;
	char fdbuf[8192] = {0};
	struct timeval tv;
	*/
	char *tmp = NULL;
	char tfile[FILENAME_MAX];

	// FIXME game.fds
#if 0
	if (engine_initialized) {
	    tv.tv_sec = 0;
	    tv.tv_usec = 0;

	    FD_ZERO(&fds);
	    FD_SET(enginefd[0], &fds);

	    for (i = 0; i < gtotal; i++) {
		if (game[i].sockfd > 0) {
		    if (game[i].sockfd > n)
			n = game[i].sockfd;

		    FD_SET(game[i].sockfd, &fds);
		}
	    }

	    n = (n > enginefd[0]) ? n : enginefd[0];

	    if ((n = select(n + 1, &fds, NULL, NULL, &tv)) > 0) {
		if (FD_ISSET(enginefd[0], &fds)) {
		    len = read(enginefd[0], fdbuf, sizeof(fdbuf));

		    if (len == -1) {
			if (errno != EAGAIN) {
			    cmessage(ERROR, ANYKEY, "Attempt #%i. read(): %s",
				    ++error_recover, strerror(errno));
			    continue;
			}
		    }
		    else {
			if (len) {
			    // FIXME engine may be associated with another
			    // selected game.
			    parse_engine_output(&game[gindex], fdbuf);
			    update_all(game[gindex]);
			}
		    }
		}

		for (i = 0; i < gtotal; i++) {
		    if (game[i].sockfd <= 0)
			continue;

		    if (FD_ISSET(game[i].sockfd, &fds)) {
			len = recv(game[i].sockfd, fdbuf, sizeof(fdbuf), 0);

			if (len == -1) {
			    if (errno != EAGAIN) {
				cmessage(ERROR, ANYKEY, 
					"Attempt #%i. recv(): %s", 
					++error_recover, strerror(errno));
				continue;
			    }
			}
			else {
			    if (len)
				parse_ics_output(fdbuf);

			    update_all(game[gindex]);
			}
		    }
		}
	    }
	    else {
		if (n == -1)
		    cmessage(ERROR, ANYKEY, "select(): %s", strerror(errno));
		else {
		    /* timeout */
		}
	    }
	}
#endif

	error_recover = 0;
	draw_board(&game[gindex], board_details, c_row, c_col);
	wmove(boardw, ROWTOMATRIX(c_row), COLTOMATRIX(c_col));

	if (!paused) {
	}

	update_panels();
	doupdate();

	if (pushkey)
	    c = pushkey;
	else {
	    if ((c = wgetch(boardw)) == ERR)
		continue;
	}

	if (!keycount && status.notify)
	    update_status_notify(game[gindex], NULL);

	// Global and other keys.
	switch (c) {
	    case 'h':
		if (game[gindex].mode != MODE_HISTORY) {
		    if (!history_total(game[gindex].hp) || status.engine ==
			    ENGINE_THINKING)
			break;

		    game[gindex].mode = MODE_HISTORY;
		    history_update_board(&game[gindex], game[gindex].b, history_total(game[gindex].hp));
		    break;
		}

		// FIXME
		if (TEST_FLAG(game[gindex].flags, GF_BLACK_OPENING)) {
		    cmessage(NULL, ANYKEY, "%s", E_RESUME_BLACK);
		    break;
		}

		// FIXME Resuming from previous history could append to a RAV.
		if (game[gindex].hindex != history_total(game[gindex].hp)) {
		    if (!pushkey) {
			if ((c = message(NULL, YESNO, "%s",
					GAME_RESUME_HISTORY_TEXT)) != 'y')
			    break;
		    }
		}
		else {
		    if (TEST_FLAG(game[gindex].flags, GF_GAMEOVER))
			break;
		}

		if (!noengine && !engine_initialized) {
		    if (start_chess_engine() < 0)
			break;

		    pushkey = 'h';
		    break;
		}

		pushkey = 0;
		oldhistorytotal = history_total(game[gindex].hp);
		game[gindex].mode = MODE_PLAY;
		status.engine = ENGINE_READY;
		update_all(game[gindex]);
		break;
	    case '>':
	    case '<':
		game_next_prev(game[gindex], (c == '>') ? 1 : 0, (keycount) ?
			keycount : 1);

		if (delete_count) {
		    markend = gindex;
		    pushkey = 'x';
		    delete_count = 0;
		}

		if (game[gindex].mode != MODE_EDIT) {
		    history_update_board(&game[gindex], game[gindex].b, history_total(game[gindex].hp));
		    update_cursor(game[gindex], game[gindex].hindex, &c_row, &c_col);
		}
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    // Not sure whether to keep these.
	    case '!': c_row = 1; break;
	    case '@': c_row = 2; break;
	    case '#': c_row = 3; break;
	    case '$': c_row = 4; break;
	    case '%': c_row = 5; break;
	    case '^': c_row = 6; break;
	    case '&': c_row = 7; break;
	    case '*': c_row = 8; break;
	    case 'A': c_col = 1; break;
	    case 'B': c_col = 2; break;
	    case 'C': c_col = 3; break;
	    case 'D': c_col = 4; break;
	    case 'E': c_col = 5; break;
	    case 'F': c_col = 6; break;
	    case 'G': c_col = 7; break;
	    case 'H': c_col = 8; break;
	    case '}':
	    case '{':
	    case '?':
		if (gtotal < 2)
		    break;

		if (!*gameexp || c == '?') {
		    if ((tmp = get_input(GAME_FIND_EXPRESSION_TITLE, gameexp,
				    1, 1, GAME_FIND_EXPRESSION_PROMPT, NULL,
				    NULL, 0, -1)) == NULL)
			break;

		    strncpy(gameexp, tmp, sizeof(gameexp));
		}

	        if ((n = find_game_exp(gameexp, (c == '{') ? 0 : 1, (keycount)
				? keycount : 1)) == 
			-1)
		    break;

		gindex = n;

		if (history_total(game[gindex].hp))
		    game[gindex].mode = MODE_HISTORY;

		history_update_board(&game[gindex], game[gindex].b, history_total(game[gindex].hp));
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'J':
		if (gtotal < 2)
		    break;

		/* FIXME field validation
		if ((tmp = get_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL, NULL,
				NULL, 0, FIELD_TYPE_INTEGER, 1, 1, gtotal))
			== NULL)
		    break;
		*/

		if (!keycount) {
		    if ((tmp = get_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL,
				    NULL, NULL, 0, -1)) == NULL)
			break;

		    if (!isinteger(tmp))
			break;

		    i = atoi(tmp);
		}
		else
		    i = keycount;

		if (--i > gtotal - 1 || i < 0)
		    break;

		gindex = i;
		history_update_board(&game[gindex], game[gindex].b, history_total(game[gindex].hp));
		update_cursor(game[gindex], game[gindex].hindex, &c_row, &c_col);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'x':
		pushkey = 0;

		if (gtotal < 2)
		    break;

		if (keycount && !delete_count) {
		    markstart = gindex;
		    delete_count = 1;
		    update_status_notify(game[gindex], "%s (delete)",
			    status.notify);
		    continue;
		}

		if (markstart >= 0 && markend >= 0) {
		    if (markstart > markend) {
			i = markstart;
			markstart = markend;
			markend = i;
		    }

		    for (i = markstart; i <= markend; i++) {
			if (toggle_delete_flag(i))
			    break;
		    }
		}
		else {
		    if (toggle_delete_flag(gindex))
			break;
		}

		markstart = markend = -1;
		update_status_window(game[gindex]);
		break;
	    case 'X':
		if (gtotal < 2) {
		    cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
		    break;
		}

		tmp = NULL;

		for (i = n = 0; i < gtotal; i++) {
		    if (TEST_FLAG(game[i].flags, GF_DELETE))
			n++;
		}

		if (!n)
		    tmp = GAME_DELETE_GAME_TEXT;
		else {
		    if (n == gtotal) {
			cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
			break;
		    }

		    tmp = GAME_DELETE_ALL_TEXT;
		}

		if (config.deleteprompt) {
		    if ((c = cmessage(NULL, YESNO, "%s", tmp)) != 'y')
			break;
		}

		delete_game((!n) ? gindex : -1);

		if (history_total(game[gindex].hp))
		    game[gindex].mode = MODE_HISTORY;

		history_update_board(&game[gindex], game[gindex].b, history_total(game[gindex].hp));
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'T':
		edit_save_tags(&game[gindex]);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 't':
		edit_tags(game[gindex], game[gindex].b, 0);
		break;
	    case 'r':
		if ((tmp = get_input(GAME_LOAD_TITLE, NULL, 1, 1,
				BROWSER_PROMPT, browse_directory, NULL, '\t',
				-1)) == NULL)
		    break;

		tmp = tilde_expand(tmp);

		if (pgn_parse_file(tmp, config.stoponerror))
		    break;

		strncpy(loadfile, tmp, sizeof(loadfile));

		if (history_total(game[gindex].hp))
		    game[gindex].mode = MODE_HISTORY;

		history_update_board(&game[gindex], game[gindex].b, history_total(game[gindex].hp));
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case 'S':
	    case 's':
		x = -1;

		if (gtotal > 1) {
		    n = message(NULL, GAME_SAVE_MULTI_PROMPT, "%s", 
			    GAME_SAVE_MULTI_TEXT);

		    if (n == 'c')
			x = gindex;
		    else if (n == 'a')
			x = -1;
		    else {
			update_status_notify(game[gindex], "%s", NOTIFY_SAVE_ABORTED);
			break;
		    }
		}

		if ((tmp = get_input(GAME_SAVE_TITLE, loadfile, 1, 1,
				BROWSER_PROMPT, browse_directory, NULL, 
				'\t', -1)) == NULL) {
		    update_status_notify(game[gindex], "%s", NOTIFY_SAVE_ABORTED);
		    break;
		}

		tmp = tilde_expand(tmp);

		if (strstr(tmp, ".") == NULL && compression_cmd(tmp, 0)
			== NULL) {
		    snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
		    tmp = tfile;
		}

		if (save_pgn(tmp, 0, x)) {
		    update_status_notify(game[gindex], "%s", NOTIFY_SAVE_FAILED);
		    break;
		}

		update_status_notify(game[gindex], "%s", NOTIFY_SAVED);
		update_all(game[gindex]);
		break;
	    case KEY_F(1):
		n = 0;

		while (n != 'q') {
		    n = help(GAME_HELP_INDEX_TITLE, GAME_HELP_INDEX_PROMPT,
			    mainhelp);

		    switch (n) {
			case 'h':
			    help(GAME_HELP_HISTORY_TITLE, ANYKEY, historyhelp);
			    break;
			case 'p':
			    help(GAME_HELP_PLAY_TITLE, ANYKEY, playhelp);
			    break;
			case 'e':
			    help(GAME_HELP_EDIT_TITLE, ANYKEY, edithelp);
			    break;
			case 'g':
			    help(GAME_HELP_GAME_TITLE, ANYKEY, gamehelp);
			    break;
			default:
			    n = 'q';
			    break;
		    }
		}

		break;
	    case 'n':
	    case 'N':
		if (c == 'N') {
		    if (cmessage(NULL, YESNO, "%s", GAME_NEW_PROMPT) != 'y')
			break;
		}

		if (c == 'n') {
		    pgn_new_game();
		    add_custom_tags(&game[gindex].tag);
		}
		else {
		    pgn_parse_file(NULL, config.stoponerror);
		    add_custom_tags(&game[gindex].tag);
		    pgn_init_board(game[gindex].b);
		}

		game[gindex].mode = MODE_PLAY;
		c_row = (game[gindex].side == WHITE) ? 2 : 7;
		c_col = 4;

		if (!noengine && (status.engine == ENGINE_OFFLINE ||
			engine_initialized == 0)) {
		    if (start_chess_engine() < 0)
			break;
		}

		SEND_TO_ENGINE("\nnew\n");
		set_engine_defaults();
		status.engine = ENGINE_READY;
		update_status_notify(game[gindex], NULL);
		update_all(game[gindex]);
		update_tag_window(game[gindex].tag);
		break;
	    case CTRL('L'):
		endwin();
		keypad(boardw, TRUE);
		update_panels();
		doupdate();
		break;
	    case KEY_ESCAPE:
		sp.icon = sp.row = sp.col = 0;
		markend = markstart = 0;

		if (keycount) {
		    keycount = 0;
		    update_status_notify(game[gindex], NULL);
		}

		if (config.validmoves)
		    board_reset_valid_moves(game[gindex].b);

		break;
	    case '0' ... '9':
		n = c - '0';

		if (keycount)
		    keycount = keycount * 10 + n;
		else
		    keycount = n;

		update_status_notify(game[gindex], "Repeat %i", keycount);
		continue;
	    case KEY_UP:
		if (game[gindex].mode == MODE_HISTORY) {
		    historymode_keys(c);
		    break;
		}

		if (keycount) {
		    c_row += keycount;
		    pushkey = '\n';
		}
		else
		    c_row++;

		if (c_row > 8)
		    c_row = 1;

		break;
	    case KEY_DOWN:
		if (game[gindex].mode == MODE_HISTORY) {
		    historymode_keys(c);
		    break;
		}

		if (keycount) {
		    c_row -= keycount;
		    pushkey = '\n';
		    update_status_notify(game[gindex], NULL);
		}
		else
		    c_row--;

		if (c_row < 1)
		    c_row = 8;

		break;
	    case KEY_LEFT:
		if (game[gindex].mode == MODE_HISTORY) {
		    historymode_keys(c);
		    break;
		}

		if (keycount) {
		    c_col -= keycount;
		    pushkey = '\n';
		}
		else
		    c_col--;

		if (c_col < 1)
		    c_col = 8;

		break;
	    case KEY_RIGHT:
		if (game[gindex].mode == MODE_HISTORY) {
		    historymode_keys(c);
		    break;
		}

		if (keycount) {
		    c_col += keycount;
		    pushkey = '\n';
		}
		else
		    c_col++;

		if (c_col > 8)
		    c_col = 1;

		break;
	    case 'e':
		if (game[gindex].mode != MODE_EDIT && game[gindex].mode !=
			MODE_PLAY)
		    break;

		// Don't edit a running game (for now).
		if (history_total(game[gindex].hp))
		    break;

		if (game[gindex].mode != MODE_EDIT) {
		    pgn_init_fen_board(&game[gindex], game[gindex].b, NULL);
		    board_details++;
		    game[gindex].mode = MODE_EDIT;
		    update_all(game[gindex]);
		    break;
		}

		board_details--;
		pgn_add_tag(&game[gindex].tag, "FEN", 
			pgn_game_to_fen(game[gindex], game[gindex].b));
		pgn_add_tag(&game[gindex].tag, "SetUp", "1");
		pgn_sort_tags(game[gindex].tag);
		game[gindex].mode = MODE_PLAY;
		update_all(game[gindex]);
		break;
	    case 'Q':
		quit = 1;
		break;
	    case KEY_RESIZE:
		do_window_resize();
		break;
#ifdef DEBUG
	    case 'O':
	        message("DEBUG BOARD", ANYKEY, "%s", debug_board(game[gindex].b));
		break;
#endif
	    case 0:
	    default:
		break;
	}

	switch (game[gindex].mode) {
	    case MODE_EDIT:
		editmode_keys(c);
		break;
	    case MODE_PLAY:
		playmode_keys(c);
		break;
	    case MODE_HISTORY:
		historymode_keys(c);
		break;
	    default:
		break;
	}

	keycount = 0;
    }
}

void usage(const char *pn, int ret)
{
    fprintf((ret) ? stderr : stdout, "%s",
    "Usage: cboard [-hvNE] [-VtRS] [-p <file>]\n"
    "  -p  Load PGN file.\n"
    "  -V  Validate a game file.\n"
    "  -S  Validate and output a PGN formatted game.\n"
    "  -R  Like -S but write a reduced PGN formatted game.\n"
    "  -t  Also write custom PGN tags from config file.\n"
    "  -N  Don't enable the chess engine (two human players).\n"
    "  -E  Stop processing on file parsing error (overrides config).\n"
    "  -v  Version information.\n"
    "  -h  This help text.\n");

    exit(ret);
}

void catch_signal(int which)
{
    switch (which) {
	case SIGINT:
	    stop_engine();
	    endwin();
	    exit(EXIT_FAILURE);
	    break;
	case SIGPIPE:
	    if (quit)
		break;

	    cmessage(NULL, ANYKEY, "%s", E_BROKEN_PIPE);
	    endwin();
	    exit(EXIT_FAILURE);
	    break;
	case SIGSTOP:
	    savetty();
	    break;
	case SIGCONT:
	    resetty();
	    keypad(boardw, TRUE);
	    break;
	default:
	    break;
    }
}

static void set_defaults()
{
    filetype = NO_FILE;
    set_config_defaults();
}

int main(int argc, char *argv[])
{
    int opt;
    struct stat st;
    char buf[FILENAME_MAX];
    char datadir[FILENAME_MAX];
    int ret = EXIT_SUCCESS;
    int validate_only = 0, validate_and_write = 0, reduced = 0;
    int write_custom_tags = 0;

    if ((config.pwd = getpwuid(getuid())) == NULL)
	err(EXIT_FAILURE, "getpwuid()");

    snprintf(datadir, sizeof(datadir), "%s/.cboard", config.pwd->pw_dir);
    snprintf(buf, sizeof(buf), "%s/cc.data", datadir);
    config.ccfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/nag.data", datadir);
    config.nagfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/agony.data", datadir);
    config.agonyfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/config", datadir);
    config.configfile = strdup(buf);
    snprintf(buf, sizeof(buf), "%s/fifo", datadir);
    config.fifo = strdup(buf);

    if (stat(datadir, &st) == -1) {
	if (errno == ENOENT) {
	    if (mkdir(datadir, 0755) == -1)
		err(EXIT_FAILURE, "%s", datadir);
	}
	else
	    err(EXIT_FAILURE, "%s", datadir);

	stat(datadir, &st);
    }

    if (!S_ISDIR(st.st_mode))
	errx(EXIT_FAILURE, "%s: %s", datadir, E_NOTADIR);

    if (access(config.fifo, R_OK) == -1 && errno == ENOENT) {
	if (mkfifo(config.fifo, 0600) == -1)
	    err(EXIT_FAILURE, "%s", config.fifo);
    }

    set_defaults();

    while ((opt = getopt(argc, argv, "ENVtSRhp:v")) != -1) {
	switch (opt) {
	    case 't':
		write_custom_tags = 1;
		break;
	    case 'E':
		config.stoponerror = 1;
		break;
	    case 'N':
		noengine = 1;
		break;
	    case 'R':
		reduced = 1;
	    case 'S':
		validate_and_write = 1;
	    case 'V':
		validate_only = 1;
		break;
	    case 'v':
		printf("%s (%s)\n%s\n", PACKAGE_STRING, curses_version(), 
			COPYRIGHT);
		exit(EXIT_SUCCESS);
	    case 'p':
		filetype = PGN_FILE;
		strncpy(loadfile, optarg, sizeof(loadfile));
		break;
	    case 'h':
	    default:
		usage(argv[0], EXIT_SUCCESS);
	}
    }

    if ((validate_only || validate_and_write) && !*loadfile)
	usage(argv[0], EXIT_FAILURE);

    if (access(config.configfile, R_OK) == 0)
	parse_rcfile(config.configfile);

    signal(SIGPIPE, catch_signal);
    signal(SIGCONT, catch_signal);
    signal(SIGSTOP, catch_signal);
    signal(SIGINT, catch_signal);

    srandom(getpid());

    switch (filetype) {
	case PGN_FILE:
	    ret = pgn_parse_file(loadfile, config.stoponerror);
	    break;
	case FEN_FILE:
	    //ret = parse_fen_file(loadfile);
	    break;
	case EPD_FILE: // Not implemented.
	case NO_FILE:
	default:
	    // No file specified. Empty game.
	    ret = pgn_parse_file(NULL, config.stoponerror);
	    add_custom_tags(&game[gindex].tag);
	    break;
    }

    if (ret == -1)
	err(EXIT_FAILURE, "%s", loadfile);

    if (validate_only || validate_and_write) {
	if (validate_and_write) {
	    int i;

	    for (i = 0; i < gtotal; i++) {
		if (write_custom_tags)
		    add_custom_tags(&game[i].tag);

		pgn_write(stdout, game[i], config.mpl, reduced);
	    }
	}

	pgn_free_all();
	exit(ret);
    }
    else if (ret)
	exit(ret);

    if (initscr() == NULL)
	errx(EXIT_FAILURE, "%s", E_INITCURSES);
    else
	curses_initialized = 1;

    if (LINES < 24 || COLS < 80) {
	endwin();
	errx(EXIT_FAILURE, "Need at least an 80x24 terminal.");
    }

    if (has_colors() == TRUE && start_color() == OK)
	init_color_pairs();

    boardw = newwin(BOARD_HEIGHT, BOARD_WIDTH, 0, COLS - BOARD_WIDTH);
    boardp = new_panel(boardw);
    historyw = newwin(HISTORY_HEIGHT, HISTORY_WIDTH, LINES - HISTORY_HEIGHT,
	    COLS - HISTORY_WIDTH);
    historyp = new_panel(historyw);
    statusw = newwin(STATUS_HEIGHT, STATUS_WIDTH, LINES - STATUS_HEIGHT, 0);
    statusp = new_panel(statusw);
    tagw = newwin(TAG_HEIGHT, TAG_WIDTH, 0, 0);
    tagp = new_panel(tagw);
    keypad(boardw, TRUE);
//  leaveok(boardw, TRUE);
    leaveok(tagw, TRUE);
    leaveok(statusw, TRUE);
    leaveok(historyw, TRUE);
    curs_set(0);
    cbreak();
    noecho();
    draw_window_decor();

    game_loop();
    stop_engine();

    endwin();
    pgn_free_all();
    del_panel(boardp);
    del_panel(historyp);
    del_panel(statusp);
    del_panel(tagp);
    delwin(boardw);
    delwin(historyw);
    delwin(statusw);
    delwin(tagw);
    exit(EXIT_SUCCESS);
}
