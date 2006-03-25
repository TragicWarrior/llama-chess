/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2006 Ben Kibbey <bjk@luxsci.net>

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
#ifndef MISC_H
#define MISC_H

void *Malloc(size_t size);
void *Realloc(void *ptr, size_t size);
void *Calloc(size_t n, size_t size);
char *trim(char *str);
char *itoa(long n);
int integer_len(long n);
int isinteger(const char *str);
char *tilde_expand(char *str);
char *compression_cmd(const char *filename, int expand);
FILE *open_file(const char *filename);

#endif
