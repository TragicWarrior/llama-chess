/* $Id: colors.c,v 1.6 2003-01-22 20:04:49 bjk Exp $ */
/*
    Copyright (C) 2002-2003 Ben Kibbey <bjk@arbornet.org>

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

#include "common.h"
#include "colors.h"

void init_color_pairs()
{
    init_pair(1, config.color[CONF_BCOORDS].fg, 
	    config.color[CONF_BCOORDS].bg);
    init_pair(2, config.color[CONF_BGRAPHICS].fg, 
	    config.color[CONF_BGRAPHICS].bg);
    init_pair(3, config.color[CONF_BWHITE].fg, 
	    config.color[CONF_BWHITE].bg);
    init_pair(4, config.color[CONF_BBLACK].fg, 
	    config.color[CONF_BBLACK].bg);
    init_pair(5, config.color[CONF_BSELECTED].fg, 
	    config.color[CONF_BSELECTED].bg);
    init_pair(6, config.color[CONF_BCURSOR].fg, 
	    config.color[CONF_BCURSOR].bg);
    init_pair(7, config.color[CONF_SWINDOW].fg, 
	    config.color[CONF_SWINDOW].bg);
    init_pair(8, config.color[CONF_SBORDER].fg, 
	    config.color[CONF_SBORDER].bg);
    init_pair(9, config.color[CONF_STITLE].fg, 
	    config.color[CONF_STITLE].bg);
    init_pair(10, config.color[CONF_SENGINE].fg, 
	    config.color[CONF_SENGINE].bg);
    init_pair(11, config.color[CONF_SNOTIFY].fg, 
	    config.color[CONF_SNOTIFY].bg);
    init_pair(12, config.color[CONF_WWINDOW].fg, 
	    config.color[CONF_WWINDOW].bg);
    init_pair(13, config.color[CONF_WBORDER].fg, 
	    config.color[CONF_WBORDER].bg);
    init_pair(14, config.color[CONF_WTITLE].fg, 
	    config.color[CONF_WTITLE].bg);
    init_pair(15, config.color[CONF_HWINDOW].fg, 
	    config.color[CONF_HWINDOW].bg);
    init_pair(16, config.color[CONF_HBORDER].fg, 
	    config.color[CONF_HBORDER].bg);
    init_pair(17, config.color[CONF_HTITLE].fg, 
	    config.color[CONF_HTITLE].bg);
    init_pair(18, config.color[CONF_MWINDOW].fg, 
	    config.color[CONF_MWINDOW].bg);
    init_pair(19, config.color[CONF_MBORDER].fg, 
	    config.color[CONF_MBORDER].bg);
    init_pair(20, config.color[CONF_MTITLE].fg, 
	    config.color[CONF_MTITLE].bg);
    init_pair(21, config.color[CONF_MPROMPT].fg, 
	    config.color[CONF_MPROMPT].bg);
    init_pair(22, config.color[CONF_IWINDOW].fg, 
	    config.color[CONF_IWINDOW].bg);
    init_pair(23, config.color[CONF_IBORDER].fg, 
	    config.color[CONF_IBORDER].bg);
    init_pair(24, config.color[CONF_ITITLE].fg, 
	    config.color[CONF_ITITLE].bg);
    init_pair(25, config.color[CONF_IPROMPT].fg, 
	    config.color[CONF_IPROMPT].bg);
    init_pair(26, config.color[CONF_BWINDOW].fg, 
	    config.color[CONF_BWINDOW].bg);
    init_pair(27, config.color[CONF_BBORDER].fg, 
	    config.color[CONF_BBORDER].bg);
    init_pair(28, config.color[CONF_BTITLE].fg, 
	    config.color[CONF_BTITLE].bg);
    init_pair(29, config.color[CONF_BMOVES].fg, 
	    config.color[CONF_BMOVES].bg);

    return;
}

void set_default_colors()
{
    config.color[CONF_BCOORDS].fg = COLOR_YELLOW;
    config.color[CONF_BCOORDS].bg = COLOR_BLACK;
    config.color[CONF_BMOVES].fg = COLOR_WHITE;
    config.color[CONF_BMOVES].bg = COLOR_CYAN;
    config.color[CONF_BMOVES].attrs = A_BOLD;
    config.color[CONF_BMOVES].nattrs = A_BOLD|A_REVERSE;
    config.color[CONF_BGRAPHICS].fg = COLOR_WHITE;
    config.color[CONF_BGRAPHICS].bg = COLOR_BLACK;
    config.color[CONF_BWHITE].fg = COLOR_WHITE;
    config.color[CONF_BWHITE].bg = COLOR_RED;
    config.color[CONF_BWHITE].nattrs = A_REVERSE;
    config.color[CONF_BBLACK].fg = COLOR_WHITE;
    config.color[CONF_BBLACK].bg = COLOR_BLACK;
    config.color[CONF_BSELECTED].fg = COLOR_WHITE;
    config.color[CONF_BSELECTED].bg = COLOR_YELLOW;
    config.color[CONF_BSELECTED].nattrs = A_BOLD|A_REVERSE;
    config.color[CONF_BCURSOR].fg = COLOR_WHITE;
    config.color[CONF_BCURSOR].bg = COLOR_GREEN;
    config.color[CONF_BCURSOR].nattrs = A_BOLD|A_REVERSE;
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
    config.color[CONF_SNOTIFY].fg = COLOR_RED;
    config.color[CONF_SNOTIFY].bg = COLOR_BLACK;
    config.color[CONF_SNOTIFY].nattrs = A_BOLD;
    config.color[CONF_WWINDOW].fg = COLOR_WHITE;
    config.color[CONF_WWINDOW].bg = COLOR_BLACK;
    config.color[CONF_WBORDER].fg = COLOR_CYAN;
    config.color[CONF_WBORDER].bg = COLOR_BLACK;
    config.color[CONF_WTITLE].fg = COLOR_WHITE;
    config.color[CONF_WTITLE].bg = COLOR_BLUE;
    config.color[CONF_WTITLE].nattrs = A_REVERSE;
    config.color[CONF_BWINDOW].fg = COLOR_WHITE;
    config.color[CONF_BWINDOW].bg = COLOR_BLACK;
    config.color[CONF_BBORDER].fg = COLOR_CYAN;
    config.color[CONF_BBORDER].bg = COLOR_BLACK;
    config.color[CONF_BTITLE].fg = COLOR_WHITE;
    config.color[CONF_BTITLE].bg = COLOR_BLUE;
    config.color[CONF_BTITLE].nattrs = A_REVERSE;
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
    config.color[CONF_MTITLE].bg = COLOR_GREEN;
    config.color[CONF_MTITLE].nattrs = A_REVERSE;
    config.color[CONF_MPROMPT].fg = COLOR_CYAN;
    config.color[CONF_MPROMPT].bg = COLOR_BLACK;
    config.color[CONF_MPROMPT].nattrs = A_BOLD;
    config.color[CONF_IWINDOW].fg = COLOR_WHITE;
    config.color[CONF_IWINDOW].bg = COLOR_BLACK;
    config.color[CONF_IBORDER].fg = COLOR_CYAN;
    config.color[CONF_IBORDER].bg = COLOR_BLACK;
    config.color[CONF_ITITLE].fg = COLOR_WHITE;
    config.color[CONF_ITITLE].bg = COLOR_GREEN;
    config.color[CONF_ITITLE].nattrs = A_REVERSE;
    config.color[CONF_IPROMPT].fg = COLOR_CYAN;
    config.color[CONF_IPROMPT].bg = COLOR_BLACK;
    config.color[CONF_IPROMPT].nattrs = A_BOLD;

    return;
}
