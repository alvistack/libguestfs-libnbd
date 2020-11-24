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
#include <stdint.h>
#include <string.h>
#include <getopt.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <assert.h>

#include <pthread.h>

#include <libnbd.h>

#include "nbdcopy.h"

unsigned connections = 4;       /* --connections */
bool flush;                     /* --flush flag */
unsigned max_requests = 64;     /* --requests */
bool progress;                  /* -p flag */
bool synchronous;               /* --synchronous flag */
unsigned threads;               /* --threads */
struct rw src, dst;             /* The source and destination. */

static bool is_nbd_uri (const char *s);
static int open_local (const char *prog,
                       const char *filename, bool writing, struct rw *rw);
static void open_nbd_uri (const char *prog,
                          const char *uri, struct rw *rw);
static void open_nbd_subprocess (const char *prog,
                                 const char **argv, size_t argc,
                                 struct rw *rw);

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Copy to and from an NBD server:\n"
"\n"
"    nbdcopy nbd://example.com local.img\n"
"    nbdcopy nbd://example.com - | file -\n"
"    nbdcopy local.img nbd://example.com\n"
"    cat disk1 disk2 | nbdcopy - nbd://example.com\n"
"\n"
"Please read the nbdcopy(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
}

static void
display_version (void)
{
  printf ("%s %s\n", PACKAGE_NAME, PACKAGE_VERSION);
}

