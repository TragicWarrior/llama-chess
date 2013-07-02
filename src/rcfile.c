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
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <err.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

#include "common.h"
#include "conf.h"
#include "misc.h"
#include "colors.h"
#include "keys.h"
#include "rcfile.h"
#include "common.h"

static int attributes(const char *filename, int line, char *str)
{
    char *tmp;
    int attrs = 0;

    while ((tmp = strsep(&str, ",")) != NULL) {
	if (strcasecmp(tmp, "BOLD") == 0)
	    attrs |= A_BOLD;
	else if (strcasecmp(tmp, "REVERSE") == 0)
	    attrs |= A_REVERSE;
	else if (strcasecmp(tmp, "NONE") == 0)
	    attrs |= A_NORMAL;
	else if (strcasecmp(tmp, "DIM") == 0)
	    attrs |= A_DIM;
	else if (strcasecmp(tmp, "STANDOUT") == 0)
	    attrs |= A_STANDOUT;
	else if (strcasecmp(tmp, "UNDERLINE") == 0)
	    attrs |= A_UNDERLINE;
	else if (strcasecmp(tmp, "BLINK") == 0)
	    attrs |= A_BLINK;
	else if (strcasecmp(tmp, "INVISIBLE") == 0)
	    attrs |= A_INVIS;
	else
	  errx(EXIT_FAILURE, _("%s(%i): invalid attribute \"%s\""), filename,
	       line, tmp);
    }

    return attrs;
}

static short color_name(const char *filename, int line, const char *color)
{
    if (strcasecmp(color, "BLACK") == 0)
	return COLOR_BLACK;
    else if (strcasecmp(color, "WHITE") == 0)
	return COLOR_WHITE;
    else if (strcasecmp(color, "GREEN") == 0)
	return COLOR_GREEN;
    else if (strcasecmp(color, "YELLOW") == 0)
	return COLOR_YELLOW;
    else if (strcasecmp(color, "MAGENTA") == 0)
	return COLOR_MAGENTA;
    else if (strcasecmp(color, "BLUE") == 0)
	return COLOR_BLUE;
    else if (strcasecmp(color, "RED") == 0)
	return COLOR_RED;
    else if (strcasecmp(color, "CYAN") == 0)
	return COLOR_CYAN;
    else if (strcasecmp(color, "-") == 0)
	;
    else
        errx(EXIT_FAILURE, _("%s(%i): invalid color \"%s\""), filename, line,
	     color);

    return -1;
}

static void parse_color(const char *filename, int line, const char *str,
	struct color_s *c)
{
    char fg[16], bg[16], attr[64], nattr[64];
    int n;
    
    if ((n = sscanf(str, "%[a-zA-Z-] %[a-zA-Z-] %[a-zA-Z,-] %[a-zA-Z,-]", fg, bg,
		    attr, nattr)) < 2)
        errx(EXIT_FAILURE, _ ("%s(%i): parse error"), filename, line);

    if ((n = color_name(filename, line, fg)) >= 0)
	c->fg = n;

    if ((n = color_name(filename, line, bg)) >= 0)
	c->bg = n;

    c->attrs = c->nattrs = 0;

    if (n > 2)
	c->attrs = attributes(filename, line, attr);

    if (n > 3)
	c->nattrs = attributes(filename, line, nattr);
}

static int on_or_off(const char *filename, int lines, const char *str)
{
    if (strcasecmp(str, "on") == 0 || strcasecmp(str, "1") == 0 || 
	    strcasecmp(str, "yes") == 0 || strcasecmp(str, "true") == 0)
	return 1;

    if (strcasecmp(str, "off") == 0 || strcasecmp(str, "0") == 0 ||
	    strcasecmp(str, "no") == 0 || strcasecmp(str, "false") == 0)
	return 0;

    errx(EXIT_FAILURE, _("%s(%i): invalid value \"%s\""), filename, lines, str);
}

