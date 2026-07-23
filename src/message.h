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
#ifndef MESSAGE_H
#define MESSAGE_H

#define MSG_WIDTH	((COLS / 5) * 4)	/* For multiline messages. */

#define message(t, p, fmt...) \
    construct_message(t, p, 0, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, ##fmt)
#define cmessage(t, p, fmt...) \
    construct_message(t, p, 1, 0, NULL, NULL, NULL, NULL, 0, 0, NULL, ##fmt)

typedef void *(message_func) (void *);

WIN *construct_message (const char *, const char *, int, int, const char *,
			message_func *, void *, window_exit_func *, wint_t,
                        int, window_resize_func *, const char *, ...);

/*
 * Yes/No confirm popup (menu style, clickable buttons).
 * On close, efunc is run if non-NULL; win->c is the first character of
 * the localized "y" on Yes, or 0 on No/Esc.  Typical efunc checks win->c
 * against yes_wchar (same as classic [ Yes or No ] prompts).
 */
WIN *construct_confirm (const char *title, const char *text,
			window_exit_func *efunc);

/* Mouse for an open confirm WIN. Returns 1 if handled. */
int confirm_dialog_mouse (WIN *win, int x, int y, mmask_t bstate);

#endif
