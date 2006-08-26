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
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

void *Malloc(size_t size)
{
    void *ptr;

    if ((ptr = malloc(size)) == NULL)
	err(EXIT_FAILURE, "malloc()");

    return ptr;
}

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

char *rtrim(char *str)
{
    int i;

    for (i = strlen(str) - 1; isspace(str[i]); i--)
	str[i] = 0;

    return str;
}

char *trim(char *str)
{
    if (!str)
	return NULL;

    while (isspace(*str))
	str++;

    return rtrim(str);
}

char *itoa(long n)
{
    static char buf[16];

    snprintf(buf, sizeof(buf), "%li", n);
    return buf;
}

int integer_len(long n)
{
    return strlen(itoa(n));
}

int isinteger(const char *str)
{
    int i;
    int len = strlen(str);

    for (i = 0; i < len; i++) {
	if (!isdigit(str[i]))
	    return 0;
    }

    return 1;
}

char *pathfix(const char *str)
{
    static char buf[FILENAME_MAX];
    struct passwd *pw;

    if (*str == '~') {
	pw = getpwuid(getuid());
	strncpy(buf, pw->pw_dir, sizeof(buf));
	strncat(buf, str + 1, sizeof(buf));
    }
    else
	strncpy(buf, str, sizeof(buf));

    return buf;
}
