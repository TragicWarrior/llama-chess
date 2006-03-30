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
#include <string.h>
#include <err.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#endif

#include "chess.h"
#include "conf.h"
#include "misc.h"
#include "colors.h"
#include "rcfile.h"

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

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
	    errx(EXIT_FAILURE, "%s(%i): invalid attribute \"%s\"", filename, 
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
    else
	errx(EXIT_FAILURE, "%s(%i): invalid color \"%s\"", filename, line, 
		color);

    return -1;
}

static void parse_color(const char *filename, int line, const char *str,
	struct color_s *c)
{
    char fg[16], bg[16], attr[64], nattr[64];
    struct color_s ctmp = *c;
    int n;
    
    if ((n = sscanf(str, "%[a-zA-Z] %[a-zA-Z] %[a-zA-Z,] %[a-zA-Z,]", fg, bg,
		    attr, nattr)) < 2)
	errx(EXIT_FAILURE, "%s(%i): parse error", filename, line);

    ctmp.fg = color_name(filename, line, fg);
    ctmp.bg = color_name(filename, line, bg);
    ctmp.attrs = ctmp.nattrs = 0;

    if (n > 2)
	ctmp.attrs = attributes(filename, line, attr);

    if (n > 3)
	ctmp.nattrs = attributes(filename, line, nattr);

    *c = ctmp;
    return;
}

static int on_or_off(const char *filename, int lines, const char *str)
{
    if (strcmp(str, "on") == 0 || strcmp(str, "1") == 0 || 
	    strcmp(str, "yes") == 0)
	return 1;

    if (strcmp(str, "off") == 0 || strcmp(str, "0") == 0 ||
	    strcmp(str, "no") == 0)
	return 0;

    errx(EXIT_FAILURE, "%s(%i): invalid value \"%s\"", filename, lines, str);
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
    return;
}

void set_config_defaults()
{
    struct stat st;

    config.engine_cmd = strdup("gnuchess");//FIXME
    config.jumpcount = 5;
    config.clevel = 6;
    config.historyagony = 0;
    config.agony = 1;
    config.linegraphics = 0;
    config.saveprompt = 1;
    config.deleteprompt = 1;
    config.validmoves = 1;
    config.stoponerror = 0;

    set_default_colors();

    if (stat(config.nagfile, &st) == -1) {
	if (errno == ENOENT)
	    copydatafile(config.nagfile, "nag.data");
	else
	    warn("%s", config.nagfile);
    }

    if (stat(config.agonyfile, &st) == -1) {
	if (errno == ENOENT)
	    copydatafile(config.agonyfile, "agony.data");
	else
	    warn("%s", config.agonyfile);
    }

    if (stat(config.ccfile, &st) == -1) {
	if (errno == ENOENT)
	    copydatafile(config.nagfile, "cc.data");
	else
	    warn("%s", config.ccfile);
    }
}

