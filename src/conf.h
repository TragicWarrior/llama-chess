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
#ifndef CONFIG_H
#define CONFIG_H

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

// See init_color_pairs() in colors.c.
enum { 
    CONF_BWHITE, CONF_BBLACK, CONF_BSELECTED, CONF_BCURSOR, CONF_BGRAPHICS,
    CONF_BCOORDS, CONF_BMOVESW, CONF_BMOVESB, CONF_BCOUNT, CONF_BDWINDOW,
    CONF_TWINDOW, CONF_TTITLE, CONF_TBORDER,
    CONF_SWINDOW, CONF_STITLE, CONF_SBORDER, CONF_SNOTIFY, CONF_SENGINE,
    CONF_HWINDOW, CONF_HTITLE, CONF_HBORDER,
    CONF_MWINDOW, CONF_MTITLE, CONF_MBORDER, CONF_MPROMPT,
    CONF_IWINDOW, CONF_ITITLE, CONF_IBORDER, CONF_IPROMPT, CONF_MENU, 
    CONF_MENUS, CONF_MENUH, CONF_MAX_COLORS
};

struct color_s {
    short fg;		// Foreground color.
    short bg;		// Background color.
    int attrs;		// Attributes for a color terminal.
    int nattrs;		// Attributes for a non-color terminal.
};

struct key_s {
    int c;
    char *str;
};

struct {
    int jumpcount;	// KEY_UP and KEY_DOWN in history mode.
    int historyagony;	// Whether to display agony strings on capture when in 
    			// history mode.
    int agony;		// Whether to display agony anywhere.
    int linegraphics;	// Board line graphics.
    int saveprompt;	// Prompt to save modified games on quit. FIXME
    int deleteprompt;	// Prompt when deleting a game.
    int clevel;		// Compression level for compressed files.
    int validmoves;	// Display valid squares a selected piece can move to.
    struct passwd *pwd;	// Used throughout (tags/home directory).
    char *nagfile;	// The pathname to the NAG data file.
    char *agonyfile;	// The pathname to the agony data file.
    char *configfile;	// The pathname to the configuration file (default or
    			// from the command line).
    char *ccfile;	// The pathname to the Country Code data file.
    char *savedirectory; // Directory where saved games are stored.
    char *engine_cmd;	// Alternate chess engine command. FIXME
    int engine;		// FIXME
    struct color_s color[CONF_MAX_COLORS]; // Color configuration.
    TAG **tag;		// Custom PGN tags.
    char *pattern;	// Filename filter in the file browser.
    char **einit;	/* Strings to send to the chess engine upon each reset
                         * or new game.
			 */
    struct key_s **keys; // Custom commands to send to the engine.
} config;

#endif
