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

#ifndef NBDCOPY_H
#define NBDCOPY_H

#include <stdbool.h>
#include <stdint.h>

#include "vector.h"

#define MAX_REQUEST_SIZE (32 * 1024 * 1024)

/* This must be a multiple of MAX_REQUEST_SIZE. */
#define THREAD_WORK_SIZE (128 * 1024 * 1024)

DEFINE_VECTOR_TYPE (handles, struct nbd_handle *)

struct rw {
  enum { LOCAL, NBD } t;
  const char *name;             /* Printable name, for error messages etc. */
  int64_t size;                 /* May be -1 for streams. */
  union {
    struct {                    /* For LOCAL. */
      int fd;
      struct stat stat;
    } local;
    handles nbd;                /* For NBD, one handle per connection. */
  } u;
};

extern unsigned connections;
extern unsigned max_requests;
extern bool progress;
extern bool synchronous;
extern unsigned threads;
extern struct rw src, dst;

extern void progress_bar (off_t pos, int64_t size);
extern void synch_copying (void);
extern void multi_thread_copying (void);

#endif /* NBDCOPY_H */