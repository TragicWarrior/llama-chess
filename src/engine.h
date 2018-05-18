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
#ifndef ENGINE_H
#define ENGINE_H

/*
 * Solaris 5.9
 */
#ifndef _PATH_DEV
#define _PATH_DEV	"/dev/"
#endif

#define RETURN(d)	{d->engine->status = ENGINE_READY; return;}

enum
{
  ENGINE_OFFLINE = -1, ENGINE_READY, ENGINE_THINKING, ENGINE_INITIALIZING
};

enum
{
  HUMAN, ENGINE
};

enum
{
  ENGINE_IN_FD,
  ENGINE_OUT_FD,
};

struct queue_s
{
  char *line;
  int status;
};

struct engine_s
{
  int fd[2];
  pid_t pid;
  int status;
  struct queue_s **queue;
  char **enginebuf;
  char *iobuf;
  int len;
};

int send_signal_to_engine (pid_t, int);
int send_to_engine (GAME g, int, const char *format, ...);
int start_chess_engine (GAME);
void set_engine_defaults (GAME, wchar_t **);
void stop_engine (GAME);
void append_enginebuf (GAME, char *);
void send_engine_command (GAME g);
void add_engine_command (GAME g, int s, const char *fmt, ...);
void parse_engine_output (GAME g, char *str);
int init_chess_engine (GAME g);

#endif
