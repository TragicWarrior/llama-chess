/* vim:tw=78:ts=8:sw=4:set ft=c:  */
/*
    Copyright (C) 2002-2006 Ben Kibbey <bjk@arbornet.org>

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
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "common.h"
#include "ics.h"

int parse_ics_output(char *str)
{
    return 0;
}

int send_to_ics(int sockfd, const char *format, ...)
{
    va_list ap;
    int len;
    char *line;
    int try = 0;

    va_start(ap, format);
#ifdef HAVE_VASPRINTF
    len = vasprintf(&line, format, ap);
#else
    line = Malloc(LINE_MAX);
    len = vsnprintf(line, LINE_MAX, format, ap);
#endif
    va_end(ap);

    while (1) {
	int n;
	fd_set fds;
	struct timeval tv;

	FD_ZERO(&fds);
	FD_SET(sockfd, &fds);

	tv.tv_sec = 0;
	tv.tv_usec = 0;

	if ((n = select(sockfd + 1, NULL, &fds, NULL, &tv)) > 0) {
	    if (FD_ISSET(sockfd, &fds)) {
		n = send(sockfd, line, len, 0);

		if (n == -1) {
		    if (errno == EAGAIN)
			continue;

		    message(ERROR, ANYKEY, "Attempt #%i. send(): %s", ++try,
			    strerror(errno));
		    continue;
		}

		if (len != n) {
		    message(NULL, ANYKEY, "try #%i: send() error to socket. "
			    "Expected %i, got %i.", ++try, len, n);
		    continue;
		}

		break;
	    }
	}
	else {
	    /* timeout */
	}
    }

    free(line);
    return 0;
}

int ics_connect(int *sockfd)
{
    struct sockaddr_in sa;
    struct hostent *h;

    memset(&sa, 0, sizeof(struct sockaddr_in));

    if ((h = gethostbyname(config.ics_server)) == NULL) {
	message(ERROR, ANYKEY, "%s: %s", config.ics_server, hstrerror(h_errno));
	return 1;
    }

    /*
    if (inet_aton(argv[1], &sa.sin_addr) == 0)
	errx(EXIT_FAILURE, "malformed address");
    */

    sa.sin_addr.s_addr = inet_addr(h->h_addr);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(config.ics_port);

    if ((*sockfd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
	message(ERROR, ANYKEY, "socket(): %s", strerror(errno));
	return 1;
    }

    fcntl(*sockfd, F_SETFL, O_NONBLOCK);

    /*
    if (bind(*sockfd, (struct sockaddr *)&sa, sizeof(struct sockaddr)) == -1) {
	message(ERROR, ANYKEY, "bind(): %s", strerror(errno));
	return 1;
    }
    */

    if (connect(*sockfd, (struct sockaddr *)&sa, sizeof(struct sockaddr)) 
	    == -1) { 
	message(ERROR, ANYKEY, "connect(): %s", strerror(errno));
	return 1;
    }

    return 0;
}