void parse_rcfile(const char *filename)
{
    FILE *fp;
    char *line, buf[LINE_MAX];
    int lines = 0;
    char *altengine = NULL;

    if ((fp = fopen(filename, "r")) == NULL)
	err(EXIT_FAILURE, "%s", filename);

    while ((line = fgets(buf, sizeof(buf), fp)) != NULL) {
	int n;
	char var[30], val[50];
	char token[MAX_PGN_LINE_LEN + 1], value[MAX_PGN_LINE_LEN + 1];
	char *p;

	lines++;
	line = trim(line);

	if (!line[0] || line[0] == '#')
	    continue;

	if ((n = sscanf(line, "%s %[^\n]", var, val)) != 2)
	    errx(EXIT_FAILURE, "%s(%i): parse error %i", filename, lines,n);

	strncpy(val, trim(val), sizeof(val));
	strncpy(var, trim(var), sizeof(var));

	if (strcmp(var, "jumpcount") == 0) {
	    if (!isinteger(val))
		errx(EXIT_FAILURE, "%s(%i): value is not an integer", filename,
			lines);

	    config.jumpcount = atoi(val);
	}
	else if (strcmp(var, "historyagony") == 0)
	    config.historyagony = on_or_off(filename, lines, val);
	else if (strcmp(var, "agony") == 0)
	    config.agony = on_or_off(filename, lines, val);
	else if (strcmp(var, "stoponerror") == 0)
	    config.stoponerror = on_or_off(filename, lines, val);
	else if (strcmp(var, "pgntag") == 0) {
	    if ((n = sscanf(val, "%s %s ", token, value)) < 1 || 
		    n > 2)
		errx(EXIT_FAILURE, "%s(%i): invalid value \"%s\"", filename, 
			lines, val);

	    if (n == 1)
		value[0] = 0;
	    else {
		p = val + strlen(token);
		strncpy(value, p, sizeof(value));
	    }

	    for (n = 0; n < strlen(token); n++) {
		if (!isalnum(token[n]) && token[n] != '_')
		    errx(EXIT_FAILURE, 
			    "%s(%i): token names must match 0-9A-Za-z_.",
			    filename, lines);
	    }

	    token[0] = toupper(token[0]);
	    pgn_add_tag(&config.tag, token, value);
	}
	else if (strcmp(var, "board_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BDWINDOW]);
	else if (strcmp(var, "board_selected") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BSELECTED]);
	else if (strcmp(var, "board_white_moves") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BMOVESW]);
	else if (strcmp(var, "board_black_moves") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BMOVESB]);
	else if (strcmp(var, "board_count") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BCOUNT]);
	else if (strcmp(var, "board_cursor") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BCURSOR]);
	else if (strcmp(var, "board_black") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BBLACK]);
	else if (strcmp(var, "board_white") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BWHITE]);
	else if (strcmp(var, "board_graphics") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BGRAPHICS]);
	else if (strcmp(var, "board_coords") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_BCOORDS]);
	else if (strcmp(var, "status_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SWINDOW]);
	else if (strcmp(var, "status_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_STITLE]);
	else if (strcmp(var, "status_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SBORDER]);
	else if (strcmp(var, "status_notify") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SNOTIFY]);
	else if (strcmp(var, "status_engine") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_SENGINE]);
	else if (strcmp(var, "tag_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_TWINDOW]);
	else if (strcmp(var, "tag_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_TTITLE]);
	else if (strcmp(var, "tag_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_TBORDER]);
	else if (strcmp(var, "history_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_HWINDOW]);
	else if (strcmp(var, "history_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_HTITLE]);
	else if (strcmp(var, "history_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_HBORDER]);
	else if (strcmp(var, "message_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MWINDOW]);
	else if (strcmp(var, "message_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MTITLE]);
	else if (strcmp(var, "message_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MBORDER]);
	else if (strcmp(var, "message_prompt") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_MPROMPT]);
	else if (strcmp(var, "input_window") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_IWINDOW]);
	else if (strcmp(var, "input_title") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_ITITLE]);
	else if (strcmp(var, "input_border") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_IBORDER]);
	else if (strcmp(var, "input_prompt") == 0)
	    parse_color(filename, lines, val, &config.color[CONF_IPROMPT]);
	else if (strcmp(var, "save_directory") == 0)
	    config.savedirectory = strdup(tilde_expand(val));
	else if (strcmp(var, "line_graphics") == 0)
	    config.linegraphics = on_or_off(filename, lines, val);
	else if (strcmp(var, "save_prompt") == 0)
	    config.saveprompt = on_or_off(filename, lines, val);
	else if (strcmp(var, "delete_prompt") == 0)
	    config.deleteprompt = on_or_off(filename, lines, val);
	else if (strcmp(var, "valid_moves") == 0)
	    config.validmoves = on_or_off(filename, lines, val);
	else if (strcmp(var, "clevel") == 0) {
	    if (!isinteger(val))
		errx(EXIT_FAILURE, "%s(%i): value is not an integer", filename,
			lines);

	    if ((config.clevel = atoi(val)) > 9 || config.clevel < 1)
		errx(EXIT_FAILURE, "%s(%i): value must be between 1 and 9", 
			filename, lines);
	}
	else if (strcmp(var, "engine_cmd") == 0)
	    altengine = strdup(val);
	else
	    errx(EXIT_FAILURE, "%s(%i): invalid parameter \"%s\"", filename,
		    lines, var);
    }

    fclose(fp);

    if (altengine)
	config.engine_cmd = altengine;

    return;
}
