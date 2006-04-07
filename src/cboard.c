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

#ifdef HAVE_WORDEXP_H
#include <wordexp.h>
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
#include "common.h"
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

void update_cursor(GAME g, int idx)
{
    char *p;
    int len;
    int t = pgn_history_total(g.hp);

    /*
     * If not deincremented then r and c would be the next move.
     */
    idx--;

    if (idx > t || idx < 0 || !t || !g.hp[idx]->move) {
	c_row = 2, c_col = 5;
	return;
    }

    p = g.hp[idx]->move;
    len = strlen(p);

    if (*p == 'O') {
	if (len <= 4)
	    c_col = 7;
	else
	    c_col = 3;

	c_row = (g.turn == WHITE) ? 1 : 8;
	return;
    }

    p += len;

    while (!isdigit(*p))
	p--;

    c_row = ROWTOINT(*p--);
    c_col = COLTOINT(*p);
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
	refresh_all();
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
    char buf[strlen(h.move) + strlen(ANNOTATION_VIEW_TITLE) + 4];
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

char *browse_directory(void *arg)
{
    char *inputstr = (char *)arg;
    int initkey = (inputstr) ? inputstr[0] : 0;
    char pattern[FILENAME_MAX];
    static char path[FILENAME_MAX];
    static char file[FILENAME_MAX];
    struct stat st;
    char *p;

    if (!*path) {
	if (config.savedirectory) {
	    if ((p = word_expand(config.savedirectory)) == NULL)
		return NULL;

	    strncpy(path, p, sizeof(path));

	    if (access(path, R_OK) == -1) {
		cmessage(ERROR, ANYKEY, "%s: %s", path, strerror(errno));
		getcwd(path, sizeof(path));
	    }
	}
	else
	    getcwd(path, sizeof(path));
    }

again:
    /*
     * First find directories (including hidden) in the working directory.
     * Then apply the config.pattern to regular files.
     */
    if ((p = word_split_append(path, '/', ".* *")) == NULL)
	return NULL;

    strncpy(pattern, p, sizeof(pattern));

    while (1) {
	WINDOW *win, *subw;
	PANEL *panel;
	ITEM **mitems = NULL;
	MENU *menu;
	char *tmp = NULL;
	int rows, cols;
	int selected = -1;
	char *mbuf = NULL;
	int idx = 0;
	int len = strlen(path);
	wordexp_t w;
	int i, n = 0;
	struct d_entries *entries = NULL;
	int which = 1;
	int x = WRDE_NOCMD;

new_we:
	if (wordexp(pattern, &w, x) != 0) {
	    cmessage(ERROR, ANYKEY, "Error in pattern\n%s", pattern);
	    return NULL;
	}

	for (i = 0; i < w.we_wordc; i++) {
	    struct tm *tp;
	    char tbuf[16];

	    if (stat(w.we_wordv[i], &st) == -1)
		continue;

	    if ((p = strrchr(w.we_wordv[i], '/')) != NULL)
		p++;
	    else
		p = w.we_wordv[i];

	    if (which) {
		if (!S_ISDIR(st.st_mode))
		    continue;

		if (p[0] == '.' && p[1] == 0)
		    continue;
	    }
	    else {
		if (S_ISDIR(st.st_mode))
		    continue;
	    }

	    len = strlen(p) + 2;
	    entries = Realloc(entries, (n + 2) * sizeof(struct d_entries));
	    entries[n].name = strdup(w.we_wordv[i]);
	    entries[n].fancy = Malloc(len);
	    strncpy(entries[n].fancy, p, len);

	    if (S_ISDIR(st.st_mode))
		entries[n].fancy[len - 2] = '/';

	    tp = localtime(&st.st_mtime);
	    strftime(tbuf, sizeof(tbuf), "%b %d %T", tp);

	    snprintf(entries[n].desc, sizeof(entries[n].desc), "%-7i %s", 
		    (int)st.st_size, tbuf);

	    memset(&entries[++n], '\0', sizeof(struct d_entries));
	}

	which--;

	if (which == 0) {
	    if ((p = word_split_append(path, '/', config.pattern)) == NULL)
		return NULL;

	    strncpy(pattern, p, sizeof(pattern));
	    x |= WRDE_REUSE;
	    goto new_we;
	}

	wordfree(&w);
	qsort(entries, n, sizeof(struct d_entries), sort_entries);

	for (i = 0; i < n; i++) {
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
	    refresh_all();
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
		    file[0] = 0;
		    goto done;
		    break;
		case KEY_F(1):
		    help(BROWSER_HELP, ANYKEY, file_browser_help);
		    break;
		case '~':
		    strncpy(path, "~/", sizeof(path));
		    cleanup(win, subw, panel, menu, mitems, entries);
		    goto again;
		    break;
		case CTRL('X'):
		    if ((tmp = get_input_str_clear(BROWSER_CHDIR_TITLE, NULL)) 
			    == NULL)
			break;

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
	strncpy(file, entries[selected].name, sizeof(file));
	cleanup(win, subw, panel, menu, mitems, entries);

	if (stat(file, &st) == -1) {
	    cmessage(ERROR, ANYKEY, "%s\n%s", file, strerror(errno));
	    continue;
	}

	if (S_ISDIR(st.st_mode)) {
	    p = file + strlen(file) - 2;

	    if (strcmp(p, "..") == 0) {
		p = file + strlen(file) - 3;
		*p = 0;

		if ((p = strrchr(file, '/')) != NULL)
		    file[strlen(file) - strlen(p)] = 0;
	    }

	    strncpy(path, file, sizeof(path));
	    goto again;
	}

	if (S_ISREG(st.st_mode))
	    break;

	cmessage(ERROR, ANYKEY, "%s\n%s", file, E_NOTAREGFILE);
    }

done:
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
	refresh_all();
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
    int total = pgn_tag_total(config.tag);

    if (!config.tag)
	return;

    for (i = 0; i < total; i++)
	pgn_tag_add(t, config.tag[i]->name, config.tag[i]->value);

    pgn_tag_sort(*t);
}

TAG **edit_tags(GAME g, BOARD b, int edit)
{
    TAG **data = NULL;
    struct tm tp;
    int data_index = 0;
    int len;
    int selected = 0;
    int n;
    int toppos = 0;

    /* Edit the backup copy, not the original in case the save fails. */
    len = pgn_tag_total(g.tag);

    for (n = 0; n < len; n++)
	pgn_tag_add(&data, g.tag[n]->name, g.tag[n]->value);

    data_index = pgn_tag_total(data);

    while (1) {
	WINDOW *win;
	PANEL *panel;
	int i;
	char buf[76] = {0};
	char *tmp = NULL;
	int rows, cols;
	int nlen = 0;

	data_index = pgn_tag_total(data);

	for (i = cols = 0, n = 4; i < data_index; i++) {
	    n = strlen(data[i]->name);

	    if (nlen < n)
		nlen = n;

	    if (data[i]->value)
		n += strlen(data[i]->value);
	    else
		n += strlen(UNKNOWN);
	    
	    if (cols < n)
		cols = n;
	}

	cols += nlen + 2;

	if (cols > COLS)
	    cols = COLS - 2;

	/* +14 for the extra prompt info. */
	if (cols < strlen(HELP_PROMPT) + 14 + 2)
	    cols = strlen(HELP_PROMPT) + 14 + 2;

	rows = (data_index + 4 > (LINES / 5) * 4) ? (LINES / 5) * 4 : 
	    data_index + 4;

	win = newwin(rows, cols, CALCPOSY(rows), CALCPOSX(cols));
	panel = new_panel(win);
	cbreak();
	noecho();
	nl();
	keypad(win, TRUE);
	wbkgd(win, CP_MESSAGE_WINDOW);
	draw_window_title(win, (edit) ? TAG_EDIT_TITLE : TAG_VIEW_TITLE, 
		cols, CP_MESSAGE_TITLE, CP_MESSAGE_BORDER);
	
	if (selected >= data_index - 1)
	    selected = data_index - 1;

	while (1) {
	    int c;
	    TAG **tmppgn = NULL;
	    char *newtag = NULL;

	    for (i = toppos, c = 2; i < data_index && c < rows - 2; i++, c++) {
		if (i == selected) {
		    wattron(win, CP_MESSAGE_WINDOW | A_REVERSE);
		    mvwprintw(win, c, 1, "%*s: %-*s", nlen, data[i]->name,
			    cols - nlen - 2 - 2, (data[i]->value && 
				data[i]->value[0]) ? data[i]->value : UNKNOWN);
		    wattroff(win, CP_MESSAGE_WINDOW | A_REVERSE);
		    continue;
		}

		mvwprintw(win, c, 1, "%*s: %-*s", nlen, data[i]->name,
			cols - nlen - 2 - 2, (data[i]->value && 
			    data[i]->value[0]) ? data[i]->value : UNKNOWN);
	    }

	    snprintf(buf, sizeof(buf), "%s %i %s %i  %s", MENU_TAG_STR,
		    selected + 1, N_OF_N_STR, data_index, HELP_PROMPT);
	    draw_prompt(win, rows - 2, cols, buf, CP_MESSAGE_PROMPT);
	    refresh_all();
	    c = wgetch(win);

	    switch (c) {
		case CTRL('T'):
		    if (!edit)
			break;

		    add_custom_tags(&data);
		    selected = data_index - 1;
		    toppos = data_index - (rows - 4);
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

		    if (selected <= 6) {
			cmessage(NULL, ANYKEY, "%s", E_REMOVE_STR);
			goto cleanup;
		    }

		    data_index = pgn_tag_total(data);

		    for (i = 0; i < data_index; i++) {
			if (i == selected)
			    continue;

			pgn_tag_add(&tmppgn, data[i]->name, data[i]->value);
		    }

		    pgn_tag_free(data);
		    data = NULL;

		    for (i = 0; tmppgn[i]; i++)
			pgn_tag_add(&data, tmppgn[i]->name, tmppgn[i]->value);

		    pgn_tag_free(tmppgn);
		    
		    if (selected >= data_index)
			selected = data_index - 1;

		    toppos -= (toppos) ? 1 : 0;
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

		    pgn_tag_add(&data, newtag, NULL);
		    data_index = pgn_tag_total(data);
		    selected = data_index - 1;
		    toppos = (data_index > rows) ? data_index - (rows - 4) : 0;
		    goto gotitem;
		    break;
		case KEY_HOME:
		    selected = toppos = 0;
		    break;
		case KEY_END:
		    selected = data_index - 1;
		    toppos = data_index - (rows - 4);
		    break;
		case CTRL('F'):
		    if (!edit)
			break;

		    pgn_tag_add(&data, "FEN", pgn_game_to_fen(g, b));
		    data_index = pgn_tag_total(data);
		    selected = data_index - 1;
		    toppos = (data_index > rows) ? data_index - (rows - 4) : 0;
		    goto gotitem;
		    break;
		case KEY_UP:
		    if (selected - 1 < 0) {
			selected = data_index - 1;

			toppos = selected - (rows - 5);
		    }
		    else {
			selected--;

			if (toppos && selected <= toppos)
			    toppos = selected;
		    }
		    break;
		case KEY_DOWN:
		    if (selected + 1 >= data_index)
			selected = toppos = 0;
		    else {
			selected++;

			if (selected - toppos > rows - 5)
			    toppos++;
		    }
		    break;
		case '\n':
		    goto gotitem;
		    break;
		case KEY_ESCAPE:
		    del_panel(panel);
		    delwin(win);
		    goto done;
		    break;
		default:
		    break;
	    }
	}

gotitem:
	nlen = strlen(data[selected]->name) + 2;
	nlen += (edit) ? strlen(TAG_EDIT_TAG_TITLE) : strlen(TAG_VIEW_TAG_TITLE);

	if (nlen > MAX_VALUE_WIDTH)
	    snprintf(buf, sizeof(buf), "%s", data[selected]->name);
	else
	    snprintf(buf, sizeof(buf), "%s \"%s\"",
		    (edit) ? TAG_EDIT_TAG_TITLE : TAG_VIEW_TAG_TITLE,
		    data[selected]->name);

	if (!edit) {
	    if (!data[selected]->value)
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
	    tmp = (data[selected]->value) ? data[selected]->value : NULL;
	    tmp = get_input(buf, tmp, 0, 0, NULL, NULL, NULL, 0, -1);
	}

	len = (tmp) ? strlen(tmp) + 1 : 1;
	data[selected]->value = Realloc(data[selected]->value, len);
	strncpy(data[selected]->value, (tmp) ? tmp : "", len);

cleanup:
	del_panel(panel);
	delwin(win);
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
		    if (pgn_is_compressed(filename)) {
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
	pgn_write(fp, game[saveindex]);
    else {
	for (i = (saveindex == -1) ? 0 : saveindex; i < saveindex_max; i++)
	    pgn_write(fp, game[i]);
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

static void draw_board(GAME *g, int details)
{
    int row, col;
    int bcol = 0, brow = 0;
    int maxy = BOARD_HEIGHT, maxx = BOARD_WIDTH;
    int ncols = 0, offset = 1;
    unsigned coords_y = 8;

    if (g->mode != MODE_PLAY && g->mode != MODE_EDIT)
	update_cursor(*g, g->hindex);

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

		    if (row == ROWTOMATRIX(c_row) && col == 
			    COLTOMATRIX(c_col)) {
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
    struct userdata_s *d = g->data;

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

    p = str;

    if (TEST_FLAG(d->flags, CF_HUMAN)) {
	if (pgn_validate_move(g, b, &p)) {
	    invalid_move(gindex + 1, p);
	    return NULL;
	}

	return p;
    }

    if (pgn_validate_only(g, b, &p)) {
	invalid_move(gindex + 1, p);
	return NULL;
    }

    return p;
}

static int move_to_engine(GAME *g, BOARD b)
{
    char *p;
    struct userdata_s *d = g->data;

    if ((p = board_to_san(g, b)) == NULL)
	return 0;

    sp.row = sp.col = sp.icon = 0;

    if (TEST_FLAG(d->flags, CF_HUMAN)) {
	pgn_history_add(g, p);
	pgn_switch_turn(g);
	SET_FLAG(g->flags, GF_MODIFIED);
	update_all(*g);
	return 1;
    }

    send_to_engine(g, "%s\n", p);
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
    struct userdata_s *d = g.data;

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

    snprintf(buf, len - 1, "%*s %s", 7, STATUS_MODE_STR, mode);

    if (g.mode == MODE_PLAY) {
	if (TEST_FLAG(d->flags, CF_HUMAN))
	    strncat(buf, " (human/human)", len - 1);
	else if (TEST_FLAG(d->flags, CF_ENGINE_LOOP))
	    strncat(buf, " (engine/engine)", len - 1);
	else
	    strncat(buf, " (human/engine)", len - 1);
    }

    mvwprintw(statusw, 4, 1, "%-*s", len, buf);

    if (d->engine) {
	switch (d->engine->status) {
	    case ENGINE_THINKING:
		engine = ENGINE_PONDER_STR;
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
    }
    else
	engine = ENGINE_OFFLINE_STR;

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
    int t = pgn_history_total(g.hp);

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

    h = pgn_history_by_n(g.hp, g.hindex);
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

    h = pgn_history_by_n(g.hp, game[gindex].hindex - 1);
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

void append_enginebuf(char *line)
{
    int i = 0;

    if (enginebuf)
	for (i = 0; enginebuf[i]; i++);

    if (i >= LINES - 3) {
	free(enginebuf[0]);

	for (i = 0; enginebuf[i+1]; i++)
	    enginebuf[i] = enginebuf[i+1];

	enginebuf[i] = strdup(line);
    }
    else {
	enginebuf = Realloc(enginebuf, (i + 2) * sizeof(char *));
	enginebuf[i++] = strdup(line);
	enginebuf[i] = NULL;
    }
}

void update_engine_window()
{
    int i;

    if (!enginebuf)
	return;

    wmove(enginew, 0, 0);
    wclrtobot(enginew);

    if (enginebuf) {
	for (i = 0; enginebuf[i]; i++)
	    mvwprintw(enginew, i + 2, 1, "%s", enginebuf[i]);
    }

    draw_window_title(enginew, "Engine IO Window", COLS, CP_MESSAGE_TITLE,
	    CP_MESSAGE_BORDER);
}

void toggle_engine_window()
{
    if (!enginew) {
	enginew = newwin(LINES, COLS, 0, 0);
	enginep = new_panel(enginew);
	draw_window_title(enginew, "Engine IO Window", COLS, CP_MESSAGE_TITLE,
		CP_MESSAGE_BORDER);
	hide_panel(enginep);
    }

    if (panel_hidden(enginep)) {
	update_engine_window();
	top_panel(enginep);
	refresh_all();
    }
    else {
	hide_panel(enginep);
	refresh_all();
    }
}

void refresh_all()
{
    update_panels();
    doupdate();
}

void update_all(GAME g)
{
    update_status_window(g);
    update_history_window(g);
    update_tag_window(g.tag);
    update_engine_window();
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

	if (i >= pgn_history_total(g.hp))
	    i = 0;
	else if (i < 0)
	    i = pgn_history_total(g.hp) - 1;

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
    pgn_tag_sort(g->tag);
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
    pgn_board_init_fen(g, b, g->rav[g->ravlevel].fen);
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

static void historymode_keys(chtype);
static int playmode_keys(chtype c)
{
    // More keys in MODE_EDIT share keys with MODE_PLAY than don't.
    int editmode = (game[gindex].mode == MODE_EDIT) ? 1 : 0;
    chtype p;
    int w, x;
    char *tmp;
    struct userdata_s *d = game[gindex].data;

    switch (c) {
	case 'H':
	    TOGGLE_FLAG(d->flags, CF_HUMAN);

	    if (!TEST_FLAG(d->flags, CF_HUMAN) &&
		    pgn_history_total(game[gindex].hp)) {
		pgn_tag_add(&game[gindex].tag, "FEN", 
			pgn_game_to_fen(game[gindex], game[gindex].b));
		x = pgn_tag_find(game[gindex].tag, "FEN");

		if (start_chess_engine(&game[gindex]) <= 0) {
		    send_to_engine(&game[gindex], "setboard %s\n",
			    game[gindex].tag[x]->value);
		    d->engine->status = ENGINE_READY;
		}
	    }

	    CLEAR_FLAG(d->flags, CF_ENGINE_LOOP);

	    if (d->engine)
		d->engine->status = ENGINE_READY;

	    update_all(game[gindex]);
	    break;
	case 'E':
	    if (!d)
		break;

	    TOGGLE_FLAG(d->flags, CF_ENGINE_LOOP);
	    CLEAR_FLAG(d->flags, CF_HUMAN);

	    if (d->engine && !TEST_FLAG(d->flags, CF_ENGINE_LOOP))
		d->engine->status = ENGINE_READY;

	    update_all(game[gindex]);
	    break;
	case '|':
	    if (!d->engine)
		break;

	    if (d->engine->status == ENGINE_OFFLINE)
		break;
	    
	    x = d->engine->status;

	    if ((tmp = get_input_str_clear(ENGINE_CMD_TITLE, NULL)) != NULL)
		send_to_engine(&game[gindex], "%s\n", tmp);
	    d->engine->status = x;
	    break;
	case '\015':
	case '\n':
	    pushkey = keycount = 0;
	    update_status_notify(game[gindex], NULL);

	    if (!editmode && !TEST_FLAG(d->flags, CF_HUMAN) &&
		    (!d->engine || d->engine->status == ENGINE_THINKING)) {
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
		game[gindex].b[ROWTOBOARD(sp.row)][COLTOBOARD(sp.col)].icon =
		    pgn_int_to_piece(game[gindex].turn, OPEN_SQUARE);
		sp.icon = sp.row = sp.col = 0;
		break;
	    }

	    if (move_to_engine(&game[gindex], game[gindex].b)) {
		if (config.validmoves)
		    pgn_reset_valid_moves(game[gindex].b);

		if (TEST_FLAG(game[gindex].flags, GF_GAMEOVER)) {
		    CLEAR_FLAG(game[gindex].flags, GF_GAMEOVER);
		    SET_FLAG(game[gindex].flags, GF_MODIFIED);
		}
	    }

	    break;
	case ' ':
	    if (!TEST_FLAG(d->flags, CF_HUMAN) && (!d->engine ||
			d->engine->status == ENGINE_OFFLINE) && !editmode) {
		if (start_chess_engine(&game[gindex])) {
		    sp.icon = 0;
		    break;
		}

		x = pgn_tag_find(game[gindex].tag, "FEN");
		w = pgn_tag_find(game[gindex].tag, "SetUp");

		if ((w >= 0 && x >= 0 && atoi(game[gindex].tag[w]->value) == 1) 
			|| (x >= 0 && w == -1)) {
		    send_to_engine(&game[gindex], "setboard %s\n",
			    game[gindex].tag[x]->value);
		    d->engine->status = ENGINE_READY;
		}
	    }

	    if (sp.icon || (!editmode && d->engine &&
			d->engine->status == ENGINE_THINKING)) {
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
		pgn_get_valid_moves(&game[gindex], game[gindex].b, sp.row,
			sp.col);

	    paused = 0;
	    break;
	case 'w':
	    send_to_engine(&game[gindex], "\nswitch\n");
	    switch_side(&game[gindex]);
	    update_status_window(game[gindex]);
	    break;
	case 'u':
	    if (!pgn_history_total(game[gindex].hp))
		break;

	    if (d->engine && d->engine->status == ENGINE_READY) {
		send_to_engine(&game[gindex], "remove\n");
		d->engine->status = ENGINE_READY;
	    }

	    game[gindex].hindex -= 2;
	    pgn_history_free(game[gindex].hp, game[gindex].hindex);
	    game[gindex].hindex = pgn_history_total(game[gindex].hp);
	    pgn_board_update(&game[gindex], game[gindex].b, 
		    game[gindex].hindex);
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
	case 'g':
	    if (!d->engine || d->engine->status == ENGINE_OFFLINE)
		start_chess_engine(&game[gindex]);

	    send_to_engine(&game[gindex], "go\n");
	    break;
	default:
	    if (!d->engine)
		break;

	    if (config.keys) {
		for (x = 0; config.keys[x]; x++) {
		    if (config.keys[x]->c == c) {
			send_to_engine(&game[gindex], "%s\n",
				config.keys[x]->str);
			d->engine->status = ENGINE_READY;
			break;
		    }
		}
	    }
	    break;
    }

    return 0;
}

static void editmode_keys(chtype c)
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

static void historymode_keys(chtype c)
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
	    pgn_history_next(&game[gindex], game[gindex].b, (keycount > 0) ?
		    config.jumpcount * keycount * movestep : 
		    config.jumpcount * movestep);
	    update_all(game[gindex]);
	    break;
	case KEY_DOWN:
	    pgn_history_prev(&game[gindex], game[gindex].b, (keycount) ?
		    config.jumpcount * keycount * movestep : 
		    config.jumpcount * movestep);
	    update_all(game[gindex]);
	    break;
	case KEY_LEFT:
	    pgn_history_prev(&game[gindex], game[gindex].b, (keycount) ?
		    keycount * movestep : movestep);
	    update_all(game[gindex]);
	    break;
	case KEY_RIGHT:
	    pgn_history_next(&game[gindex], game[gindex].b, (keycount) ? 
		    keycount * movestep : movestep);
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
	    if (pgn_history_total(game[gindex].hp) < 2)
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
	    pgn_board_update(&game[gindex], game[gindex].b, game[gindex].hindex);
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
	    if (pgn_history_total(game[gindex].hp) < 2)
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

	    if (n < 0 || n > (pgn_history_total(game[gindex].hp) / 2))
		break;

	    game[gindex].hindex = (n) ? n * 2 - 1 : n * 2;
	    pgn_board_update(&game[gindex], game[gindex].b,
		    game[gindex].hindex);
	    update_all(game[gindex]);
	    break;
	default: 
	    break;
    }
}

static void cleanup_all_games()
{
    int i;

    for (i = 0; i < gtotal; i++) {
	struct userdata_s *d;

	if (game[i].data) {
	    stop_engine(&game[i]);
	    d = game[i].data;
	    free(game[i].data);
	    game[i].data = NULL;
	}
    }
}

void update_loading_window()
{
    if (!loadingw) {
	loadingw = newwin(3, COLS / 2, CALCPOSY(3), CALCPOSX(COLS / 2));
	loadingp = new_panel(loadingw);
	wbkgd(loadingw, CP_MESSAGE_WINDOW);
    }

    wmove(loadingw, 0, 0);
    wclrtobot(loadingw);
    wattron(loadingw, CP_MESSAGE_BORDER);
    box(loadingw, ACS_VLINE, ACS_HLINE);
    wattroff(loadingw, CP_MESSAGE_BORDER);
    mvwprintw(loadingw, 1, CENTER_INT((COLS / 2),
		11 + strlen(itoa(gtotal))), "Loading... %i", gtotal);
    update_panels();
    doupdate();
}

void init_userdata()
{
    int i;

    for (i = 0; i < gtotal; i++) {
	struct userdata_s *d = NULL;

	d = Calloc(1, sizeof(struct userdata_s));
	game[i].data = d;
	d->n = i;
    }
}

// Global and other keys.
static int globalkeys(chtype c)
{
    static char gameexp[255] = {0};
    FILE *fp;
    char *tmp, *p;
    int n, i;
    char tfile[FILENAME_MAX];
    struct userdata_s *d = game[gindex].data;

    switch (c) {
	case 'W':
	    toggle_engine_window();
	    break;
	case KEY_F(10):
	    cmessage("ABOUT", ANYKEY, "%s\n%s with %i colors and %i "
		    "color pairs\nCopyright 2002-2006 %s", PACKAGE_STRING,
		    curses_version(), COLORS, COLOR_PAIRS, PACKAGE_BUGREPORT);
	    break;
	case 'h':
	    if (game[gindex].mode != MODE_HISTORY) {
		if (!pgn_history_total(game[gindex].hp) || 
			(d->engine && d->engine->status == ENGINE_THINKING))
		    return 1;

		game[gindex].mode = MODE_HISTORY;
		pgn_board_update(&game[gindex], game[gindex].b, pgn_history_total(game[gindex].hp));
		update_all(game[gindex]);
		return 1;
	    }

	    // FIXME
	    if (TEST_FLAG(game[gindex].flags, GF_BLACK_OPENING)) {
		cmessage(NULL, ANYKEY, "%s", E_RESUME_BLACK);
		return 1;
	    }

	    // FIXME Resuming from previous history could append to a RAV.
	    if (game[gindex].hindex != pgn_history_total(game[gindex].hp)) {
		if (!pushkey) {
		    if ((c = message(NULL, YESNO, "%s",
				    GAME_RESUME_HISTORY_TEXT)) != 'y')
			return 1;
		}
	    }
	    else {
		if (TEST_FLAG(game[gindex].flags, GF_GAMEOVER))
		    return 1;
	    }

	    if (!TEST_FLAG(d->flags, CF_HUMAN) && (!d->engine || 
			d->engine->status == ENGINE_OFFLINE)) {
		if (start_chess_engine(&game[gindex]) < 0)
		    return 1;

		pushkey = 'h';
		return 1;
	    }

	    pushkey = 0;
	    oldhistorytotal = pgn_history_total(game[gindex].hp);
	    game[gindex].mode = MODE_PLAY;
	    update_all(game[gindex]);
	    return 1;
	case '>':
	case '<':
	    game_next_prev(game[gindex], (c == '>') ? 1 : 0, (keycount) ?
		    keycount : 1);

	    if (delete_count) {
		markend = delete_count;
		pushkey = 'x';
		delete_count = 0;
	    }

	    if (game[gindex].mode != MODE_EDIT) {
		pgn_board_update(&game[gindex], game[gindex].b, pgn_history_total(game[gindex].hp));
	    }
	    update_all(game[gindex]);
	    update_tag_window(game[gindex].tag);
	    return 1;
	case '}':
	case '{':
	case '?':
		  if (gtotal < 2)
		      return 1;

		  if (!*gameexp || c == '?') {
		      if ((tmp = get_input(GAME_FIND_EXPRESSION_TITLE, gameexp,
				      1, 1, GAME_FIND_EXPRESSION_PROMPT, NULL,
				      NULL, 0, -1)) == NULL)
			  return 1;

		      strncpy(gameexp, tmp, sizeof(gameexp));
		  }

		  if ((n = find_game_exp(gameexp, (c == '{') ? 0 : 1, (keycount)
				  ? keycount : 1)) == 
			  -1)
		      return 1;

		  gindex = n;

		  if (pgn_history_total(game[gindex].hp))
		      game[gindex].mode = MODE_HISTORY;

		  pgn_board_update(&game[gindex], game[gindex].b, pgn_history_total(game[gindex].hp));
		  update_all(game[gindex]);
		  update_tag_window(game[gindex].tag);
		  return 1;
	case 'J':
		  if (gtotal < 2)
		      return 1;

		  /* FIXME field validation
		     if ((tmp = get_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL, NULL,
		     NULL, 0, FIELD_TYPE_INTEGER, 1, 1, gtotal))
		     == NULL)
		     return 1;
		     */

		  if (!keycount) {
		      if ((tmp = get_input(GAME_JUMP_TITLE, NULL, 1, 1, NULL,
				      NULL, NULL, 0, -1)) == NULL)
			  return 1;

		      if (!isinteger(tmp))
			  return 1;

		      i = atoi(tmp);
		  }
		  else
		      i = keycount;

		  if (--i > gtotal - 1 || i < 0)
		      return 1;

		  gindex = i;
		  pgn_board_update(&game[gindex], game[gindex].b, pgn_history_total(game[gindex].hp));
		  update_all(game[gindex]);
		  update_tag_window(game[gindex].tag);
		  return 1;
	case 'x':
		  pushkey = 0;

		  if (gtotal < 2)
		      return 1;

		  if (keycount && !delete_count) {
		      markstart = gindex;
		      delete_count = keycount;
		      update_status_notify(game[gindex], "%s (delete)",
			      status.notify);
		      return 1;
		  }

		  if (markstart >= 0 && markend >= 0) {
		      if (markstart > markend) {
			  i = markstart;
			  markstart = markend;
			  markend = i;
		      }

		      for (i = markstart; i <= markend; i++) {
			  if (toggle_delete_flag(i))
			      return 1;
		      }
		  }
		  else {
		      if (toggle_delete_flag(gindex))
			  return 1;
		  }

		  markstart = markend = -1;
		  update_status_window(game[gindex]);
		  return 1;
	case 'X':
		  if (gtotal < 2) {
		      cmessage(NULL, ANYKEY, "%s", E_DELETE_GAME);
		      return 1;
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
			  return 1;
		      }

		      tmp = GAME_DELETE_ALL_TEXT;
		  }

		  if (config.deleteprompt) {
		      if ((c = cmessage(NULL, YESNO, "%s", tmp)) != 'y')
			  return 1;
		  }

		  delete_game((!n) ? gindex : -1);

		  if (pgn_history_total(game[gindex].hp))
		      game[gindex].mode = MODE_HISTORY;

		  pgn_board_update(&game[gindex], game[gindex].b, pgn_history_total(game[gindex].hp));
		  update_all(game[gindex]);
		  update_tag_window(game[gindex].tag);
		  return 1;
	case 'T':
		  edit_save_tags(&game[gindex]);
		  update_all(game[gindex]);
		  update_tag_window(game[gindex].tag);
		  return 1;
	case 't':
		  edit_tags(game[gindex], game[gindex].b, 0);
		  return 1;
	case 'r':
		  if ((tmp = get_input(GAME_LOAD_TITLE, NULL, 1, 1,
				  BROWSER_PROMPT, browse_directory, NULL, '\t',
				  -1)) == NULL)
		      return 1;

		  if ((tmp = word_expand(tmp)) == NULL)
		      break;

		  if ((fp = pgn_open(tmp)) == NULL) {
		      cmessage(ERROR, ANYKEY, "%s\n%s", tmp, strerror(errno));
		      return 1;
		  }

		  if (pgn_parse(fp))
		      return 1;

		  del_panel(loadingp);
		  delwin(loadingw);
		  loadingw = NULL;
		  loadingp = NULL;
		  init_userdata();
		  strncpy(loadfile, tmp, sizeof(loadfile));

		  if (pgn_history_total(game[gindex].hp))
		      game[gindex].mode = MODE_HISTORY;

		  pgn_board_update(&game[gindex], game[gindex].b, pgn_history_total(game[gindex].hp));
		  update_all(game[gindex]);
		  update_tag_window(game[gindex].tag);
		  return 1;
	case 'S':
	case 's':
		  i = -1;

		  if (gtotal > 1) {
		      n = message(NULL, GAME_SAVE_MULTI_PROMPT, "%s", 
			      GAME_SAVE_MULTI_TEXT);

		      if (n == 'c')
			  i = gindex;
		      else if (n == 'a')
			  i = -1;
		      else {
			  update_status_notify(game[gindex], "%s", NOTIFY_SAVE_ABORTED);
			  return 1;
		      }
		  }

		  if ((tmp = get_input(GAME_SAVE_TITLE, loadfile, 1, 1,
				  BROWSER_PROMPT, browse_directory, NULL, 
				  '\t', -1)) == NULL) {
		      update_status_notify(game[gindex], "%s", NOTIFY_SAVE_ABORTED);
		      return 1;
		  }

		  if ((tmp = word_expand(tmp)) == NULL)
		      break;

		  if (pgn_is_compressed(tmp)) {
		      snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
		      tmp = tfile;
		  }
		  else {
		      if ((p = strchr(tmp, '.')) != NULL) {
			  if (strcmp(p, ".pgn") != 0) {
			      snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
			      tmp = tfile;
			  }
		      }
		      else {
			  snprintf(tfile, sizeof(tfile), "%s.pgn", tmp);
			  tmp = tfile;
		      }
		  }

		  if (save_pgn(tmp, 0, i)) {
		      update_status_notify(game[gindex], "%s", NOTIFY_SAVE_FAILED);
		      return 1;
		  }

		  update_status_notify(game[gindex], "%s", NOTIFY_SAVED);
		  update_all(game[gindex]);
		  return 1;
	case KEY_F(1):
		  n = 0;

		  switch (game[gindex].mode) {
		      case MODE_PLAY:
			  c = help(GAME_HELP_PLAY_TITLE, ANYKEY, playhelp);
			  break;
		      case MODE_HISTORY:
			  c = help(GAME_HELP_HISTORY_TITLE, ANYKEY, historyhelp);
			  break;
		      case MODE_EDIT:
			  c = help(GAME_HELP_EDIT_TITLE, ANYKEY, edithelp);
			  break;
		      default:
			  break;
		  }

		  while (c == KEY_F(1)) {
		      c = help(GAME_HELP_INDEX_TITLE, GAME_HELP_INDEX_PROMPT,
			      mainhelp);

		      switch (c) {
			  case 'h':
			      c = help(GAME_HELP_HISTORY_TITLE, ANYKEY, historyhelp);
			      break;
			  case 'p':
			      c = help(GAME_HELP_PLAY_TITLE, ANYKEY, playhelp);
			      break;
			  case 'e':
			      c = help(GAME_HELP_EDIT_TITLE, ANYKEY, edithelp);
			      break;
			  case 'g':
			      c = help(GAME_HELP_GAME_TITLE, ANYKEY, gamehelp);
			      break;
			  default:
			      break;
		      }
		  }

		  return 1;
	case 'n':
	case 'N':
		  if (c == 'N') {
		      if (cmessage(NULL, YESNO, "%s", GAME_NEW_PROMPT) != 'y')
			  return 1;
		  }

		  if (c == 'n') {
		      pgn_new_game();
		      add_custom_tags(&game[gindex].tag);
		      d = Calloc(1, sizeof(struct userdata_s));
		      game[gindex].data = d;
		  }
		  else {
		      cleanup_all_games();
		      pgn_parse(NULL);
		      add_custom_tags(&game[gindex].tag);
		      pgn_board_init(game[gindex].b);
		      d = Calloc(1, sizeof(struct userdata_s));
		      game[gindex].data = d;
		  }

		  game[gindex].mode = MODE_PLAY;
		  c_row = (game[gindex].side == WHITE) ? 2 : 7;
		  c_col = 4;
		  update_status_notify(game[gindex], NULL);
		  update_all(game[gindex]);
		  update_tag_window(game[gindex].tag);
		  return 1;
	case CTRL('L'):
		  endwin();
		  keypad(boardw, TRUE);
		  refresh_all();
		  return 1;
	case KEY_ESCAPE:
		  sp.icon = sp.row = sp.col = 0;
		  markend = markstart = 0;

		  if (keycount) {
		      keycount = 0;
		      update_status_notify(game[gindex], NULL);
		  }

		  if (config.validmoves)
		      pgn_reset_valid_moves(game[gindex].b);

		  return 1;
	case '0' ... '9':
		  n = c - '0';

		  if (keycount)
		      keycount = keycount * 10 + n;
		  else
		      keycount = n;

		  update_status_notify(game[gindex], "Repeat %i", keycount);
		  return -1;
	case KEY_UP:
		  if (game[gindex].mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      c_row += keycount;
		      pushkey = '\n';
		  }
		  else
		      c_row++;

		  if (c_row > 8)
		      c_row = 1;

		  return 1;
	case KEY_DOWN:
		  if (game[gindex].mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      c_row -= keycount;
		      pushkey = '\n';
		      update_status_notify(game[gindex], NULL);
		  }
		  else
		      c_row--;

		  if (c_row < 1)
		      c_row = 8;

		  return 1;
	case KEY_LEFT:
		  if (game[gindex].mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      c_col -= keycount;
		      pushkey = '\n';
		  }
		  else
		      c_col--;

		  if (c_col < 1)
		      c_col = 8;

		  return 1;
	case KEY_RIGHT:
		  if (game[gindex].mode == MODE_HISTORY)
		      return 0;

		  if (keycount) {
		      c_col += keycount;
		      pushkey = '\n';
		  }
		  else
		      c_col++;

		  if (c_col > 8)
		      c_col = 1;

		  return 1;
	case 'e':
		  if (game[gindex].mode != MODE_EDIT && game[gindex].mode !=
			  MODE_PLAY)
		      return 1;

		  // Don't edit a running game (for now).
		  if (pgn_history_total(game[gindex].hp))
		      return 1;

		  if (game[gindex].mode != MODE_EDIT) {
		      pgn_board_init_fen(&game[gindex], game[gindex].b, NULL);
		      board_details++;
		      game[gindex].mode = MODE_EDIT;
		      update_all(game[gindex]);
		      return 1;
		  }

		  board_details--;
		  pgn_tag_add(&game[gindex].tag, "FEN", 
			  pgn_game_to_fen(game[gindex], game[gindex].b));
		  pgn_tag_add(&game[gindex].tag, "SetUp", "1");
		  pgn_tag_sort(game[gindex].tag);
		  game[gindex].mode = MODE_PLAY;
		  update_all(game[gindex]);
		  return 1;
	case 'Q':
		  quit = 1;
		  return 1;
	case KEY_RESIZE:
		  do_window_resize();
		  return 1;
#ifdef DEBUG
	case 'D':
		  message("DEBUG BOARD", ANYKEY, "%s", debug_board(game[gindex].b));
		  return 1;
#endif
	case 0:
	default:
		  break;
    }

    return 0;
}

void game_loop()
{  
    int error_recover = 0;

    c_row = 2, c_col = 5;
    gindex = gtotal - 1;

    if (pgn_history_total(game[gindex].hp))
	game[gindex].mode = MODE_HISTORY;
    else
	game[gindex].mode = MODE_PLAY;

    if (game[gindex].mode == MODE_HISTORY) {
	pgn_board_update(&game[gindex], game[gindex].b,
		pgn_history_total(game[gindex].hp));
    }

    update_status_notify(game[gindex], "%s", GAME_HELP_PROMPT);
    movestep = 2;
    paused = 1; //FIXME clock
    flushinp();
    update_all(game[gindex]);
    update_tag_window(game[gindex].tag);
    wtimeout(boardw, 70);

    while (!quit) {
	int c = 0;
	int n = 0, i;
	char fdbuf[8192] = {0};
	int len;
	struct timeval tv = {0, 0};
	fd_set rfds;
	struct userdata_s *d = NULL;

	FD_ZERO(&rfds);

	for (i = 0; i < gtotal; i++) {
	    d = game[i].data;

	    if (d->engine) {
		if (d->engine->fd[ENGINE_IN_FD] > 2) {
		    if (d->engine->fd[ENGINE_IN_FD] > n)
			n = d->engine->fd[ENGINE_IN_FD];

		    FD_SET(d->engine->fd[ENGINE_IN_FD], &rfds);
		}
	    }
	}

	if (n) {
	    if ((n = select(n + 1, &rfds, NULL, NULL, &tv)) > 0) {
		for (i = 0; i < gtotal; i++) {
		    d = game[i].data;

		    if (d->engine) {
			if (FD_ISSET(d->engine->fd[ENGINE_IN_FD], &rfds)) {
			    len = read(d->engine->fd[ENGINE_IN_FD], fdbuf,
				    sizeof(fdbuf));

			    if (len == -1) {
				if (errno != EAGAIN) {
				    cmessage(ERROR, ANYKEY, "Engine read(): %s",
					    strerror(errno));
				    waitpid(d->engine->pid, &n, 0);
				    free(d->engine);
				    d->engine = NULL;
				    break;
				}
			    }
			    else {
				if (len) {
				    parse_engine_output(&game[i], fdbuf);
				    update_all(game[gindex]);
				}
			    }
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

	if (TEST_FLAG(game[gindex].flags, GF_GAMEOVER) && game[gindex].mode 
		!= MODE_HISTORY) {
	    game[gindex].mode = MODE_HISTORY;
	    update_all(game[gindex]);
	}

	error_recover = 0;
	draw_board(&game[gindex], board_details);
	wmove(boardw, ROWTOMATRIX(c_row), COLTOMATRIX(c_col));

	if (!paused) {
	}

	refresh_all();

	if (pushkey)
	    c = pushkey;
	else {
	    if ((c = wgetch(boardw)) == ERR)
		continue;
	}

	if (!keycount && status.notify)
	    update_status_notify(game[gindex], NULL);


	if ((n = globalkeys(c)) == 1) {
	    keycount = 0;
	    continue;
	}
	else if (n == -1)
	    continue;

	switch (game[gindex].mode) {
	    case MODE_EDIT:
		editmode_keys(c);
		break;
	    case MODE_PLAY:
		if (playmode_keys(c))
		    continue;
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
    "Usage: cboard [-hvE] [-VtRS] [-p <file>]\n"
    "  -p  Load PGN file.\n"
    "  -V  Validate a game file.\n"
    "  -S  Validate and output a PGN formatted game.\n"
    "  -R  Like -S but write a reduced PGN formatted game.\n"
    "  -t  Also write custom PGN tags from config file.\n"
    "  -E  Stop processing on file parsing error (overrides config).\n"
    "  -v  Version information.\n"
    "  -h  This help text.\n");

    exit(ret);
}

void cleanup_all()
{
    int i;

    cleanup_all_games();
    pgn_free_all();
    del_panel(boardp);
    del_panel(historyp);
    del_panel(statusp);
    del_panel(tagp);
    delwin(boardw);
    delwin(historyw);
    delwin(statusw);
    delwin(tagw);

    if (enginew) {
	del_panel(enginep);
	delwin(enginew);

	if (enginebuf) {
	    for (i = 0; enginebuf[i]; i++)
		free(enginebuf[i]);

	    free(enginebuf);
	}
    }

    endwin();
}

void catch_signal(int which)
{
    switch (which) {
	case SIGINT:
	case SIGPIPE:
	    if (which == SIGPIPE && quit)
		break;

	    if (which == SIGPIPE)
		cmessage(NULL, ANYKEY, "%s", E_BROKEN_PIPE);

	    cleanup_all();
	    exit(EXIT_FAILURE);
	    break;
	case SIGSTOP:
	    savetty();
	    break;
	case SIGCONT:
	    resetty();
	    keypad(boardw, TRUE);
	    curs_set(0);
	    cbreak();
	    noecho();
	    break;
	case SIGUSR1:
	    if (curses_initialized) {
		update_loading_window(game[gindex]);
		break;
	    }

	    fprintf(stderr, "Loading... %i\r", gtotal);
	    fflush(stderr);
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
    FILE *fp;
    int i;

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

    while ((opt = getopt(argc, argv, "EVtSRhp:v")) != -1) {
	switch (opt) {
	    case 't':
		write_custom_tags = 1;
		break;
	    case 'E':
		config.stoponerror = 1;
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
    signal(SIGUSR1, catch_signal);

    srandom(getpid());

    switch (filetype) {
	case PGN_FILE:
	    if ((fp = pgn_open(loadfile)) == NULL)
		err(EXIT_FAILURE, "%s", loadfile);

	    ret = pgn_parse(fp);
	    break;
	case FEN_FILE:
	    //ret = parse_fen_file(loadfile);
	    break;
	case EPD_FILE: // Not implemented.
	case NO_FILE:
	default:
	    // No file specified. Empty game.
	    ret = pgn_parse(NULL);
	    add_custom_tags(&game[gindex].tag);
	    break;
    }

    if (validate_only || validate_and_write) {
	if (validate_and_write) {
	    for (i = 0; i < gtotal; i++) {
		if (write_custom_tags)
		    add_custom_tags(&game[i].tag);

		pgn_write(stdout, game[i]);
	    }
	}

	pgn_free_all();
	exit(ret);
    }
    else if (ret)
	exit(ret);

    init_userdata();

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
    cleanup_all();
    exit(EXIT_SUCCESS);
}
