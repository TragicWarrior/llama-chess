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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>

#include "common.h"
#include "conf.h"
#include "colors.h"

static int next_cp;

/*
 * Looks for a matching color pair or creates a new color pair if not found.
 */
static chtype
find_cp (short fg, short bg, attr_t attrs)
{
  short i;
  short xfg, xbg;

  for (i = 1; i < next_cp; i++)
    {
      pair_content (i, &xfg, &xbg);

      if (xfg == fg && xbg == bg)
	return COLOR_PAIR (i) | attrs;
    }

  init_pair (next_cp, fg, bg);
  return COLOR_PAIR (next_cp++) | attrs;
}

/*
 * Mixes two color pairs' fg and bg colors determined by 'which'.
 */
chtype
mix_cp (chtype a, chtype b, attr_t attrs, int which)
{
  short afg, abg;
  short bfg, bbg;

  pair_content (PAIR_NUMBER (a), &afg, &abg);
  pair_content (PAIR_NUMBER (b), &bfg, &bbg);

  switch (which)
    {
    case A_FG_B_BG:
      return find_cp (afg, bbg, attrs);
    case A_FG_B_FG:
      return find_cp (afg, bfg, attrs);
    case A_BG_B_BG:
      return find_cp (abg, bbg, attrs);
    case B_FG_A_BG:
      return find_cp (bfg, abg, attrs);
    case B_BG_B_FG:
      return find_cp (bbg, bfg, attrs);
    case A_BG_A_FG:
      return find_cp (abg, afg, attrs);
    case B_BG_A_FG:
      return find_cp (bbg, afg, attrs);
    }

  return 0;
}

void
init_color_pairs ()
{
  next_cp = 1;

  init_pair (next_cp++, config.color[CONF_BCOORDS].fg,
	     config.color[CONF_BCOORDS].bg);
  init_pair (next_cp++, config.color[CONF_BGRAPHICS].fg,
	     config.color[CONF_BGRAPHICS].bg);
  init_pair (next_cp++, config.color[CONF_BWHITE].fg,
	     config.color[CONF_BWHITE].bg);
  init_pair (next_cp++, config.color[CONF_BBLACK].fg,
	     config.color[CONF_BBLACK].bg);
  init_pair (next_cp++, config.color[CONF_BSELECTED].fg,
	     config.color[CONF_BSELECTED].bg);
  init_pair (next_cp++, config.color[CONF_BCURSOR].fg,
	     config.color[CONF_BCURSOR].bg);
  init_pair (next_cp++, config.color[CONF_SWINDOW].fg,
	     config.color[CONF_SWINDOW].bg);
  init_pair (next_cp++, config.color[CONF_SBORDER].fg,
	     config.color[CONF_SBORDER].bg);
  init_pair (next_cp++, config.color[CONF_STITLE].fg,
	     config.color[CONF_STITLE].bg);
  init_pair (next_cp++, config.color[CONF_SENGINE].fg,
	     config.color[CONF_SENGINE].bg);
  init_pair (next_cp++, config.color[CONF_SNOTIFY].fg,
	     config.color[CONF_SNOTIFY].bg);
  init_pair (next_cp++, config.color[CONF_TWINDOW].fg,
	     config.color[CONF_TWINDOW].bg);
  init_pair (next_cp++, config.color[CONF_TBORDER].fg,
	     config.color[CONF_TBORDER].bg);
  init_pair (next_cp++, config.color[CONF_TTITLE].fg,
	     config.color[CONF_TTITLE].bg);
  init_pair (next_cp++, config.color[CONF_HWINDOW].fg,
	     config.color[CONF_HWINDOW].bg);
  init_pair (next_cp++, config.color[CONF_HBORDER].fg,
	     config.color[CONF_HBORDER].bg);
  init_pair (next_cp++, config.color[CONF_HTITLE].fg,
	     config.color[CONF_HTITLE].bg);
  init_pair (next_cp++, config.color[CONF_MWINDOW].fg,
	     config.color[CONF_MWINDOW].bg);
  init_pair (next_cp++, config.color[CONF_MBORDER].fg,
	     config.color[CONF_MBORDER].bg);
  init_pair (next_cp++, config.color[CONF_MTITLE].fg,
	     config.color[CONF_MTITLE].bg);
  init_pair (next_cp++, config.color[CONF_MPROMPT].fg,
	     config.color[CONF_MPROMPT].bg);
  init_pair (next_cp++, config.color[CONF_IWINDOW].fg,
	     config.color[CONF_IWINDOW].bg);
  init_pair (next_cp++, config.color[CONF_IBORDER].fg,
	     config.color[CONF_IBORDER].bg);
  init_pair (next_cp++, config.color[CONF_ITITLE].fg,
	     config.color[CONF_ITITLE].bg);
  init_pair (next_cp++, config.color[CONF_IPROMPT].fg,
	     config.color[CONF_IPROMPT].bg);
  init_pair (next_cp++, config.color[CONF_BMOVESW].fg,
	     config.color[CONF_BMOVESW].bg);
  init_pair (next_cp++, config.color[CONF_BMOVESB].fg,
	     config.color[CONF_BMOVESB].bg);
  init_pair (next_cp++, config.color[CONF_BCOUNT].fg,
	     config.color[CONF_BCOUNT].bg);
  init_pair (next_cp++, config.color[CONF_BDWINDOW].fg,
	     config.color[CONF_BDWINDOW].bg);
  init_pair (next_cp++, config.color[CONF_MENU].fg,
	     config.color[CONF_MENU].bg);
  init_pair (next_cp++, config.color[CONF_MENUS].fg,
	     config.color[CONF_MENUS].bg);
  init_pair (next_cp++, config.color[CONF_MENUH].fg,
	     config.color[CONF_MENUH].bg);
  init_pair (next_cp++, config.color[CONF_HISTORY_MENU_LG].fg,
	     config.color[CONF_HISTORY_MENU_LG].bg);

  /*
   * These are not configurable. They are the color pairs that are mixed
   * with the background of the current square and foreground of the current
   * piece. See draw_board() for details.
   */
  init_pair (next_cp++, config.color[CONF_BWHITE].fg,
	     config.color[CONF_BWHITE].bg);
  init_pair (next_cp++, config.color[CONF_BBLACK].fg,
	     config.color[CONF_BWHITE].bg);
  init_pair (next_cp++, config.color[CONF_BBLACK].fg,
	     config.color[CONF_BBLACK].bg);
  init_pair (next_cp++, config.color[CONF_BWHITE].fg,
	     config.color[CONF_BBLACK].bg);

  init_pair (next_cp++, config.color[CONF_BCASTLING].fg,
	     config.color[CONF_BCASTLING].bg);
  init_pair (next_cp++, config.color[CONF_BENPASSANT].fg,
	     config.color[CONF_BENPASSANT].bg);
  init_pair (next_cp++, config.color[CONF_BATTACK].fg,
	     config.color[CONF_BATTACK].bg);
  init_pair (next_cp++, config.color[CONF_BPREVMOVE].fg,
	     config.color[CONF_BPREVMOVE].bg);
}

