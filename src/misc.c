/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2018 Ben Kibbey <bjk@luxsci.net>

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

#include "common.h"
#include "misc.h"

void *
Malloc (size_t size)
{
  void *ptr;

  if ((ptr = malloc (size)) == NULL)
    err (EXIT_FAILURE, "malloc()");

  return ptr;
}

void *
Realloc (void *ptr, size_t size)
{
  void *ptr2;

  if ((ptr2 = realloc (ptr, size)) == NULL)
    err (EXIT_FAILURE, "realloc()");

  return ptr2;
}

void *
Calloc (size_t n, size_t size)
{
  void *p;

  if ((p = calloc (n, size)) == NULL)
    err (EXIT_FAILURE, "calloc()");

  return p;
}

char *
rtrim (char *str)
{
  int i;

  if (!*str)
    return str;

  for (i = strlen (str) - 1; isspace (str[i]); i--)
    str[i] = 0;

  return str;
}

char *
trim (char *str)
{
  if (!str)
    return NULL;

  while (isspace (*str))
    str++;

  return rtrim (str);
}

char *
itoa (long n, char *buf)
{
  sprintf (buf, "%li", n);
  return buf;
}

char *
trim_multi (char *value)
{
  char *p;
  int lastc;
  char *str, *s;

  if (!value || !*value)
    return value;

  str = Malloc (strlen (value) + 1);

  for (p = value, lastc = 0, s = str; *p; p++)
    {
      if (isspace (lastc) && isspace (*p))
	continue;

      lastc = *s++ = *p;
    }

  *s = 0;
  return str;
}

int
integer_len (long n)
{
  char buf[32];

  itoa (n, buf);
  return strlen (buf);
}

int
isinteger (const char *str)
{
  int i = 0;
  int len = strlen (str);

  if (*str == '-' && isdigit (*(str + 1)))
    i = 1;

  for (; i < len; i++)
    {
      if (!isdigit (str[i]))
	return 0;
    }

  return 1;
}

char *
pathfix (const char *str)
{
  static char buf[FILENAME_MAX] = { 0 };
  struct passwd *pw;

  if (*str == '~')
    {
      pw = getpwuid (getuid ());
      strncpy (buf, pw->pw_dir, sizeof (buf) - 1);
      strncat (buf, str + 1, sizeof (buf) - 1);
    }
  else
    strncpy (buf, str, sizeof (buf) - 1);

  return buf;
}

wchar_t *
str_etc (const char *str, int maxlen, int rev)
{
  wchar_t *p;
  wchar_t *buf = NULL;

  if (!str)
    return NULL;

  p = str_to_wchar (str);

  if (wcslen (p) > maxlen)
    {
      wchar_t *dot = str_to_wchar (_("..."));

      if (rev)
	{
	  buf = str_to_wchar_len (_("..."), maxlen);
	  buf[wcslen (dot)] = 0;
	  wcsncat (buf, p, maxlen - wcslen (dot));
	}
      else
	{
	  buf = str_to_wchar_len (str, maxlen);
	  buf[wcslen (buf) - wcslen (dot)] = 0;
	  wcscat (buf, dot);
	}

      free (dot);
      free (p);
    }
  else
    buf = p;

  return buf;
}

char **
split_str (char *str, const char *delim, int *n, int *width, int force_trim)
{
  char *tmp;
  int total = 0;
  char **lines = NULL;
  int w = 0;

  if (!str || !delim)
    return NULL;

  while ((tmp = strsep (&str, delim)) != NULL)
    {
      char *p;
      wchar_t *wc;
      int len;

      tmp = rtrim (tmp);

      if (!*tmp)
	continue;

      lines = Realloc (lines, (total + 2) * sizeof (char *));
      p = force_trim ? strdup (trim (tmp)) : strdup (tmp);
      wc = str_to_wchar (p);
      len = wcslen (wc);
      if (w < len)
	w = len;

      free (wc);
      lines[total++] = p;
    }

  if (total)
    {
      lines[total] = NULL;
      *n += total;

      if (*width < w + 2)
	*width = w + 2;
    }

  return lines;
}

// 'max' will always allocate that amount when not 0.
wchar_t *
str_to_wchar_len (const char *str, int max)
{
  wchar_t *wc;
  mbstate_t ps;
  const char *p = str;
  size_t len = max;

  memset (&ps, 0, sizeof (mbstate_t));
  if (!len)
    len = mbsrtowcs (NULL, &p, 0, &ps);

  wc = Calloc (len + 1, sizeof (wchar_t));
  p = str;
  memset (&ps, 0, sizeof (mbstate_t));
  len = mbsrtowcs (wc, &p, len, &ps);
  return wc;
}

wchar_t *
str_to_wchar (const char *str)
{
  return str_to_wchar_len (str, 0);
}

char *
wchar_to_str (const wchar_t * str)
{
  size_t len = wcstombs (NULL, str, 0);
  char *s = Malloc (len * sizeof (char) + 1);
  len = wcstombs (s, str, len);
  s[len] = 0;
  return s;
}

size_t
wcharv_length (wchar_t **s)
{
  size_t n = 0;

  for (n = 0; s && s[n]; n++);
  return n;
}

/* Replace the first occurence of 's' with 'r' in 'h'. */
char *
string_replace (const char *hay, const char *n, const char *r)
{
  char *buf, *b;
  const char *hp = hay;

  if (!strstr (hay, n))
    return strdup (hay);

  buf = malloc (strlen (hay) + strlen (r) + 1);
  b = buf;
  for (hp = hay; hp && *hp; hp++)
    {
      if (!strncmp (hp, n, strlen (n)))
        {
          const char *rp;

          for (rp = r; *rp; rp++)
            *b++ = *rp;

          hp += strlen (n);
          while (*hp)
            *b++ = *hp++;
          break;
        }

      *b++ = *hp;
    }

  *b = 0;
  return buf;
}
