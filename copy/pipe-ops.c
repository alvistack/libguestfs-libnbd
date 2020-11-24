/* NBD client library in userspace.
 * Copyright (C) 2020 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>

#include "nbdcopy.h"

static size_t
pipe_synch_read (struct rw *rw,
                 void *data, size_t len, uint64_t offset)
{
  ssize_t r;

  assert (rw->t == LOCAL);

  r = read (rw->u.local.fd, data, len);
  if (r == -1) {
    perror (rw->name);
    exit (EXIT_FAILURE);
  }
  return r;
}

static void
pipe_synch_write (struct rw *rw,
                  const void *data, size_t len, uint64_t offset)
{
  ssize_t r;

  assert (rw->t == LOCAL);

  while (len > 0) {
    r = write (rw->u.local.fd, data, len);
    if (r == -1) {
      perror (rw->name);
      exit (EXIT_FAILURE);
    }
    data += r;
    len -= r;
  }
}

static void
pipe_asynch_read (struct rw *rw,
                  struct buffer *buffer,
                  nbd_completion_callback cb)
{
  abort (); /* See comment below. */
}

static void
pipe_asynch_write (struct rw *rw,
                   struct buffer *buffer,
                   nbd_completion_callback cb)
{
  abort (); /* See comment below. */
}

struct rw_ops pipe_ops = {
  .synch_read = pipe_synch_read,
  .synch_write = pipe_synch_write,

  /* Asynch pipe operations are not defined.  These should never be
   * called because pipes/streams/sockets force --synchronous.
   * Because calling a NULL pointer screws up the stack trace when
   * we're not using frame pointers, these are defined to functions
   * that call abort().
   */
  .asynch_read = pipe_asynch_read,
  .asynch_write = pipe_asynch_write,
};