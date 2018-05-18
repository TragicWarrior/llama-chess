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
#ifndef COMMON_H
#define COMMON_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <wchar.h>

#if defined(HAVE_NCURSESW_CURSES_H)
#include <ncursesw/curses.h>
#elif defined(HAVE_NCURSESW_H)
#include <ncursesw.h>
#elif defined(HAVE_NCURSES_CURSES_H)
#include <ncurses/curses.h>
#elif defined(HAVE_NCURSES_H)
#include <ncurses.h>
#elif defined(HAVE_CURSES_H)
#include <curses.h>
#else
#error "SysV or X/Open-compatible Curses header file required"
#endif

#if defined(HAVE_NCURSESW_PANEL_H)
#include <ncursesw/panel.h>
#elif defined(HAVE_NCURSES_PANEL_H)
#include <ncurses/panel.h>
#elif defined(HAVE_PANEL_H)
#include <panel.h>
#else
#error "SysV-compatible Curses Panel header file required"
#endif

#if defined(HAVE_NCURSESW_MENU_H)
#include <ncursesw/menu.h>
#elif defined(HAVE_NCURSES_MENU_H)
#include <ncurses/menu.h>
#elif defined(HAVE_MENU_H)
#include <menu.h>
#else
#error "SysV-compatible Curses Menu header file required"
#endif

#if defined(HAVE_NCURSESW_FORM_H)
#include <ncursesw/form.h>
#elif defined(HAVE_NCURSES_FORM_H)
#include <ncurses/form.h>
#elif defined(HAVE_FORM_H)
#include <form.h>
#else
#error "SysV-compatible Curses Form header file required"
#endif

#ifndef _
#include "gettext.h"
#define _(msgid) gettext(msgid)
#endif

#include "chess.h"

#define CF_ENGINE_LOOP	0x01
#define CF_HUMAN	0x02
#define CF_NEW		0x04
#define CF_CLOCK	0x08
#define CF_MODIFIED	0x10
#define CF_DELETE	0x20
#ifdef WITH_LIBPERL
#define CF_PERL		0x40
#endif

#include <sys/time.h>
#define MAX_TC		8	/* Time controls. */

#define ANY_KEY_STR             _("[ press any key to continue ]")
#define ANY_KEY_SCROLL_STR      _("[ press any key to continue (UP/DN to scroll) ]")
#define ERROR_STR               _("[ ERROR ]")

struct clock_s
{
  struct timeval elapsed;
  unsigned short move;		/* move count */
  int tc[MAX_TC][2];		/* 0 = move count, 1 = time (in seconds) */
  int tcn;
  int incr;
};

/*
 * Attached to game[n].data.
 */
struct userdata_s
{
  BOARD b;
  struct engine_s *engine;
  unsigned short flags;
  char c_row;
  char c_col;
  char pm_frfr[6];		// Previous move
  char pm_row;
  char pm_col;
  char ospm_row;
  char ospm_col;
  char pm_undo;
  char paused;
  unsigned n;
  unsigned char mode;
  char rotate;			// Rotation control board
  int go_move;			// Movement function result 'do_play_go'
  int play_mode;
  struct clock_s wclock;
  struct clock_s bclock;
  struct timeval elapsed;

  // The selected piece.
  struct
  {
    unsigned char icon;
    char scol;
    char srow;
    char col;
    char row;
  } sp;

  void *data;			// For the history menu

#ifdef WITH_LIBPERL
  char *perlfen;
  char *oldfen;
  unsigned short perlflags;
#endif
};

/* A pointer to the game in focus. */
GAME gp;

void gameover (GAME);
void update_cursor (GAME, int);
void invalid_move (int n, int e, const char *m);
void update_status_window (GAME g);
void update_all (GAME g);
void update_status_notify (GAME g, const char *fmt, ...);
void update_tag_window (TAG ** tags);
void update_all (GAME g);
void edit_tags (GAME g, BOARD b, int edit);
void add_custom_tags (TAG *** t);
wchar_t *translate_tag_name (const char *);

#endif
