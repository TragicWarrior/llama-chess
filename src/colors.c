/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    Copyright (C) 2002-2024 Ben Kibbey <bjk@luxsci.net>

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

#include <vdk.h>

#include "common.h"
#include "conf.h"
#include "colors.h"

/*
 * Mixes two color pairs' fg and bg colors determined by 'which'.
 * Resolves the result via VDK's pair matrix (same as CP_* macros).
 */
chtype
mix_cp(chtype a, chtype b, attr_t attrs, int which)
{
    short afg, abg;
    short bfg, bbg;
    short fg = 0, bg = 0;

    if (!COLORS)
        return attrs;

    pair_content(PAIR_NUMBER(a), &afg, &abg);
    pair_content(PAIR_NUMBER(b), &bfg, &bbg);

    switch (which)
    {
    case A_FG_B_BG:
        fg = afg;
        bg = bbg;
        break;
    case A_FG_B_FG:
        fg = afg;
        bg = bfg;
        break;
    case A_BG_B_BG:
        fg = abg;
        bg = bbg;
        break;
    case B_FG_A_BG:
        fg = bfg;
        bg = abg;
        break;
    case B_BG_B_FG:
        fg = bbg;
        bg = bfg;
        break;
    case A_BG_A_FG:
        fg = abg;
        bg = afg;
        break;
    case B_BG_A_FG:
        fg = bbg;
        bg = afg;
        break;
    default:
        return 0;
    }

    return COLOR_PAIR(vdk_color_pair(fg, bg)) | attrs;
}

void set_default_colors()
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
    /* Bold white → bright white (plain COLOR_WHITE is dim/grey on many terms). */
    config.color[CONF_BWHITE].fg = COLOR_WHITE;
    config.color[CONF_BWHITE].bg = COLOR_RED;
    config.color[CONF_BWHITE].attrs = A_BOLD;
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
    /* Menus: bright white on cyan; border uses the same pair with bold. */
    config.color[CONF_MENU].fg = COLOR_WHITE;
    config.color[CONF_MENU].bg = COLOR_CYAN;
    config.color[CONF_MENU].attrs = A_BOLD;
    config.color[CONF_MENU].nattrs = A_BOLD;
    /* Selected row: reverse so it stands out on the cyan body. */
    config.color[CONF_MENUS].fg = COLOR_CYAN;
    config.color[CONF_MENUS].bg = COLOR_WHITE;
    config.color[CONF_MENUS].attrs = A_BOLD;
    config.color[CONF_MENUS].nattrs = A_BOLD | A_REVERSE;
    /* Menubar focused item highlight. */
    config.color[CONF_MENUH].fg = COLOR_WHITE;
    config.color[CONF_MENUH].bg = COLOR_BLUE;
    config.color[CONF_MENUH].attrs = A_BOLD;
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
