/* $Id: message.h,v 1.4 2002-12-20 17:14:08 bjk Exp $ */
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
#ifndef MESSAGE_H
#define MESSAGE_H

#define MSG_WIDTH	60 /* For multiline messages. */
#define MESSAGE_CP	((COLORS) ? \
	COLOR_PAIR(9) | config.color[CONF_MESSAGE].attrs : \
	config.color[CONF_MESSAGE].nattrs)

#endif
