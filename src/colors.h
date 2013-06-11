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
#ifndef COLORS_H
#define COLORS_H

enum {
    A_FG_B_BG, A_FG_B_FG, A_BG_B_BG, B_FG_A_BG, B_BG_B_FG, A_BG_A_FG,
    A_BG_B_FG, B_BG_A_FG
};

#define CP_BOARD_COORDS	((COLORS) ? \
	COLOR_PAIR(1) | config.color[CONF_BCOORDS].attrs : \
	config.color[CONF_BCOORDS].nattrs)

#define CP_BOARD_GRAPHICS	((COLORS) ? \
	COLOR_PAIR(2) | config.color[CONF_BGRAPHICS].attrs : \
	config.color[CONF_BGRAPHICS].nattrs)

#define CP_BOARD_WHITE	((COLORS) ? \
	COLOR_PAIR(3) | config.color[CONF_BWHITE].attrs : \
	config.color[CONF_BWHITE].nattrs)

#define CP_BOARD_BLACK	((COLORS) ? \
	COLOR_PAIR(4) | config.color[CONF_BBLACK].attrs : \
	config.color[CONF_BBLACK].nattrs)

#define CP_BOARD_SELECTED	((COLORS) ? \
	COLOR_PAIR(5) | config.color[CONF_BSELECTED].attrs : \
	config.color[CONF_BSELECTED].nattrs)

#define CP_BOARD_CURSOR	((COLORS) ? \
	COLOR_PAIR(6) | config.color[CONF_BCURSOR].attrs : \
	config.color[CONF_BCURSOR].nattrs)

#define CP_STATUS_WINDOW	((COLORS) ? \
	COLOR_PAIR(7) | config.color[CONF_SWINDOW].attrs : \
	config.color[CONF_SWINDOW].nattrs)

#define CP_STATUS_BORDER	((COLORS) ? \
	COLOR_PAIR(8) | config.color[CONF_SBORDER].attrs : \
	config.color[CONF_SBORDER].nattrs)

#define CP_STATUS_TITLE	((COLORS) ? \
	COLOR_PAIR(9) | config.color[CONF_STITLE].attrs : \
	config.color[CONF_STITLE].nattrs)

#define CP_STATUS_ENGINE	((COLORS) ? \
	COLOR_PAIR(10) | config.color[CONF_SENGINE].attrs : \
	config.color[CONF_SENGINE].nattrs)

#define CP_STATUS_NOTIFY	((COLORS) ? \
	COLOR_PAIR(11) | config.color[CONF_SNOTIFY].attrs : \
	config.color[CONF_SNOTIFY].nattrs)

#define CP_TAG_WINDOW	((COLORS) ? \
	COLOR_PAIR(12) | config.color[CONF_TWINDOW].attrs : \
	config.color[CONF_TWINDOW].nattrs)

#define CP_TAG_BORDER	((COLORS) ? \
	COLOR_PAIR(13) | config.color[CONF_TBORDER].attrs : \
	config.color[CONF_TBORDER].nattrs)

#define CP_TAG_TITLE	((COLORS) ? \
	COLOR_PAIR(14) | config.color[CONF_TTITLE].attrs : \
	config.color[CONF_TTITLE].nattrs)

#define CP_HISTORY_WINDOW	((COLORS) ? \
	COLOR_PAIR(15) | config.color[CONF_HWINDOW].attrs : \
	config.color[CONF_HWINDOW].nattrs)

#define CP_HISTORY_BORDER	((COLORS) ? \
	COLOR_PAIR(16) | config.color[CONF_HBORDER].attrs : \
	config.color[CONF_HBORDER].nattrs)

#define CP_HISTORY_TITLE	((COLORS) ? \
	COLOR_PAIR(17) | config.color[CONF_HTITLE].attrs : \
	config.color[CONF_HTITLE].nattrs)

#define CP_MESSAGE_WINDOW	((COLORS) ? \
	COLOR_PAIR(18) | config.color[CONF_MWINDOW].attrs : \
	config.color[CONF_MWINDOW].nattrs)

#define CP_MESSAGE_BORDER	((COLORS) ? \
	COLOR_PAIR(19) | config.color[CONF_MBORDER].attrs : \
	config.color[CONF_MBORDER].nattrs)

#define CP_MESSAGE_TITLE	((COLORS) ? \
	COLOR_PAIR(20) | config.color[CONF_MTITLE].attrs : \
	config.color[CONF_MTITLE].nattrs)