void copydatafile(const char *dst, const char *src)
{
    FILE *fp, *ofp;
    char buf[LINE_MAX], *s;

    snprintf(buf, sizeof(buf), "%s/%s", DATA_PATH, src);

    if ((fp = fopen(buf, "r")) == NULL) {
	if (errno != ENOENT)
	    warn("%s", buf);
	return;
    }

    if ((ofp = fopen(dst, "w+")) == NULL) {
	fclose(fp);
	warn("%s", dst);
	return;
    }

    while ((s = fgets(buf, sizeof(buf), fp)) != NULL)
	fprintf(ofp, "%s", s);

    fclose(fp);
    fclose(ofp);
}

static char *fancy_key_name(wint_t c)
{
    static char buf[64] = {0};
    char *p;

    /* FIXME key_name() supports wint_t but cannot return a string containing
     * META characters. */
    strncpy(buf, keyname(c), sizeof(buf)-1);
    p = buf;

    if (*p == '^' && *(p + 1) == '[')
        return _("Escape");

    if (*p == '^' && *(p + 1) == 'J')
        return _ ("Enter");

    if (strncasecmp(p, "KEY_", 4) == 0) {
	for (p = buf; *p; p++)
	    *p = tolower(*p);

	p = buf;
	p += 4;

	if (*p == 'f') {
	    char *t = buf + 4;

	    p += 2;

	    while (isdigit(*p))
		*++t = *p++;

	    *++t = 0;
	    p = buf + 4;
	}
	else if (strcmp(p, "ppage") == 0)
	    return _("PgUp");
	else if (strcmp(p, "npage") == 0)
	    return _ ("PgDown");

	*p = toupper(*p);
	return p;
    }

    if (*p == ' ')
        return _("Space");

    return p;
}

void add_key_binding(struct key_s ***dst, key_func *func, wint_t c,
		     char *desc, int repeat)
{
    int i = 0;
    struct key_s **k = *dst;

    if (k)
	for (i = 0; k[i]; i++);

    k = Realloc(k, (i + 2) * sizeof(struct key_s *));
    k[i] = Calloc(1, sizeof(struct key_s));
    k[i]->f = func;
    k[i]->c = c;
    k[i]->r = repeat;
    k[i]->key = strdup(fancy_key_name(c));

    if (desc)
	k[i]->d = str_to_wchar(desc);

    i++;
    k[i] = NULL;
    *dst = k;
}

