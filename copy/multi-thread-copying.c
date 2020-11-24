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
#include <stdint.h>
#include <stdbool.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <errno.h>
#include <assert.h>
#include <sys/stat.h>

#include <pthread.h>

#include <libnbd.h>

#include "rounding.h"
#include "iszero.h"

#include "nbdcopy.h"

/* Threads pick up work in units of THREAD_WORK_SIZE starting at the
 * next_offset.  The lock protects next_offset.
 */
static uint64_t next_offset = 0;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

static bool
get_next_offset (uint64_t *offset, uint64_t *count)
{
  bool r = false;               /* returning false means no more work */

  pthread_mutex_lock (&lock);
  if (next_offset < src.size) {
    *offset = next_offset;

    /* Work out how large this range is.  The last range may be
     * smaller than THREAD_WORK_SIZE.
     */
    *count = src.size - *offset;
    if (*count > THREAD_WORK_SIZE)
      *count = THREAD_WORK_SIZE;

    next_offset += THREAD_WORK_SIZE;
    r = true;                   /* there is more work */

    /* XXX This means the progress bar "runs fast" since it shows the
     * progress issuing commands, not necessarily progress performing
     * the commands.  We might move this into a callback, but those
     * are called from threads and not necessarily in monotonic order
     * so the progress bar would move erratically.
     */
    if (progress)
      progress_bar (*offset, dst.size);
  }
  pthread_mutex_unlock (&lock);
  return r;
}

static void *worker_thread (void *ip);

void
multi_thread_copying (void)
{
  pthread_t *workers;
  size_t i;
  int err;

  /* Some invariants that should be true if the main program called us
   * correctly.
   */
  assert (threads > 0);
  assert (threads == connections);
  if (src.t == NBD)
    assert (src.u.nbd.size == connections);
  if (dst.t == NBD)
    assert (dst.u.nbd.size == connections);
  assert (src.size != -1);

  workers = malloc (sizeof (pthread_t) * threads);
  if (workers == NULL) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }

  /* Start the worker threads. */
  for (i = 0; i < threads; ++i) {
    err = pthread_create (&workers[i], NULL, worker_thread,
                          (void *)(uintptr_t)i);
    if (err != 0) {
      errno = err;
      perror ("pthread_create");
      exit (EXIT_FAILURE);
    }
  }

  /* Wait until all worker threads exit. */
  for (i = 0; i < threads; ++i) {
    err = pthread_join (workers[i], NULL);
    if (err != 0) {
      errno = err;
      perror ("pthread_join");
      exit (EXIT_FAILURE);
    }
  }

  free (workers);
}

static void wait_for_request_slots (uintptr_t index);
static unsigned in_flight (struct nbd_handle *src_nbd,
                           struct nbd_handle *dst_nbd);
static void poll_both_ends (struct nbd_handle *src_nbd,
                            struct nbd_handle *dst_nbd);
static int finished_read (void *vp, int *error);
static int free_buffer (void *vp, int *error);
static void fill_dst_range_with_zeroes (struct buffer *buffer);

/* There are 'threads' worker threads, each copying work ranges from
 * src to dst until there are no more work ranges.
 */