void
set_default_colors ()
{
  config.color[CONF_BDWINDOW].fg = COLOR_WHITE;
  config.color[CONF_BDWINDOW].bg = COLOR_BLACK;
  config.color[CONF_BCOORDS].fg = COLOR_YELLOW;
  config.color[CONF_BCOORDS].bg = COLOR_BLACK;
  config.color[CONF_BMOVESW].fg = COLOR_WHITE;
  config.color[CONF_BMOVESW].bg = COLOR_WHITE;
  config.color[CONF_BMOVESW].nattrs = A_REVERSE;
  config.color[CONF_BMOVESB].fg = COLOR_WHITE;
  config.color[CONF_BMOVESB].bg = COLOR_BLUE;
  config.color[CONF_BMOVESB].nattrs = A_REVERSE;
  config.color[CONF_BCOUNT].fg = COLOR_MAGENTA;
  config.color[CONF_BCOUNT].bg = COLOR_CYAN;
  config.color[CONF_BCOUNT].attrs = A_BOLD;
  config.color[CONF_BCOUNT].nattrs = A_REVERSE;
  config.color[CONF_BGRAPHICS].fg = COLOR_WHITE;
  config.color[CONF_BGRAPHICS].bg = COLOR_BLACK;
  config.color[CONF_BWHITE].fg = COLOR_WHITE;
  config.color[CONF_BWHITE].bg = COLOR_RED;
  config.color[CONF_BWHITE].nattrs = A_REVERSE;
  config.color[CONF_BBLACK].fg = COLOR_CYAN;
  config.color[CONF_BBLACK].bg = COLOR_BLACK;
  config.color[CONF_BSELECTED].fg = COLOR_WHITE;
  config.color[CONF_BSELECTED].bg = COLOR_YELLOW;
  config.color[CONF_BSELECTED].nattrs = A_BOLD | A_REVERSE;
  config.color[CONF_BCURSOR].fg = COLOR_WHITE;
  config.color[CONF_BCURSOR].bg = COLOR_GREEN;
  config.color[CONF_BCURSOR].nattrs = A_BOLD | A_REVERSE;
  config.color[CONF_SWINDOW].fg = COLOR_WHITE;
  config.color[CONF_SWINDOW].bg = COLOR_BLACK;
  config.color[CONF_SBORDER].fg = COLOR_CYAN;
  config.color[CONF_SBORDER].bg = COLOR_BLACK;
  config.color[CONF_STITLE].fg = COLOR_WHITE;
  config.color[CONF_STITLE].bg = COLOR_BLUE;
  config.color[CONF_STITLE].nattrs = A_REVERSE;
  config.color[CONF_SENGINE].fg = COLOR_YELLOW;
  config.color[CONF_SENGINE].bg = COLOR_BLACK;
  config.color[CONF_SENGINE].nattrs = A_BOLD;
  config.color[CONF_SNOTIFY].fg = COLOR_GREEN;
  config.color[CONF_SNOTIFY].bg = COLOR_BLACK;
  config.color[CONF_SNOTIFY].nattrs = A_BOLD;
  config.color[CONF_TWINDOW].fg = COLOR_WHITE;
  config.color[CONF_TWINDOW].bg = COLOR_BLACK;
  config.color[CONF_TBORDER].fg = COLOR_CYAN;
  config.color[CONF_TBORDER].bg = COLOR_BLACK;
  config.color[CONF_TTITLE].fg = COLOR_WHITE;
  config.color[CONF_TTITLE].bg = COLOR_BLUE;
  config.color[CONF_TTITLE].nattrs = A_REVERSE;
  config.color[CONF_HWINDOW].fg = COLOR_WHITE;
  config.color[CONF_HWINDOW].bg = COLOR_BLACK;
  config.color[CONF_HBORDER].fg = COLOR_CYAN;
  config.color[CONF_HBORDER].bg = COLOR_BLACK;
  config.color[CONF_HTITLE].fg = COLOR_WHITE;
  config.color[CONF_HTITLE].bg = COLOR_BLUE;
  config.color[CONF_HTITLE].nattrs = A_REVERSE;
  config.color[CONF_MWINDOW].fg = COLOR_WHITE;
  config.color[CONF_MWINDOW].bg = COLOR_BLACK;
  config.color[CONF_MBORDER].fg = COLOR_CYAN;
  config.color[CONF_MBORDER].bg = COLOR_BLACK;
  config.color[CONF_MTITLE].fg = COLOR_WHITE;
  config.color[CONF_MTITLE].bg = COLOR_MAGENTA;
  config.color[CONF_MTITLE].nattrs = A_REVERSE;
  config.color[CONF_MPROMPT].fg = COLOR_WHITE;
  config.color[CONF_MPROMPT].bg = COLOR_MAGENTA;
  config.color[CONF_MPROMPT].nattrs = A_BOLD;
  config.color[CONF_IWINDOW].fg = COLOR_WHITE;
  config.color[CONF_IWINDOW].bg = COLOR_BLACK;
  config.color[CONF_IBORDER].fg = COLOR_CYAN;
  config.color[CONF_IBORDER].bg = COLOR_BLACK;
  config.color[CONF_ITITLE].fg = COLOR_WHITE;
  config.color[CONF_ITITLE].bg = COLOR_MAGENTA;
  config.color[CONF_ITITLE].nattrs = A_REVERSE;
  config.color[CONF_IPROMPT].fg = COLOR_WHITE;
  config.color[CONF_IPROMPT].bg = COLOR_MAGENTA;
  config.color[CONF_IPROMPT].nattrs = A_BOLD;
  config.color[CONF_MENU].fg = COLOR_WHITE;
  config.color[CONF_MENU].bg = COLOR_BLACK;
  config.color[CONF_MENUS].fg = COLOR_WHITE;
  config.color[CONF_MENUS].bg = COLOR_RED;
  config.color[CONF_MENUS].nattrs = A_BOLD;
  config.color[CONF_MENUH].fg = COLOR_BLUE;
  config.color[CONF_MENUH].bg = COLOR_YELLOW;
  config.color[CONF_MENUH].nattrs = A_BOLD;
  config.color[CONF_HISTORY_MENU_LG].fg = COLOR_GREEN;
  config.color[CONF_HISTORY_MENU_LG].bg = COLOR_BLACK;
  config.color[CONF_BCASTLING].fg = COLOR_YELLOW;
  config.color[CONF_BCASTLING].bg = COLOR_BLACK;
  config.color[CONF_BCASTLING].attrs = A_BOLD;
  config.color[CONF_BCASTLING].nattrs = A_BOLD;
  config.color[CONF_BENPASSANT].fg = COLOR_MAGENTA;
  config.color[CONF_BENPASSANT].bg = COLOR_BLACK;
  config.color[CONF_BENPASSANT].attrs = A_BOLD;
  config.color[CONF_BENPASSANT].nattrs = A_BOLD;
  config.color[CONF_BATTACK].fg = COLOR_BLUE;
  config.color[CONF_BATTACK].bg = COLOR_BLACK;
  config.color[CONF_BATTACK].attrs = A_BOLD | A_BLINK;
  config.color[CONF_BATTACK].nattrs = A_REVERSE;
  config.color[CONF_BPREVMOVE].fg = COLOR_WHITE;
  config.color[CONF_BPREVMOVE].bg = COLOR_MAGENTA;
  config.color[CONF_BPREVMOVE].nattrs = A_BOLD | A_REVERSE;
}