#define CP_MESSAGE_PROMPT	((COLORS) ? \
	COLOR_PAIR(21) | config.color[CONF_MPROMPT].attrs : \
	config.color[CONF_MPROMPT].nattrs)

#define CP_INPUT_WINDOW	((COLORS) ? \
	COLOR_PAIR(22) | config.color[CONF_IWINDOW].attrs : \
	config.color[CONF_IWINDOW].nattrs)

#define CP_INPUT_BORDER	((COLORS) ? \
	COLOR_PAIR(23) | config.color[CONF_IBORDER].attrs : \
	config.color[CONF_IBORDER].nattrs)

#define CP_INPUT_TITLE	((COLORS) ? \
	COLOR_PAIR(24) | config.color[CONF_ITITLE].attrs : \
	config.color[CONF_ITITLE].nattrs)

#define CP_INPUT_PROMPT	((COLORS) ? \
	COLOR_PAIR(25) | config.color[CONF_IPROMPT].attrs : \
	config.color[CONF_IPROMPT].nattrs)

#define CP_BOARD_MOVES_WHITE	((COLORS) ? \
	COLOR_PAIR(26) | config.color[CONF_BMOVESW].attrs : \
	config.color[CONF_BMOVESW].nattrs)

#define CP_BOARD_MOVES_BLACK	((COLORS) ? \
	COLOR_PAIR(27) | config.color[CONF_BMOVESB].attrs : \
	config.color[CONF_BMOVESB].nattrs)

#define CP_BOARD_COUNT	((COLORS) ? \
	COLOR_PAIR(28) | config.color[CONF_BCOUNT].attrs : \
	config.color[CONF_BCOUNT].nattrs)

#define CP_BOARD_WINDOW	((COLORS) ? \
	COLOR_PAIR(29) | config.color[CONF_BDWINDOW].attrs : \
	config.color[CONF_BDWINDOW].nattrs)

#define CP_MENU	((COLORS) ? \
	COLOR_PAIR(30) | config.color[CONF_MENU].attrs : \
	config.color[CONF_MENU].nattrs)

#define CP_MENU_SELECTED	((COLORS) ? \
	COLOR_PAIR(31) | config.color[CONF_MENUS].attrs : \
	config.color[CONF_MENUS].nattrs)

#define CP_MENU_HIGHLIGHT	((COLORS) ? \
	COLOR_PAIR(32) | config.color[CONF_MENUH].attrs : \
	config.color[CONF_MENUH].nattrs)

#define CP_HISTORY_MENU_LG	((COLORS) ? \
	COLOR_PAIR(33) | config.color[CONF_HISTORY_MENU_LG].attrs : \
	config.color[CONF_HISTORY_MENU_LG].nattrs)

#define CP_BOARD_W_W		((COLORS) ? \
	COLOR_PAIR(34) | config.color[CONF_BWHITE].attrs : \
	config.color[CONF_BWHITE].nattrs)

#define CP_BOARD_W_B		((COLORS) ? \
	COLOR_PAIR(35) | config.color[CONF_BBLACK].attrs : \
	config.color[CONF_BWHITE].nattrs)

#define CP_BOARD_B_B		((COLORS) ? \
	COLOR_PAIR(36) | config.color[CONF_BBLACK].attrs : \
	config.color[CONF_BBLACK].nattrs)

#define CP_BOARD_B_W		((COLORS) ? \
	COLOR_PAIR(37) | config.color[CONF_BWHITE].attrs : \
	config.color[CONF_BBLACK].nattrs)

#define CP_BOARD_CASTLING	((COLORS) ? \
	COLOR_PAIR(38) | config.color[CONF_BCASTLING].attrs : \
	config.color[CONF_BCASTLING].nattrs)

#define CP_BOARD_ENPASSANT	((COLORS) ? \
	COLOR_PAIR(39) | config.color[CONF_BENPASSANT].attrs : \
	config.color[CONF_BENPASSANT].nattrs)

#define CP_BOARD_ATTACK	((COLORS) ? \
	COLOR_PAIR(40) | config.color[CONF_BATTACK].attrs : \
	config.color[CONF_BATTACK].nattrs)

void init_color_pairs();
void set_default_colors();
#ifdef HAVE_ATTR_T
chtype mix_cp(chtype a, chtype b, attr_t, int which);
#else
chtype mix_cp(chtype a, chtype b, int, int which);
#endif

#endif