static void *
worker_thread (void *indexp)
{
  uintptr_t index = (uintptr_t) indexp;
  uint64_t offset, count;
  struct nbd_handle *src_nbd, *dst_nbd;
  extent_list exts = empty_vector;

  /* In the case where src or dst is NBD, use
   * {src|dst}.u.nbd.ptr[index] so that each thread is connected to
   * its own NBD connection.  If either src or dst is LOCAL then set
   * src_nbd/dst_nbd to NULL so hopefully we'll crash hard if the
   * program accidentally tries to use them.
   */
  if (src.t == NBD)
    src_nbd = src.u.nbd.ptr[index];
  else
    src_nbd = NULL;
  if (dst.t == NBD)
    dst_nbd = dst.u.nbd.ptr[index];
  else
    dst_nbd = NULL;

  while (get_next_offset (&offset, &count)) {
    size_t i;

    assert (0 < count && count <= THREAD_WORK_SIZE);
    if (extents)
      src.ops->get_extents (&src, index, offset, count, &exts);
    else
      default_get_extents (&src, index, offset, count, &exts);

    for (i = 0; i < exts.size; ++i) {
      struct buffer *buffer;
      char *data;
      size_t len;

      if (exts.ptr[i].hole) {
        /* The source is a hole so we can proceed directly to
         * skipping, trimming or writing zeroes at the destination.
         */
        buffer = calloc (1, sizeof *buffer);
        if (buffer == NULL) {
          perror ("malloc");
          exit (EXIT_FAILURE);
        }
        buffer->offset = exts.ptr[i].offset;
        buffer->len = exts.ptr[i].length;
        buffer->index = index;
        fill_dst_range_with_zeroes (buffer);
      }

      else /* data */ {
        /* As the extent might be larger than permitted for a single
         * command, we may have to split this into multiple read
         * requests.
         */
        while (exts.ptr[i].length > 0) {
          len = exts.ptr[i].length;
          if (len > MAX_REQUEST_SIZE)
            len = MAX_REQUEST_SIZE;
          data = malloc (len);
          if (data == NULL) {
            perror ("malloc");
            exit (EXIT_FAILURE);
          }
          buffer = calloc (1, sizeof *buffer);
          if (buffer == NULL) {
            perror ("malloc");
            exit (EXIT_FAILURE);
          }
          buffer->offset = exts.ptr[i].offset;
          buffer->len = len;
          buffer->data = data;
          buffer->free_data = free;
          buffer->index = index;

          wait_for_request_slots (index);

          /* Begin the asynch read operation. */
          src.ops->asynch_read (&src, buffer,
                                (nbd_completion_callback) {
                                  .callback = finished_read,
                                  .user_data = buffer,
                                });

          exts.ptr[i].offset += len;
          exts.ptr[i].length -= len;
        }
      }

      offset += count;
      count = 0;
    } /* for extents */
  }

  /* Wait for in flight NBD requests to finish. */
  while (in_flight (src_nbd, dst_nbd) > 0)
    poll_both_ends (src_nbd, dst_nbd);

  if (progress)
    progress_bar (1, 1);

  free (exts.ptr);
  return NULL;
}

/* If the number of requests in flight exceeds the limit, poll
 * waiting for at least one request to finish.  This enforces
 * the user --requests option.
 */
static void
wait_for_request_slots (uintptr_t index)
{
  struct nbd_handle *src_nbd, *dst_nbd;

  if (src.t == NBD)
    src_nbd = src.u.nbd.ptr[index];
  else
    src_nbd = NULL;
  if (dst.t == NBD)
    dst_nbd = dst.u.nbd.ptr[index];
  else
    dst_nbd = NULL;

  while (in_flight (src_nbd, dst_nbd) >= max_requests)
    poll_both_ends (src_nbd, dst_nbd);
}

/* Count the number of NBD commands in flight.  Since the commands are
 * auto-retired in the callbacks we don't need to count "done"
 * commands.
 */
static unsigned
in_flight (struct nbd_handle *src_nbd, struct nbd_handle *dst_nbd)
{
  return
    (src_nbd ? nbd_aio_in_flight (src_nbd) : 0) +
    (dst_nbd ? nbd_aio_in_flight (dst_nbd) : 0);
}

/* Poll (optional) NBD src and NBD dst, moving the state machine(s)
 * along.  This is a lightly modified nbd_poll.
 */