void set_default_keys()
{
    add_key_binding(&history_keys, do_history_jump_next, KEY_UP, _("history jump next"), 1);
    add_key_binding(&history_keys, do_history_jump_prev, KEY_DOWN, _("history jump previous"), 1);
    add_key_binding(&history_keys, do_history_next, KEY_RIGHT, _("next move"), 1);
    add_key_binding(&history_keys, do_history_prev, KEY_LEFT, _("previous move"), 1);
    add_key_binding(&history_keys, do_history_half_move_toggle, ' ', _("toggle half move (ply) stepping"), 0);
    add_key_binding(&history_keys, do_history_rotate_board, 'r', _("rotate board"), 0);
    add_key_binding(&history_keys, do_history_jump, 'j', _("jump to move number"), 1);
    add_key_binding(&history_keys, do_history_find_new, '/', _("new move text expression"), 0);
    add_key_binding(&history_keys, do_history_find_next, ']', _("find next move text expression"), 1);
    add_key_binding(&history_keys, do_history_find_prev, '[', _("find previous move text expression"), 1);
    add_key_binding(&history_keys, do_history_annotate, CTRL_KEY('a'), _("annotate the previous move"), 0);
    add_key_binding(&history_keys, do_history_rav_next, '+', _("next variation of the previous move"), 0);
    add_key_binding(&history_keys, do_history_rav_prev, '-', _("previous variation of the previous move"), 0);
    add_key_binding(&history_keys, do_history_menu, 'M', _("move history tree"), 0);
    add_key_binding(&history_keys, do_history_help, KEY_F(1), _("more help"), 0);
    add_key_binding(&history_keys, do_history_help, CTRL_KEY('g'), NULL, 0);
    add_key_binding(&history_keys, do_history_toggle, 'h', _("exit history mode"), 0);

    add_key_binding(&edit_keys, do_edit_select, ' ', _("select piece for movement"), 0);
    add_key_binding(&edit_keys, do_edit_commit, '\n', _("commit selected piece"), 0);
    add_key_binding(&edit_keys, do_edit_cancel_selected, KEY_ESCAPE, _("cancel selected piece"), 0);
    add_key_binding(&edit_keys, do_edit_delete, 'd', _("remove the piece under the cursor"), 0);
    add_key_binding(&edit_keys, do_edit_insert, 'i', _("insert piece"), 0);
    add_key_binding(&edit_keys, do_edit_toggle_castle, 'c', _("toggle castling availability"), 0);
    add_key_binding(&edit_keys, do_edit_enpassant, 'p', _("toggle enpassant square"), 0);
    add_key_binding(&edit_keys, do_edit_switch_turn, 'w', _("toggle turn"), 0);
    add_key_binding(&edit_keys, do_edit_help, KEY_F(1), _("more help"), 0);
    add_key_binding(&history_keys, do_edit_help, CTRL_KEY('g'), NULL, 0);
    add_key_binding(&edit_keys, do_edit_exit, 'e', _("exit edit mode"), 0);

    add_key_binding(&play_keys, do_play_select, ' ', _("select piece for movement"), 0);
    add_key_binding(&play_keys, do_play_commit, '\n', _("commit selected piece"), 0);
    add_key_binding(&play_keys, do_play_cancel_selected, KEY_ESCAPE, _("cancel selected piece"), 0);
    add_key_binding(&play_keys, do_play_set_clock, 'C', _("set clock"), 0);
//    add_key_binding(&play_keys, do_play_switch_turn, 'w', _("switch turn"), 0);
    add_key_binding(&play_keys, do_play_undo, 'u', _("undo previous move"), 1);
    add_key_binding(&play_keys, do_play_go, 'g', _("force the chess engine to make the next move"), 0);
    add_key_binding(&play_keys, do_play_send_command, '|', _("send a command to the chess engine"), 0);
    add_key_binding(&play_keys, do_play_toggle_eh_mode, 'w', _("toggle engine/human play"), 0);
    add_key_binding(&play_keys, do_play_toggle_engine, 'E', _("toggle engine/engine play"), 0);
    add_key_binding(&play_keys, do_play_toggle_human, 'H', _("toggle human/human play"), 0);
    add_key_binding(&play_keys, do_play_toggle_pause, 'p', _("toggle pausing of this game"), 0);
    add_key_binding(&play_keys, do_play_history_mode, 'h', _("enter history mode"), 0);
    add_key_binding(&play_keys, do_play_edit_mode, 'e', _("enter edit mode"), 0);
    add_key_binding(&play_keys, do_play_help, KEY_F(1), _("more help"), 0);
    add_key_binding(&play_keys, do_play_help, CTRL_KEY('g'), NULL, 0);

    add_key_binding(&global_keys, do_global_tag_edit, CTRL_KEY('t'), _("edit roster tags"), 0);
    add_key_binding(&global_keys, do_global_tag_view, 't', _("view roster tags"), 0);
    add_key_binding(&global_keys, do_global_find_new, '?', _("new find game expression"), 0);
    add_key_binding(&global_keys, do_global_find_next, '}', _("find next game"), 1);
    add_key_binding(&global_keys, do_global_find_prev, '{', _("find previous game"), 1);
    add_key_binding(&global_keys, do_global_new_game, CTRL_KEY('n'), _("new game or round"), 0);
    add_key_binding(&global_keys, do_global_new_all, CTRL_KEY('k'), _("new game from scratch"), 0);
    add_key_binding(&global_keys, do_global_copy_game, CTRL_KEY('i'), _("copy current game"), 0);
    add_key_binding(&global_keys, do_global_next_game, '>', _("next game"), 1);
    add_key_binding(&global_keys, do_global_prev_game, '<', _("previous game"), 1);
    add_key_binding(&global_keys, do_global_game_jump, 'J', _("jump to game"), 1);
    add_key_binding(&global_keys, do_global_toggle_delete, 'X', _("toggle delete flag"), 1);
    add_key_binding(&global_keys, do_global_delete_game, CTRL_KEY('X'), _("delete the current or flagged games"), 0);
    add_key_binding(&global_keys, do_global_resume_game, CTRL_KEY('r'), _("load a PGN file"), 0);
    add_key_binding(&global_keys, do_global_save_game, 's', _("save game"), 0);
    add_key_binding(&global_keys, do_global_toggle_board_details, CTRL_KEY('d'), _("toggle board details"), 0);
    add_key_binding(&global_keys, do_global_toggle_strict_castling, CTRL_KEY('p'), _("toggle strict castling"), 0);
    add_key_binding(&global_keys, do_global_toggle_engine_window, 'W', _("toggle chess engine IO window"), 0);
#ifdef WITH_LIBPERL
    add_key_binding(&global_keys, do_global_perl, CTRL_KEY('O'), _("Call PERL subroutine"), 0);
#endif
    add_key_binding(&global_keys, do_global_about, KEY_F(10), _("version information"), 0);
    add_key_binding(&global_keys, do_global_quit, 'Q', _("quit"), 0);
}

