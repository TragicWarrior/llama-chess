/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2013 Ben Kibbey <bjk@luxsci.net>

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>
#include <err.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef WITH_DMALLOC
#include <dmalloc.h>
#endif

#include "misc.h"

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

    if (!*str)
	return str;

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

char *trim_multi(char *value)
{
    char *p;
    int lastc;
    char *str, *s;

    if (!value || !*value)
	return value;

    str = Malloc(strlen(value) + 1);

    for (p = value, lastc = 0, s = str; *p; p++) {
	if (isspace(lastc) && isspace(*p))
	    continue;

	lastc = *s++ = *p;
    }

    *s = 0;
    return str;
}

int integer_len(long n)
{
    return strlen(itoa(n));
}

int isinteger(const char *str)
{
    int i = 0;
    int len = strlen(str);

    if (*str == '-' && isdigit(*(str + 1)))
	i = 1;

    for (; i < len; i++) {
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

wchar_t *str_etc(const char *str, int maxlen, int rev)
{
    int len;
    wchar_t *p;
    wchar_t *buf;

    if (!str)
	return NULL;

    p = str_to_wchar (str);
    len = wcslen(p);

    if (len > maxlen) {
        wchar_t *dot = str_to_wchar ("...");

        buf = Malloc (maxlen+4*sizeof(wchar_t));
        buf[0] = 0;

	if (rev) {
	    wcsncat (buf, dot, maxlen);
	    wcsncat (buf, p, maxlen);
	}
	else {
	    wcsncat (buf, p, maxlen);
	    wcsncat (buf, dot, maxlen);
	}

	free (dot);
	free (p);
    }
    else
      buf = p;

    return buf;
}

char **split_str(char *str, char *delim, int *n, int *width, int force_trim)
{
    char *tmp;
    int total = 0;
    char **lines = NULL;
    int w = 0;

    if (!str || !delim)
	return NULL;

    while ((tmp = strsep(&str, delim)) != NULL) {
	char *p;

	tmp = rtrim(tmp);

	if (!*tmp)
	    continue;

	lines = Realloc(lines, (total + 2) * sizeof(char *));
	p = force_trim ? strdup(trim(tmp)) : strdup(tmp);

	if (w < strlen(p))
	    w = strlen(p);

	lines[total++] = p;
    }

    lines[total] = NULL;
    *n += total;

    if (*width < w + 2)
	*width = w + 2;

    return lines;
}

wchar_t *
str_to_wchar (const char *str)
{
  wchar_t *wc;
  mbstate_t ps;
  const char *p = str;

  memset (&ps, 0, sizeof(mbstate_t));
  size_t len = mbsrtowcs (NULL, &p, 0, &ps)+1;
  wc = Calloc (1, len * sizeof(wchar_t));
  p = str;
  memset (&ps, 0, sizeof(mbstate_t));
  len = mbsrtowcs (wc, &p, len, &ps);
  return wc;
}