static void
poll_both_ends (struct nbd_handle *src_nbd, struct nbd_handle *dst_nbd)
{
  struct pollfd fds[2] = { 0 };
  int r;

  /* Note: poll will ignore fd == -1 */

  if (!src_nbd)
    fds[0].fd = -1;
  else {
    fds[0].fd = nbd_aio_get_fd (src_nbd);
    switch (nbd_aio_get_direction (src_nbd)) {
    case LIBNBD_AIO_DIRECTION_READ:
      fds[0].events = POLLIN;
      break;
    case LIBNBD_AIO_DIRECTION_WRITE:
      fds[0].events = POLLOUT;
      break;
    case LIBNBD_AIO_DIRECTION_BOTH:
      fds[0].events = POLLIN|POLLOUT;
      break;
    default:
      fprintf (stderr, "%s: %s\n", src.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  if (!dst_nbd)
    fds[1].fd = -1;
  else {
    fds[1].fd = nbd_aio_get_fd (dst_nbd);
    switch (nbd_aio_get_direction (dst_nbd)) {
    case LIBNBD_AIO_DIRECTION_READ:
      fds[1].events = POLLIN;
      break;
    case LIBNBD_AIO_DIRECTION_WRITE:
      fds[1].events = POLLOUT;
      break;
    case LIBNBD_AIO_DIRECTION_BOTH:
      fds[1].events = POLLIN|POLLOUT;
      break;
    default:
      fprintf (stderr, "%s: %s\n", src.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  r = poll (fds, 2, -1);
  if (r == -1) {
    perror ("poll");
    exit (EXIT_FAILURE);
  }
  if (r == 0)
    return;

  if (src_nbd) {
    r = 0;
    if ((fds[0].revents & (POLLIN | POLLHUP)) != 0)
      r = nbd_aio_notify_read (src_nbd);
    else if ((fds[0].revents & POLLOUT) != 0)
      r = nbd_aio_notify_write (src_nbd);
    else if ((fds[0].revents & (POLLERR | POLLNVAL)) != 0) {
      errno = ENOTCONN;
      r = -1;
    }
    if (r == -1) {
      perror (src.name);
      exit (EXIT_FAILURE);
    }
  }

  if (dst_nbd) {
    r = 0;
    if ((fds[1].revents & (POLLIN | POLLHUP)) != 0)
      r = nbd_aio_notify_read (dst_nbd);
    else if ((fds[1].revents & POLLOUT) != 0)
      r = nbd_aio_notify_write (dst_nbd);
    else if ((fds[1].revents & (POLLERR | POLLNVAL)) != 0) {
      errno = ENOTCONN;
      r = -1;
    }
    if (r == -1) {
      perror (dst.name);
      exit (EXIT_FAILURE);
    }
  }
}

/* Create a sub-buffer of an existing buffer. */
static struct buffer *
copy_subbuffer (struct buffer *buffer, uint64_t offset, size_t len,
                bool hole)
{
  const uint64_t end = buffer->offset + buffer->len;
  struct buffer *newbuffer;

  assert (buffer->offset <= offset && offset < end);
  assert (offset + len <= end);

  newbuffer = calloc (1, sizeof *newbuffer);
  if (newbuffer == NULL) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }
  newbuffer->offset = offset;
  newbuffer->len = len;
  if (!hole) {
    newbuffer->data = malloc (len);
    if (newbuffer->data == NULL) {
      perror ("malloc");
      exit (EXIT_FAILURE);
    }
    memcpy (newbuffer->data, buffer->data + offset - buffer->offset, len);
    newbuffer->free_data = free;
  }
  newbuffer->index = buffer->index;

  return newbuffer;
}

/* Callback called when src has finished one read command.  This
 * initiates a write.
 */
static int
finished_read (void *vp, int *error)
{
  struct buffer *buffer = vp;

  if (allocated || sparse_size == 0) {
    /* If sparseness detection (see below) is turned off then we write
     * the whole buffer.
     */
    dst.ops->asynch_write (&dst, buffer,
                           (nbd_completion_callback) {
                             .callback = free_buffer,
                             .user_data = buffer,
                           });
  }
  else {                               /* Sparseness detection. */
    const uint64_t start = buffer->offset;
    const uint64_t end = start + buffer->len;
    uint64_t last_offset = start;
    bool last_is_hole = false;
    uint64_t i;
    struct buffer *newbuffer;

    /* Iterate over the buffer, starting on a block boundary. */
    for (i = ROUND_UP (start, sparse_size);
         i + sparse_size <= end;
         i += sparse_size) {
      if (is_zero (&buffer->data[i-start], sparse_size)) {
        /* It's a hole.  If the last was a hole too then we do nothing
         * here which coalesces.  Otherwise write the last data and
         * start a new hole.
         */
        if (!last_is_hole) {
          /* Write the last data (if any). */
          if (i - last_offset > 0) {
            newbuffer = copy_subbuffer (buffer, last_offset, i - last_offset,
                                        false);
            dst.ops->asynch_write (&dst, newbuffer,
                                   (nbd_completion_callback) {
                                     .callback = free_buffer,
                                     .user_data = newbuffer,
                                   });
          }
          /* Start the new hole. */
          last_offset = i;
          last_is_hole = true;
        }
      }
      else {
        /* It's data.  If the last was data too, do nothing =>
         * coalesce.  Otherwise write the last hole and start a new
         * data.
         */
        if (last_is_hole) {
          /* Write the last hole (if any). */
          if (i - last_offset > 0) {
            newbuffer = copy_subbuffer (buffer, last_offset, i - last_offset,
                                        true);
            fill_dst_range_with_zeroes (newbuffer);
          }
          /* Start the new data. */
          last_offset = i;
          last_is_hole = false;
        }
      }
    } /* for i */

    /* Write the last_offset up to the end. */
    if (end - last_offset > 0) {
      if (!last_is_hole) {
        newbuffer = copy_subbuffer (buffer, last_offset, end - last_offset,
                                    false);
        dst.ops->asynch_write (&dst, newbuffer,
                               (nbd_completion_callback) {
                                 .callback = free_buffer,
                                 .user_data = newbuffer,
                               });
      }
      else {
        newbuffer = copy_subbuffer (buffer, last_offset, end - last_offset,
                                    true);
        fill_dst_range_with_zeroes (newbuffer);
      }
    }

    /* Free the original buffer since it has been split into
     * subbuffers and the original is no longer needed.
     */
    free_buffer (buffer, &errno);
  }

  return 1; /* auto-retires the command */
}

/* Fill a range in dst with zeroes.  This is called from the copying
 * loop when we see a hole in the source.  Depending on the command
 * line flags this could mean:
 *
 * --destination-is-zero:
 *                 do nothing
 *
 * --allocated:    we must write zeroes either using an efficient
 *                 zeroing command or writing a buffer of zeroes
 *
 * (neither flag)  try trimming if supported, else write zeroes
 *                 as above
 *
 * This takes over ownership of the buffer and frees it eventually.
 */
static void
fill_dst_range_with_zeroes (struct buffer *buffer)
{
  char *data;

  if (destination_is_zero)
    goto free_and_return;

  if (!allocated) {
    /* Try trimming. */
    wait_for_request_slots (buffer->index);
    if (dst.ops->asynch_trim (&dst, buffer,
                              (nbd_completion_callback) {
                                .callback = free_buffer,
                                .user_data = buffer,
                              }))
      return;
  }

  /* Try efficient zeroing. */
  wait_for_request_slots (buffer->index);
  if (dst.ops->asynch_zero (&dst, buffer,
                            (nbd_completion_callback) {
                              .callback = free_buffer,
                              .user_data = buffer,
                            }))
    return;

  /* Fall back to loop writing zeroes.  This is going to be slow
   * anyway, so do it synchronously. XXX
   */
  data = calloc (1, MAX_REQUEST_SIZE);
  if (!data) {
    perror ("calloc");
    exit (EXIT_FAILURE);
  }
  while (buffer->len > 0) {
    size_t len = buffer->len;

    if (len > MAX_REQUEST_SIZE)
      len = MAX_REQUEST_SIZE;

    dst.ops->synch_write (&dst, data, len, buffer->offset);
    buffer->len -= len;
    buffer->offset += len;
  }
  free (data);

 free_and_return:
  free_buffer (buffer, &errno);
}

static int
free_buffer (void *vp, int *error)
{
  struct buffer *buffer = vp;

  if (buffer->free_data) buffer->free_data (buffer->data);
  free (buffer);
  return 1; /* auto-retires the command */
}
