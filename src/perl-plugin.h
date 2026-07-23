/* vim:tw=78:ts=4:sw=4:sts=4:et:set ft=c:  */
/*
    Copyright (C) 2007-2024 Ben Kibbey <bjk@luxsci.net>

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
#ifndef PERL_PLUGIN_H
#define PERL_PLUGIN_H

typedef void(perl_error_func)(const char *fmt, ...);

int perl_init_file(const char *filename, perl_error_func *efunc);
int perl_call_sub(const char *str, const char *arg, char **result);
void perl_cleanup(void);

#endif