void set_config_defaults()
{
    struct stat st;

    config.pattern = strdup("*.[Pp][Gg][Nn]*");
    config.engine_cmd = strdup("gnuchess --xboard");
    config.engine_protocol = 1;
    config.jumpcount = 5;
    config.linegraphics = 1;
    config.saveprompt = 1;
    config.deleteprompt = 1;
    config.validmoves = 1;
    config.details = 1;
    config.showattacks = 1;
    config.coordsyleft = TRUE;
    config.fmpolyglot = FALSE;
    config.boardleft = TRUE;
    config.exitdialogbox = TRUE;
    config.enginecmdblacktag = TRUE;

    set_default_colors();

    if (stat(config.nagfile, &st) == -1) {
	if (errno == ENOENT)
	    copydatafile(config.nagfile, "nag.data");
	else
	    warn("%s", config.nagfile);
    }

    if (stat(config.ccfile, &st) == -1) {
	if (errno == ENOENT)
	    copydatafile(config.ccfile, "cc.data");
	else
	    warn("%s", config.ccfile);
    }
}

static void update_key(struct key_s **dst, struct custom_key_s config_key, 
	wint_t c, char *desc)
{
    int i;

    for (i = 0; dst[i]; i++) {
	if (dst[i]->f == config_key.func) {
	    dst[i]->c = c;

	    if (dst[i]->key)
		free(dst[i]->key);

	    dst[i]->key = strdup(fancy_key_name(c));

	    if (desc && dst[i]->d)
		free(dst[i]->d);

	    if (desc)
		dst[i]->d = str_to_wchar(desc);

	    goto done;
	}
    }

    add_key_binding(&dst, config_key.func, c,
		    (desc) ? desc : _("no description"), config_key.r);

done:
    return;
}

