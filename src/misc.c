/* $Id: misc.c,v 1.2 2002-12-07 21:30:06 bjk Exp $ */
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
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <ctype.h>

void *Realloc(void *ptr, size_t size)
{
    void *ptr2;

    if ((ptr2 = realloc(ptr, size)) == NULL)
	err(EXIT_FAILURE, "realloc()");

    return ptr2;
}

void *Calloc(size_t n, size_t size)
{
    void *p;

    if ((p = calloc(n, size)) == NULL)
	err(EXIT_FAILURE, "calloc()");

    return p;
}

char *real_filename(char *path)
{
    char *tmp;

    if (!path[0])
	return NULL;

    if ((tmp = strrchr(path, '/')) == NULL)
	return path;

    return ++tmp;
}

char *trim(char *str)
{
    int i = 0;

    if (!str[0])
	return NULL;

    while (isspace(*str))
	str++;

    for (i = strlen(str) - 1; isspace(str[i]); i--)
	str[i] = 0;

    return str;
}
