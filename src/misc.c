/* $Id: misc.c,v 1.11 2003-01-31 20:47:38 bjk Exp $ */
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
#include <stdio.h>
#include <stdlib.h>
#include <err.h>
#include <string.h>
#include <ctype.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"

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

char *real_filename(char *path)
{
    char *tmp;
    static char buf[FILENAME_MAX];
    int slash = 0;

    if (!path[0])
	return NULL;

    strncpy(buf, path, sizeof(buf));
    tmp = buf;

    if (tmp[strlen(tmp) - 1] == '/') {
	tmp[strlen(tmp) - 1] = 0;
	slash = 1;
    }

    if ((tmp = strrchr(tmp, '/')) == NULL)
	return path;

    if (slash)
	buf[strlen(tmp)] = '/';

    return ++tmp;
}

char *trim(char *str)
{
    int i = 0;

    if (!str)
	return NULL;

    while (isspace(*str))
	str++;

    for (i = strlen(str) - 1; isspace(str[i]); i--)
	str[i] = 0;

    return str;
}

char *itoa(long n)
{
    static char buf[16];

    snprintf(buf, sizeof(buf), "%li", n);
    return buf;
}

int integer_len(int n)
{
    int len = 1;

    if (n >= 10)
	len++;
    else if (n >= 100)
	len++;
    else if (n >= 1000)
	len++;
    else if (n >= 10000)
	len++;
    else if (n >= 100000)
	len++;

    return len;
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

char *tilde_expand(char *str)
{
    static char buf[FILENAME_MAX];

    if (*str == '~') {
	strncpy(buf, getenv("HOME"), sizeof(buf));

	if (++str)
	    strncat(buf, str, sizeof(buf));
    }
    else
	return str;

    return buf;
}

#ifdef DEBUG
void DUMP(const char *fmt, ...)
{
    FILE *fp;
    va_list ap;
    char line[LINE_MAX];

    if ((fp = fopen("debug", "a")) == NULL)
	return;

    va_start(ap, fmt);
    vsnprintf(line, sizeof(line), fmt, ap);
    va_end(ap);

    fprintf(fp, "%s", line);
    fclose(fp);
    return;
}
#endif

char *str_etc(const char *str, int maxlen, int rev)
{
    int len = strlen(str);
    static char buf[80], *p = buf;
    int i;

    strncpy(buf, str, sizeof(buf));

    if (len > maxlen) {
	if (rev) {
	    p = buf;
	    *p++ = '.';
	    *p++ = '.';
	    *p++ = '.';

	    for (i = 0; i < maxlen + 3; i++)
		*p++ = buf[(len - maxlen) + i + 3]; 
	}
	else {
	    p = buf + maxlen - 4;
	    *p++ = '.';
	    *p++ = '.';
	    *p++ = '.';
	}

	*p = '\0';
    }

    return buf;
}