static int parse_key(const char *filename, int lines, char **key)
{
    char *p = *key;
    char *orig = *key;
    wint_t c = 0;

    if (!key || !key[0])
	return 0;

    if (*p == '\"') {
	if (orig[strlen(orig) - 1] != '\"')
	    errx(EXIT_FAILURE, _("%s(%i): unbalanced quotes"), filename, lines);

	p++;
	orig[strlen(orig) - 1] = 0;
    }

    if (*p == '<') {
	p++;

	if (strncasecmp(p, "up", 2) == 0) {
	    c = KEY_UP;
	    p += 2;
	}
	else if (strncasecmp(p, "down", 4) == 0) {
	    c = KEY_DOWN;
	    p += 4;
	}
	else if (strncasecmp(p, "left", 4) == 0) {
	    c = KEY_LEFT;
	    p += 4;
	}
	else if (strncasecmp(p, "right", 5) == 0) {
	    c = KEY_RIGHT;
	    p += 5;
	}
	else if (strncasecmp(p, "home", 4) == 0) {
	    c = KEY_HOME;
	    p += 4;
	}
	else if (strncasecmp(p, "end", 3) == 0) {
	    c = KEY_END;
	    p += 3;
	}
	else if (strncasecmp(p, "delete", 6) == 0) {
	    c = KEY_DC;
	    p += 6;
	}
	else if (strncasecmp(p, "pgup", 4) == 0) {
	    c = KEY_PPAGE;
	    p += 4;
	}
	else if (strncasecmp(p, "pgdn", 4) == 0) {
	    c = KEY_NPAGE;
	    p += 4;
	}
	else if (strncasecmp(p, "insert", 5) == 0) {
	    c = KEY_IC;
	    p += 5;
	}
	else if (strncasecmp(p, "space", 5) == 0) {
	    c = ' ';
	    p += 5;
	}
	else if (strncasecmp(p, "escape", 6) == 0) {
	    c = KEY_ESCAPE;
	    p += 6;
	}
	else if (strncasecmp(p, "enter", 5) == 0) {
	    c = '\n';
	    p += 5;
	}
	else if (*p == '^') {
	    p++;
	    c = CTRL_KEY(*p++);
	}
	else if (*p == 'F' || *p == 'f') {
	    c = KEY_F(atoi(++p));
	    p += integer_len(atoi(p));
	}

	if (!c)
	    c = *p++;

	if (*p++ != '>')
	    errx(EXIT_FAILURE, _("%s(%i): parse error \"%s\""), filename,
		 lines, orig);
    }
    else if (*p == '\\') {
	    p++;
	    c = *p++;
    }
    else
	c = *p++;

    orig += strlen(orig) - strlen(p);
    *key = orig;
    return c;
}

static void parse_key_binding(const char *filename, int lines, char *val)
{
    char mode[64], key[16], func[64], desc[64];
    int n;
    int m = 0;
    wint_t c = 0;
    int f;
    int i;
    char *p;

    n = sscanf(val, "%s %s %s %s", mode, key, func, desc);

    if (n < 3)
        errx(EXIT_FAILURE, _ ("%s(%i): too few arguments"), filename, lines);

    if (strcasecmp(mode, "history") == 0)
	m = MODE_HISTORY;
    else if (strcasecmp(mode, "play") == 0)
	m = MODE_PLAY;
    else if (strcasecmp(mode, "edit") == 0)
	m = MODE_EDIT;
    else
        errx(EXIT_FAILURE, _ ("%s(%i): invalid game mode \"%s\""), filename,
	     lines, mode);

    p = key;
    c = parse_key(filename, lines, &p);

    if (m != -1) {
	for (i = 0; global_keys[i]; i++) {
	    if (global_keys[i]->c == c)
	        errx(EXIT_FAILURE,
		     _("%s(%i): key \"%s\" conflicts with a global key"),
		     filename, lines, key);
	}
    }

    for (f = -2, i = 0; config_keys[i].name; i++) {
	if (strcmp(config_keys[i].name, func) == 0 && 
		config_keys[i].mode == m) {
	    f = i;
	    break;
	}
    }

    if (f == -2)
        errx(EXIT_FAILURE, _("%s(%i): invalid command \"%s\""), filename,
	     lines, func);

    switch (m) {
	case MODE_PLAY:
	    update_key(play_keys, config_keys[f], c, (n > 3) ? desc : NULL);
	    break;
	case MODE_HISTORY:
	    update_key(history_keys, config_keys[f], c, (n > 3) ? desc : NULL);
	    break;
	case MODE_EDIT:
	    update_key(edit_keys, config_keys[f], c, (n > 3) ? desc : NULL);
	    break;
	default:
	    update_key(global_keys, config_keys[f], c, (n > 3) ? desc : NULL);
	    break;
    }
}