int
main (int argc, char *argv[])
{
  enum {
    HELP_OPTION = CHAR_MAX + 1,
    LONG_OPTIONS,
    SHORT_OPTIONS,
    FLUSH_OPTION,
    SYNCHRONOUS_OPTION,
  };
  const char *short_options = "C:pR:T:V";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "connections",        required_argument, NULL, 'C' },
    { "flush",              no_argument,       NULL, FLUSH_OPTION },
    { "progress",           no_argument,       NULL, 'p' },
    { "requests",           required_argument, NULL, 'R' },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "synchronous",        no_argument,       NULL, SYNCHRONOUS_OPTION },
    { "threads",            required_argument, NULL, 'T' },
    { "version",            no_argument,       NULL, 'V' },
    { NULL }
  };
  int c;
  size_t i;

  for (;;) {
    c = getopt_long (argc, argv, short_options, long_options, NULL);
    if (c == -1)
      break;

    switch (c) {
    case HELP_OPTION:
      usage (stdout, EXIT_SUCCESS);

    case LONG_OPTIONS:
      for (i = 0; long_options[i].name != NULL; ++i) {
        if (strcmp (long_options[i].name, "long-options") != 0 &&
            strcmp (long_options[i].name, "short-options") != 0)
          printf ("--%s\n", long_options[i].name);
      }
      exit (EXIT_SUCCESS);

    case SHORT_OPTIONS:
      for (i = 0; short_options[i]; ++i) {
        if (short_options[i] != ':' && short_options[i] != '+')
          printf ("-%c\n", short_options[i]);
      }
      exit (EXIT_SUCCESS);

    case FLUSH_OPTION:
      flush = true;
      break;

    case SYNCHRONOUS_OPTION:
      synchronous = true;
      break;

    case 'C':
      if (sscanf (optarg, "%u", &connections) != 1 || connections == 0) {
        fprintf (stderr, "%s: --connections: could not parse: %s\n",
                 argv[0], optarg);
        exit (EXIT_FAILURE);
      }
      break;

    case 'p':
      progress = true;
      break;

    case 'R':
      if (sscanf (optarg, "%u", &max_requests) != 1 || max_requests == 0) {
        fprintf (stderr, "%s: --requests: could not parse: %s\n",
                 argv[0], optarg);
        exit (EXIT_FAILURE);
      }
      break;

    case 'T':
      if (sscanf (optarg, "%u", &threads) != 1) {
        fprintf (stderr, "%s: --threads: could not parse: %s\n",
                 argv[0], optarg);
        exit (EXIT_FAILURE);
      }
      break;

    case 'V':
      display_version ();
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* The remaining parameters describe the SOURCE and DESTINATION
   * and may either be -, filenames, NBD URIs or [ ... ] sequences.
   */
  if (optind >= argc)
    usage (stderr, EXIT_FAILURE);

  if (strcmp (argv[optind], "[") == 0) { /* Source is [...] */
    for (i = optind+1; i < argc; ++i)
      if (strcmp (argv[i], "]") == 0)
        goto found1;
    usage (stderr, EXIT_FAILURE);

  found1:
    connections = 1;            /* multi-conn not supported */
    src.t = NBD;
    src.name = argv[optind+1];
    open_nbd_subprocess (argv[0],
                         (const char **) &argv[optind+1], i-optind-1, &src);
    optind = i+1;
  }
  else {                        /* Source is not [...]. */
    src.name = argv[optind++];
    src.t = is_nbd_uri (src.name) ? NBD : LOCAL;

    if (src.t == LOCAL)
      src.u.local.fd = open_local (argv[0], src.name, false, &src);
    else
      open_nbd_uri (argv[0], src.name, &src);
  }

  if (optind >= argc)
    usage (stderr, EXIT_FAILURE);

  if (strcmp (argv[optind], "[") == 0) { /* Destination is [...] */
    for (i = optind+1; i < argc; ++i)
      if (strcmp (argv[i], "]") == 0)
        goto found2;
    usage (stderr, EXIT_FAILURE);

  found2:
    connections = 1;            /* multi-conn not supported */
    dst.t = NBD;
    dst.name = argv[optind+1];
    open_nbd_subprocess (argv[0],
                         (const char **) &argv[optind+1], i-optind-1, &dst);
    optind = i+1;
  }
  else {                        /* Destination is not [...] */
    dst.name = argv[optind++];
    dst.t = is_nbd_uri (dst.name) ? NBD : LOCAL;

    if (dst.t == LOCAL)
      dst.u.local.fd = open_local (argv[0], dst.name, true /* writing */, &dst);
    else {
      open_nbd_uri (argv[0], dst.name, &dst);

      /* Obviously this is not going to work if the server is
       * advertising read-only, so fail early with a nice error message.
       */
      if (nbd_is_read_only (dst.u.nbd.ptr[0])) {
        fprintf (stderr, "%s: %s: "
                 "this NBD server is read-only, cannot write to it\n",
                 argv[0], dst.name);
        exit (EXIT_FAILURE);
      }
    }
  }

  /* There must be no extra parameters. */
  if (optind != argc)
    usage (stderr, EXIT_FAILURE);

  /* Check we've set the fields of src and dst. */
  assert (src.t);
  assert (src.ops);
  assert (src.name);
  assert (dst.t);
  assert (dst.ops);
  assert (dst.name);

  /* Prevent copying between local files or devices.  It's unlikely
   * this program will ever be better than highly tuned utilities like
   * cp.
   */
  if (src.t == LOCAL && dst.t == LOCAL) {
    fprintf (stderr,
             "%s: this tool does not let you copy between local files, use\n"
             "cp(1) or dd(1) instead.\n",
             argv[0]);
    exit (EXIT_FAILURE);
  }

  /* If multi-conn is not supported, force connections to 1. */
  if ((src.t == NBD && ! nbd_can_multi_conn (src.u.nbd.ptr[0])) ||
      (dst.t == NBD && ! nbd_can_multi_conn (dst.u.nbd.ptr[0])))
    connections = 1;

  /* Calculate the number of threads from the number of connections. */
  if (threads == 0) {
    long t;

#ifdef _SC_NPROCESSORS_ONLN
    t = sysconf (_SC_NPROCESSORS_ONLN);
    if (t <= 0) {
      perror ("could not get number of cores online");
      t = 1;
    }
#else
    t = 1;
#endif
    threads = (unsigned) t;
  }

  if (synchronous)
    connections = 1;

  if (connections < threads)
    threads = connections;
  if (threads < connections)
    connections = threads;

  /* Calculate the source and destination sizes.  We set these to -1
   * if the size is not known (because it's a stream).  Note that for
   * local types, open_local set something in *.size already.
   */
  if (src.t == NBD) {
    src.size = nbd_get_size (src.u.nbd.ptr[0]);
    if (src.size == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], src.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }
  if (dst.t == LOCAL && S_ISREG (dst.u.local.stat.st_mode)) {
    /* If the destination is an ordinary file then the original file
     * size doesn't matter.  Truncate it to the source size.  But
     * truncate it to zero first so the file is completely empty and
     * sparse.
     */
    dst.size = src.size;
    if (ftruncate (dst.u.local.fd, 0) == -1 ||
        ftruncate (dst.u.local.fd, dst.size) == -1) {
      perror ("truncate");
      exit (EXIT_FAILURE);
    }
  }
  else if (dst.t == NBD) {
    dst.size = nbd_get_size (dst.u.nbd.ptr[0]);
    if (dst.size == -1) {
      fprintf (stderr, "%s: %s: %s\n", argv[0], dst.name, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
  }

  /* Check if the source is bigger than the destination, since that
   * would truncate (ie. lose) data.  Copying from smaller to larger
   * is OK.
   */
  if (src.size >= 0 && dst.size >= 0 && src.size > dst.size) {
    fprintf (stderr,
             "nbdcopy: error: destination size is smaller than source size\n");
    exit (EXIT_FAILURE);
  }

  /* If #connections > 1 then multi-conn is enabled at both ends and
   * we need to open further connections.
   */
  if (connections > 1) {
    assert (threads == connections);

    if (src.t == NBD) {
      for (i = 1; i < connections; ++i)
        open_nbd_uri (argv[0], src.name, &src);
      assert (src.u.nbd.size == connections);
    }
    if (dst.t == NBD) {
      for (i = 1; i < connections; ++i)
        open_nbd_uri (argv[0], dst.name, &dst);
      assert (dst.u.nbd.size == connections);
    }
  }

  /* Start copying. */
  if (synchronous)
    synch_copying ();
  else
    multi_thread_copying ();

  /* Shut down the source side. */
  if (src.t == LOCAL) {
    if (close (src.u.local.fd) == -1) {
      fprintf (stderr, "%s: %s: close: %m\n", argv[0], src.name);
      exit (EXIT_FAILURE);
    }
  }
  else {
    for (i = 0; i < src.u.nbd.size; ++i) {
      if (nbd_shutdown (src.u.nbd.ptr[i], 0) == -1) {
        fprintf (stderr, "%s: %s: %s\n", argv[0], src.name, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      nbd_close (src.u.nbd.ptr[i]);
    }
    free (src.u.nbd.ptr);
  }

  /* Shut down the destination side. */
  if (dst.t == LOCAL) {
    if (flush &&
        (S_ISREG (dst.u.local.stat.st_mode) ||
         S_ISBLK (dst.u.local.stat.st_mode)) &&
        fsync (dst.u.local.fd) == -1) {
      perror (dst.name);
      exit (EXIT_FAILURE);
    }

    if (close (dst.u.local.fd) == -1) {
      fprintf (stderr, "%s: %s: close: %m\n", argv[0], dst.name);
      exit (EXIT_FAILURE);
    }
  }
  else {
    for (i = 0; i < dst.u.nbd.size; ++i) {
      if (flush && nbd_flush (dst.u.nbd.ptr[i], 0) == -1) {
        fprintf (stderr, "%s: %s: %s\n", argv[0], dst.name, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      if (nbd_shutdown (dst.u.nbd.ptr[i], 0) == -1) {
        fprintf (stderr, "%s: %s: %s\n", argv[0], dst.name, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      nbd_close (dst.u.nbd.ptr[i]);
    }
    free (dst.u.nbd.ptr);
  }

  exit (EXIT_SUCCESS);
}

/* Return true if the parameter is an NBD URI. */
static bool
is_nbd_uri (const char *s)
{
  return
    strncmp (s, "nbd:", 4) == 0 ||
    strncmp (s, "nbds:", 5) == 0 ||
    strncmp (s, "nbd+unix:", 9) == 0 ||
    strncmp (s, "nbds+unix:", 10) == 0 ||
    strncmp (s, "nbd+vsock:", 10) == 0 ||
    strncmp (s, "nbds+vsock:", 11) == 0;
}

/* Open a local (non-NBD) file, ie. a file, device, or "-" for stdio.
 * Returns the open file descriptor which the caller must close.
 *
 * “writing” is true if this is the destination parameter.
 * “rw->u.local.stat” and “rw->size” return the file stat and size,
 * but size can be returned as -1 if we don't know the size (if it's a
 * pipe or stdio).
 */
static int
open_local (const char *prog,
            const char *filename, bool writing, struct rw *rw)
{
  int flags, fd;

  if (strcmp (filename, "-") == 0) {
    synchronous = true;
    fd = writing ? STDOUT_FILENO : STDIN_FILENO;
    if (writing && isatty (fd)) {
      fprintf (stderr, "%s: refusing to write to tty\n", prog);
      exit (EXIT_FAILURE);
    }
  }
  else {
    /* If it's a block device and we're writing we don't want to turn
     * it into a truncated regular file by accident, so try to open
     * without O_CREAT first.
     */
    flags = writing ? O_WRONLY : O_RDONLY;
    fd = open (filename, flags);
    if (fd == -1) {
      if (writing) {
        /* Try again, with more flags. */
        flags |= O_TRUNC|O_CREAT|O_EXCL;
        fd = open (filename, flags, 0644);
      }
      if (fd == -1) {
        perror (filename);
        exit (EXIT_FAILURE);
      }
    }
  }

  if (fstat (fd, &rw->u.local.stat) == -1) {
    perror (filename);
    exit (EXIT_FAILURE);
  }
  if (S_ISBLK (rw->u.local.stat.st_mode)) {
    /* Block device. */
    rw->ops = &file_ops;
    rw->size = lseek (fd, 0, SEEK_END);
    if (rw->size == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
    if (lseek (fd, 0, SEEK_SET) == -1) {
      perror ("lseek");
      exit (EXIT_FAILURE);
    }
  }
  else if (S_ISREG (rw->u.local.stat.st_mode)) {
    /* Regular file. */
    rw->ops = &file_ops;
    rw->size = rw->u.local.stat.st_size;
  }
  else {
    /* Probably stdin/stdout, a pipe or a socket.  Set size == -1
     * which means don't know, and force synchronous mode.
     */
    synchronous = true;
    rw->ops = &pipe_ops;
    rw->size = -1;
  }

  return fd;
}

static void
open_nbd_uri (const char *prog,
              const char *uri, struct rw *rw)
{
  struct nbd_handle *nbd;

  rw->ops = &nbd_ops;
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", prog, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */

  if (handles_append (&rw->u.nbd, nbd) == -1) {
    perror ("realloc");
    exit (EXIT_FAILURE);
  }

  if (nbd_connect_uri (nbd, uri) == -1) {
    fprintf (stderr, "%s: %s: %s\n", prog, uri, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
}

DEFINE_VECTOR_TYPE (const_string_vector, const char *);

static void
open_nbd_subprocess (const char *prog,
                     const char **argv, size_t argc,
                     struct rw *rw)
{
  struct nbd_handle *nbd;
  const_string_vector copy = empty_vector;
  size_t i;

  rw->ops = &nbd_ops;
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", prog, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (handles_append (&rw->u.nbd, nbd) == -1) {
  memory_error:
    perror ("realloc");
    exit (EXIT_FAILURE);
  }

  /* We have to copy the args so we can null-terminate them. */
  for (i = 0; i < argc; ++i) {
    if (const_string_vector_append (&copy, argv[i]) == -1)
      goto memory_error;
  }
  if (const_string_vector_append (&copy, NULL) == -1)
    goto memory_error;

  if (nbd_connect_systemd_socket_activation (nbd, (char **) copy.ptr) == -1) {
    fprintf (stderr, "%s: %s: %s\n", prog, argv[0], nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  free (copy.ptr);
}