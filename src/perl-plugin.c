/* vim:tw=78:ts=8:sw=4:set ft=c:  */
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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <EXTERN.h>
#include <perl.h>

#include "perl-plugin.h"

static PerlInterpreter *my_perl;
static perl_error_func *error_func;

int perl_init_file(const char *filename, perl_error_func *efunc)
{
  const char *args[] = {"", "-e", "0"};
  char *buf;
  int argc;
  char **argv, **env;

  if (my_perl)
    return 0;

  error_func = efunc;
  PERL_SYS_INIT3(&argc, &argv, &env);
  my_perl = perl_alloc();
  perl_construct(my_perl);
  PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
  perl_parse(my_perl, NULL, 3, (char **) args, NULL);
  perl_run(my_perl);

  asprintf(&buf, "do \"%s\"", filename);
  eval_pv(buf, FALSE);
  free(buf);

  if (SvTRUE(ERRSV))
  {
    (*error_func)("%s", SvPV_nolen(ERRSV));
    perl_cleanup();
    return 1;
  }

  return 0;
}

int perl_call_sub(const char *str, const char *arg, char **result)
{
  int flags = G_EVAL;
  int ret = 0;

  dSP;
  ENTER;
  SAVETMPS;
  PUSHMARK(SP);

  if (arg && *arg)
  {
    XPUSHs(sv_2mortal(newSVpvn(arg, strlen(arg))));
    PUTBACK;
  }
  else
    flags |= G_NOARGS;

  call_pv(str, flags);

  if (SvTRUE(ERRSV))
  {
    (*error_func)("%s", SvPV_nolen(ERRSV));
    ret = 1;
  }
  else
    *result = strdup(POPp);

  PUTBACK;
  FREETMPS;
  LEAVE;
  return ret;
}

void perl_cleanup()
{
  if (!my_perl)
    return;

  perl_destruct(my_perl);
  perl_free(my_perl);
  PERL_SYS_TERM();
  my_perl = NULL;
  error_func = NULL;
}
