/* $Id: colors.h,v 1.2 2002-12-27 14:36:01 bjk Exp $ */
/*
    Copyright (C) 2002 Ben Kibbey <bjk@arbornet.org>

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

#define CP_WHITE_WINDOW	((COLORS) ? \
	COLOR_PAIR(12) | config.color[CONF_WWINDOW].attrs : \
	config.color[CONF_WWINDOW].nattrs)

#define CP_WHITE_BORDER	((COLORS) ? \
	COLOR_PAIR(13) | config.color[CONF_WBORDER].attrs : \
	config.color[CONF_WBORDER].nattrs)

#define CP_WHITE_TITLE	((COLORS) ? \
	COLOR_PAIR(14) | config.color[CONF_WTITLE].attrs : \
	config.color[CONF_WTITLE].nattrs)

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

#define CP_BLACK_WINDOW	((COLORS) ? \
	COLOR_PAIR(12) | config.color[CONF_BWINDOW].attrs : \
	config.color[CONF_BWINDOW].nattrs)

#define CP_BLACK_BORDER	((COLORS) ? \
	COLOR_PAIR(13) | config.color[CONF_BBORDER].attrs : \
	config.color[CONF_BBORDER].nattrs)

#define CP_BLACK_TITLE	((COLORS) ? \
	COLOR_PAIR(14) | config.color[CONF_BTITLE].attrs : \
	config.color[CONF_BTITLE].nattrs)

#endif
