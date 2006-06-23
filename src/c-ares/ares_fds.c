/* Copyright 1998 by the Massachusetts Institute of Technology.
 *
 * Permission to use, copy, modify, and distribute this
 * software and its documentation for any purpose and without
 * fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting
 * documentation, and that the name of M.I.T. not be used in
 * advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 */

#include "setup.h"
#include <sys/types.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

#include "ares.h"
#include "ares_private.h"

int ares_fds(ares_channel channel)
{
  struct server_state *server;
  ares_socket_t nfds;
  int i;

  /* No queries, no file descriptors. */
  if (!channel->queries)
    return 0;

  nfds = 0;
  for (i = 0; i < channel->nservers; i++)
    {
      server = &channel->servers[i];
      if (server->udp_socket != ARES_SOCKET_BAD)
        {
          server->udp_pollfd->events = POLLIN | POLLPRI;

		  if (server->udp_socket >= nfds)
            nfds = server->udp_socket + 1;
        }
      if (server->tcp_socket != ARES_SOCKET_BAD)
        {
          server->tcp_pollfd->events = POLLIN | POLLPRI;
          if (server->qhead)
            server->tcp_pollfd->events |= POLLOUT;
          if (server->tcp_socket >= nfds)
            nfds = server->tcp_socket + 1;
        }
    }
  return (int)nfds;
}