static void parse_macro(const char *filename, int lines, char *val)
{
    char mode[16], key[8], keys[256] = {0};
    int n;
    int m;
    wint_t c;
    int i = 0;
    char *p;

    n = sscanf(val, "%s %s %255c", mode, key, keys);

    if (n != 3)
         errx(EXIT_FAILURE, _("%s(%i): too few arguments"), filename, lines);

    if (strcasecmp(mode, "history") == 0)
	m = MODE_HISTORY;
    else if (strcasecmp(mode, "play") == 0)
	m = MODE_PLAY;
    else if (strcasecmp(mode, "edit") == 0)
	m = MODE_EDIT;
    else if (strcasecmp(mode, "any") == 0)
	m = -1;
    else
        errx(EXIT_FAILURE, _ ("%s(%i): invalid game mode \"%s\""), filename,
	     lines, mode);

    p = key;
    c = parse_key(filename, lines, &p);

    if (macros)
	for (i = 0; macros[i]; i++);

    macros = Realloc(macros, (i + 2) * sizeof(struct macro_s));
    macros[i] = Calloc(1, sizeof(struct macro_s));
    macros[i]->c = c;
    macros[i]->mode = m;
    p = keys;

    while ((c = parse_key(filename, lines, &p)) != 0) {
	macros[i]->keys = Realloc(macros[i]->keys, (macros[i]->total + 2) *
		sizeof(int));
	macros[i]->keys[macros[i]->total++] = c;
    }

    macros[++i] = NULL;
}

