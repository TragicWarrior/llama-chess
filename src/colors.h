/* vim:tw=78:ts=8:sw=4:set ft=c:  */
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
#ifndef COLORS_H
#define COLORS_H

/*
 * All color pairs go through VDK (vdk_color_init / vdk_color_pair).
 * cboard only stores logical fg/bg/attrs in config.color[]; it does not
 * own an init_pair table of its own.
 */
#include <vdk.h>

enum
{
  A_FG_B_BG, A_FG_B_FG, A_BG_B_BG, B_FG_A_BG, B_BG_B_FG, A_BG_A_FG,
  A_BG_B_FG, B_BG_A_FG
};

#define CP_CFG(conf) \
	((COLORS) \
	 ? (COLOR_PAIR (vdk_color_pair (config.color[conf].fg, \
					config.color[conf].bg)) \
	    | config.color[conf].attrs) \
	 : config.color[conf].nattrs)

#define CP_BOARD_COORDS		CP_CFG (CONF_BCOORDS)
#define CP_BOARD_GRAPHICS	CP_CFG (CONF_BGRAPHICS)
#define CP_BOARD_WHITE		CP_CFG (CONF_BWHITE)
#define CP_BOARD_BLACK		CP_CFG (CONF_BBLACK)
#define CP_BOARD_SELECTED	CP_CFG (CONF_BSELECTED)
#define CP_BOARD_CURSOR		CP_CFG (CONF_BCURSOR)
#define CP_STATUS_WINDOW	CP_CFG (CONF_SWINDOW)
#define CP_STATUS_BORDER	CP_CFG (CONF_SBORDER)
#define CP_STATUS_TITLE		CP_CFG (CONF_STITLE)
#define CP_STATUS_ENGINE	CP_CFG (CONF_SENGINE)
#define CP_STATUS_NOTIFY	CP_CFG (CONF_SNOTIFY)
#define CP_TAG_WINDOW		CP_CFG (CONF_TWINDOW)
#define CP_TAG_BORDER		CP_CFG (CONF_TBORDER)
#define CP_TAG_TITLE		CP_CFG (CONF_TTITLE)
#define CP_HISTORY_WINDOW	CP_CFG (CONF_HWINDOW)
#define CP_HISTORY_BORDER	CP_CFG (CONF_HBORDER)
#define CP_HISTORY_TITLE	CP_CFG (CONF_HTITLE)
#define CP_MESSAGE_WINDOW	CP_CFG (CONF_MWINDOW)
#define CP_MESSAGE_BORDER	CP_CFG (CONF_MBORDER)
#define CP_MESSAGE_TITLE	CP_CFG (CONF_MTITLE)
#define CP_MESSAGE_PROMPT	CP_CFG (CONF_MPROMPT)
#define CP_INPUT_WINDOW		CP_CFG (CONF_IWINDOW)
#define CP_INPUT_BORDER		CP_CFG (CONF_IBORDER)
#define CP_INPUT_TITLE		CP_CFG (CONF_ITITLE)
#define CP_INPUT_PROMPT		CP_CFG (CONF_IPROMPT)
#define CP_BOARD_MOVES_WHITE	CP_CFG (CONF_BMOVESW)
#define CP_BOARD_MOVES_BLACK	CP_CFG (CONF_BMOVESB)
#define CP_BOARD_COUNT		CP_CFG (CONF_BCOUNT)
#define CP_BOARD_WINDOW		CP_CFG (CONF_BDWINDOW)
#define CP_MENU			CP_CFG (CONF_MENU)
#define CP_MENU_SELECTED	CP_CFG (CONF_MENUS)
#define CP_MENU_HIGHLIGHT	CP_CFG (CONF_MENUH)
#define CP_HISTORY_MENU_LG	CP_CFG (CONF_HISTORY_MENU_LG)
#define CP_BOARD_CASTLING	CP_CFG (CONF_BCASTLING)
#define CP_BOARD_ENPASSANT	CP_CFG (CONF_BENPASSANT)
#define CP_BOARD_ATTACK		CP_CFG (CONF_BATTACK)
#define CP_BOARD_PREVMOVE	CP_CFG (CONF_BPREVMOVE)

/* Piece-on-square mixes (fg of piece, bg of square). */
#define CP_MIX(fgconf, bgconf, attr_src) \
	((COLORS) \
	 ? (COLOR_PAIR (vdk_color_pair (config.color[fgconf].fg, \
					config.color[bgconf].bg)) \
	    | config.color[attr_src].attrs) \
	 : config.color[attr_src].nattrs)

#define CP_BOARD_W_W	CP_MIX (CONF_BWHITE, CONF_BWHITE, CONF_BWHITE)
#define CP_BOARD_W_B	CP_MIX (CONF_BBLACK, CONF_BWHITE, CONF_BBLACK)
#define CP_BOARD_B_B	CP_MIX (CONF_BBLACK, CONF_BBLACK, CONF_BBLACK)
#define CP_BOARD_B_W	CP_MIX (CONF_BWHITE, CONF_BBLACK, CONF_BWHITE)

void set_default_colors (void);
chtype mix_cp (chtype a, chtype b, attr_t attrs, int which);

#endif