void parse_rcfile(const char *filename)
{
    FILE *fp;
    char *line, buf[LINE_MAX];
    int lines = 0;
    char *altengine = NULL;
    int init = 0;
    int k = 0;
    int c;

    if ((fp = fopen(filename, "r")) == NULL)
	err(EXIT_FAILURE, "%s", filename);

    while ((line = fgets(buf, sizeof(buf), fp)) != NULL) {
	int n;
	char var[30] = {0}, val[LINE_MAX - sizeof(var) - 1] = {0};
	char token[MAX_PGN_LINE_LEN + 1] = {0};
	char value[MAX_PGN_LINE_LEN + 1] = {0};
	char *p;

	lines++;
	line = trim(line);

	if (!line[0] || line[0] == '#')
	    continue;

	if ((n = sscanf(line, "%s %[^\n]", var, val)) != 2)
	  errx(EXIT_FAILURE, _("%s(%i): parse error %i"), filename, lines,n);

	p = strdup(trim(val));
	strncpy(val, p, sizeof(val)-1);
	free(p);
	p = strdup(trim(var));
	strncpy(var, p, sizeof(var)-1);
	free(p);

	if (strcmp(var, "jump_count") == 0) {
	    if (!isinteger(val))
	      errx(EXIT_FAILURE, _("%s(%i): value is not an integer"), filename,
		   lines);

	    config.jumpcount = atoi(val);
	}
	else if (strcmp(var, "engine_init") == 0) {
	    config.einit = Realloc(config.einit, (init + 2) * sizeof(char *));
	    config.einit[init++] = strdup(val);
	    config.einit[init] = NULL;
	}
	else if (strcmp(var, "pattern") == 0) {
	    free(config.pattern);
	    config.pattern = strdup(val);
	}
	else if (strcmp(var, "mpl") == 0) {
	    if (!isinteger(val))
	        errx(EXIT_FAILURE, _("%s(%i): value is not an integer"),
		     filename, lines);
	    pgn_config_set(PGN_MPL, atoi(val));
	}
	else if (strcmp(var, "stop_on_error") == 0)
	    pgn_config_set(PGN_STOP_ON_ERROR, on_or_off(filename, lines, val));
	else if (strcmp(var, "tag") == 0) {
	    if ((n = sscanf(val, "%s %s ", token, value)) < 1 || n > 2)
	      errx(EXIT_FAILURE, _ ("%s(%i): invalid value \"%s\""), filename,
			lines, val);

	    if (n == 1)
		value[0] = 0;
	    else {
		p = val + strlen(token);
		strncpy(value, p, sizeof(value)-1);
	    }

	    for (n = 0; n < strlen(token); n++) {
		if (!isalnum(token[n]) && token[n] != '_')
		    errx(EXIT_FAILURE,
			 _ ("%s(%i): token names must match 0-9A-Za-z_."),
			 filename, lines);
	    }

	    token[0] = toupper(token[0]);
	    pgn_tag_add(&config.tag, token, value);
	}
	else if (strcmp(var, "save_directory") == 0)
	    config.savedirectory = strdup(val);
	else if (strcmp(var, "line_graphics") == 0)
	    config.linegraphics = on_or_off(filename, lines, val);
	else if (strcmp(var, "save_prompt") == 0)
	    config.saveprompt = on_or_off(filename, lines, val);
	else if (strcmp(var, "delete_prompt") == 0)
	    config.deleteprompt = on_or_off(filename, lines, val);
	else if (strcmp(var, "valid_moves") == 0)
	    config.validmoves = on_or_off(filename, lines, val);
	else if (strcmp(var, "board_details") == 0)
	    config.details = on_or_off(filename, lines, val);
	else if (strcmp(var, "show_attacks") == 0)
	    config.showattacks = on_or_off(filename, lines, val);
	else if (strcmp(var, "coords_y_left") == 0 )
	    config.coordsyleft = on_or_off(filename, lines, val);
	else if (strcmp(var, "fm_polyglot") == 0 )
	    config.fmpolyglot = on_or_off(filename, lines, val);
	else if (strcmp(var, "board_left") == 0 )
	    config.boardleft = on_or_off(filename, lines, val);
	else if (strcmp(var, "exit_dialog_box") == 0 )
	    config.exitdialogbox = on_or_off(filename, lines, val);
	else if (strcmp(var, "engine_cmd_blacktag") == 0 )
	    config.enginecmdblacktag = on_or_off(filename, lines, val);
	else if (strcmp(var, "strict_castling") == 0)
	    pgn_config_set(PGN_STRICT_CASTLING, on_or_off(filename, lines, val));
	else if (strcmp(var, "engine_cmd") == 0)
	    altengine = strdup(val);
	else if (strcmp(var, "engine_protocol") == 0) {
	    if (!isinteger(val))
	      errx(EXIT_FAILURE, _ ("%s(%i): value is not an integer"),
		   filename, lines);

	    config.engine_protocol = atoi(val);

	    if (config.engine_protocol != 1 && config.engine_protocol != 2)
	      errx(EXIT_FAILURE, _ ("%s(%i): invalid value"), filename, lines);
	}
	else if (strcmp(var, "utf8_pieces") == 0)
	    config.utf8_pieces = on_or_off (filename, lines, val);
	else if (strcmp(var, "color_board_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BDWINDOW]);
	else if (strcmp(var, "color_board_selected") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BSELECTED]);
	else if (strcmp(var, "color_board_white_moves") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BMOVESW]);
	else if (strcmp(var, "color_board_black_moves") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BMOVESB]);
	else if (strcmp(var, "color_board_count") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BCOUNT]);
	else if (strcmp(var, "color_board_cursor") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BCURSOR]);
	else if (strcmp(var, "color_board_black") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BBLACK]);
	else if (strcmp(var, "color_board_white") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BWHITE]);
	else if (strcmp(var, "color_board_graphics") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BGRAPHICS]);
	else if (strcmp(var, "color_board_coords") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BCOORDS]);
	else if (strcmp(var, "color_board_castling") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BCASTLING]);
	else if (strcmp(var, "color_board_enpassant") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BENPASSANT]);
	else if (strcmp(var, "color_board_attack") == 0)
	  parse_color(filename, lines, val, &config.color[CONF_BATTACK]);
	else if (strcmp(var, "color_status_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SWINDOW]);
	else if (strcmp(var, "color_status_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_STITLE]);
	else if (strcmp(var, "color_status_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SBORDER]);
	else if (strcmp(var, "color_status_notify") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SNOTIFY]);
	else if (strcmp(var, "color_status_engine") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SENGINE]);
	else if (strcmp(var, "color_tag_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_TWINDOW]);
	else if (strcmp(var, "color_tag_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_TTITLE]);
	else if (strcmp(var, "color_tag_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_TBORDER]);
	else if (strcmp(var, "color_history_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_HWINDOW]);
	else if (strcmp(var, "color_history_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_HTITLE]);
	else if (strcmp(var, "color_history_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_HBORDER]);
	else if (strcmp(var, "color_message_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MWINDOW]);
	else if (strcmp(var, "color_message_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MTITLE]);
	else if (strcmp(var, "color_message_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MBORDER]);
	else if (strcmp(var, "color_message_prompt") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MPROMPT]);
	else if (strcmp(var, "color_input_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_IWINDOW]);
	else if (strcmp(var, "color_input_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_ITITLE]);
	else if (strcmp(var, "color_input_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_IBORDER]);
	else if (strcmp(var, "color_input_prompt") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_IPROMPT]);
	else if (strcmp(var, "color_menu") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MENU]);
	else if (strcmp(var, "color_menu_selected") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MENUS]);
	else if (strcmp(var, "color_menu_highlight") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MENUH]);
	else if (strcmp(var, "color_menu_graphics") == 0)
	    parse_color(filename, lines, val,
		    &config.color[CONF_HISTORY_MENU_LG]);
	else if (strcmp(var, "bind") == 0)
	    parse_key_binding(filename, lines, val);
	else if (strcmp(var, "macro") == 0)
	    parse_macro(filename, lines, val);
	else if (strcmp(var, "cbind") == 0) {
	    config.keys = Realloc(config.keys, (k + 2) *
		    sizeof(struct config_key_s *));
	    config.keys[k] = Calloc(1, sizeof(struct config_key_s));
	    p = val;
	    n = 0;

	    while (*p && !isspace(*p))
		p++, n++;

	    c = *p;
	    *p = 0;
	    p -= n;

	    if (strcasecmp(p, "none") == 0)
		config.keys[k]->type = KEY_DEFAULT;
	    else if (strcasecmp(p, "repeat") == 0)
		config.keys[k]->type = KEY_REPEAT;
	    else if (strcasecmp(p, "set") == 0)
		config.keys[k]->type = KEY_SET;
	    else
	      errx(EXIT_FAILURE, _ ("%s(%i): invalid value \"%s\""), filename,
			lines, p);

	    p = val + n;
	    *p = c;

	    while (*p && isspace(*p))
		p++;

	    config.keys[k]->c = *p++;

	    while (isspace(*p))
		p++;

	    config.keys[k++]->str = str_to_wchar (p);
	    config.keys[k] = NULL;
	}
	else
	  errx(EXIT_FAILURE, _ ("%s(%i): invalid parameter \"%s\""), filename,
	       lines, var);
    }

    fclose(fp);

    if (altengine) {
	free(config.engine_cmd);
	config.engine_cmd = NULL;
	config.engine_cmd = altengine;
    }

    if (config.enginecmdblacktag)
	pgn_tag_add(&config.tag, "Black", config.engine_cmd);
}
