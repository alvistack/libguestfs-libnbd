/* NBD client library in userspace
 * Copyright (C) 2020-2021 Red Hat Inc.
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
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include <libnbd.h>

#include "vector.h"
#include "version.h"

#include "nbdinfo.h"

const char *progname;
struct nbd_handle *nbd;
FILE *fp;                       /* output file descriptor */
bool list_all = false;          /* --list option */
bool probe_content = false;     /* --content / --no-content option */
bool json_output = false;       /* --json option */
const char *map = NULL;         /* --map option */
bool size_only = false;         /* --size option */

struct export {
  char *name;
  char *desc;
};
DEFINE_VECTOR_TYPE (exports, struct export)
static exports export_list = empty_vector;

static int collect_export (void *opaque, const char *name,
                           const char *desc);
static bool list_all_exports (struct nbd_handle *nbd1, const char *uri);

static void __attribute__((noreturn))
usage (FILE *fp, int exitcode)
{
  fprintf (fp,
"\n"
"Display information and metadata about NBD servers and exports:\n"
"\n"
"    nbdinfo nbd://localhost\n"
"    nbdinfo \"nbd+unix:///?socket=/tmp/unixsock\"\n"
"    nbdinfo --size nbd://example.com\n"
"    nbdinfo --map nbd://example.com\n"
"    nbdinfo --json nbd://example.com\n"
"    nbdinfo --list nbd://example.com\n"
"\n"
"Please read the nbdinfo(1) manual page for full usage.\n"
"\n"
);
  exit (exitcode);
}

int
main (int argc, char *argv[])
{
  enum {
    HELP_OPTION = CHAR_MAX + 1,
    LONG_OPTIONS,
    SHORT_OPTIONS,
    CONTENT_OPTION,
    NO_CONTENT_OPTION,
    JSON_OPTION,
    MAP_OPTION,
    SIZE_OPTION,
  };
  const char *short_options = "LV";
  const struct option long_options[] = {
    { "help",               no_argument,       NULL, HELP_OPTION },
    { "content",            no_argument,       NULL, CONTENT_OPTION },
    { "no-content",         no_argument,       NULL, NO_CONTENT_OPTION },
    { "json",               no_argument,       NULL, JSON_OPTION },
    { "list",               no_argument,       NULL, 'L' },
    { "long-options",       no_argument,       NULL, LONG_OPTIONS },
    { "map",                optional_argument, NULL, MAP_OPTION },
    { "short-options",      no_argument,       NULL, SHORT_OPTIONS },
    { "size",               no_argument,       NULL, SIZE_OPTION },
    { "version",            no_argument,       NULL, 'V' },
    { NULL }
  };
  int c;
  size_t i;
  char *output = NULL;
  size_t output_len = 0;
  bool content_flag = false, no_content_flag = false;
  bool list_okay = true;

  progname = argv[0];

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

    case JSON_OPTION:
      json_output = true;
      break;

    case CONTENT_OPTION:
      content_flag = true;
      break;

    case NO_CONTENT_OPTION:
      no_content_flag = true;
      break;

    case MAP_OPTION:
      map = optarg ? optarg : "base:allocation";
      break;

    case SIZE_OPTION:
      size_only = true;
      break;

    case 'L':
      list_all = true;
      break;

    case 'V':
      display_version ("nbdinfo");
      exit (EXIT_SUCCESS);

    default:
      usage (stderr, EXIT_FAILURE);
    }
  }

  /* There must be exactly 1 parameter, the URI. */
  if (argc - optind != 1)
    usage (stderr, EXIT_FAILURE);

  /* You cannot combine certain options. */
  if (!!list_all + !!map + !!size_only > 1) {
    fprintf (stderr,
             "%s: you cannot use --list, --map and --size together.\n",
             progname);
    exit (EXIT_FAILURE);
  }
  if (content_flag && no_content_flag) {
    fprintf (stderr, "%s: you cannot use %s and %s together.\n",
             progname, "--content", "--no-content");
    exit (EXIT_FAILURE);
  }

  /* Work out if we should probe content. */
  probe_content = !list_all;
  if (content_flag)
    probe_content = true;
  if (no_content_flag)
    probe_content = false;
  if (map)
    probe_content = false;

  /* Try to write output atomically.  We spool output into a
   * memstream, pointed to by fp, and write it all at once at the end.
   * On error nothing should be printed on stdout.
   */
  fp = open_memstream (&output, &output_len);
  if (fp == NULL) {
    fprintf (stderr, "%s: ", progname);
    perror ("open_memstream");
    exit (EXIT_FAILURE);
  }

  /* Open the NBD side. */
  nbd = nbd_create ();
  if (nbd == NULL) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }
  nbd_set_uri_allow_local_file (nbd, true); /* Allow ?tls-psk-file. */

  /* Set optional modes in the handle. */
  if (!map && !size_only) {
    nbd_set_opt_mode (nbd, true);
    nbd_set_full_info (nbd, true);
  }
  if (map)
    nbd_add_meta_context (nbd, map);

  /* Connect to the server. */
  if (nbd_connect_uri (nbd, argv[optind]) == -1) {
    fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
    exit (EXIT_FAILURE);
  }

  if (list_all) {               /* --list */
    if (nbd_opt_list (nbd,
                      (nbd_list_callback) {.callback = collect_export}) == -1) {
      fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
      exit (EXIT_FAILURE);
    }
    if (probe_content)
      /* Disconnect from the server to move the handle into a closed
       * state, in case the server serializes further connections.
       * But we can ignore errors in this case.
       */
      nbd_opt_abort (nbd);
  }

  if (size_only)                /* --size (!list_all) */
    do_size ();
  else if (map)                 /* --map (!list_all) */
    do_map ();
  else {                        /* not --size or --map */
    const char *protocol;
    int tls_negotiated;

    /* Print per-connection fields. */
    protocol = nbd_get_protocol (nbd);
    tls_negotiated = nbd_get_tls_negotiated (nbd);

    if (!json_output) {
      if (protocol) {
        fprintf (fp, "protocol: %s", protocol);
        if (tls_negotiated >= 0)
          fprintf (fp, " %s TLS", tls_negotiated ? "with" : "without");
        fprintf (fp, "\n");
      }
    }
    else {
      fprintf (fp, "{\n");
      if (protocol) {
        fprintf (fp, "\"protocol\": ");
        print_json_string (protocol);
        fprintf (fp, ",\n");
      }

      if (tls_negotiated >= 0)
        fprintf (fp, "\"TLS\": %s,\n", tls_negotiated ? "true" : "false");
    }

    if (!list_all)
      list_okay = show_one_export (nbd, NULL, true, true);
    else
      list_okay = list_all_exports (nbd, argv[optind]);

    if (json_output)
      fprintf (fp, "}\n");
  }

  for (i = 0; i < export_list.size; ++i) {
    free (export_list.ptr[i].name);
    free (export_list.ptr[i].desc);
  }
  free (export_list.ptr);
  nbd_opt_abort (nbd);
  nbd_shutdown (nbd, 0);
  nbd_close (nbd);

  /* Close the output stream and copy it to the real stdout. */
  if (fclose (fp) == EOF) {
    fprintf (stderr, "%s: ", progname);
    perror ("fclose");
    exit (EXIT_FAILURE);
  }
  if (puts (output) == EOF) {
    fprintf (stderr, "%s: ", progname);
    perror ("puts");
    exit (EXIT_FAILURE);
  }

  exit (list_okay ? EXIT_SUCCESS : EXIT_FAILURE);
}

static int
collect_export (void *opaque, const char *name, const char *desc)
{
  struct export e;

  e.name = strdup (name);
  e.desc = strdup (desc);
  if (e.name == NULL || e.desc == NULL ||
      exports_append (&export_list, e) == -1) {
    perror ("malloc");
    exit (EXIT_FAILURE);
  }

  return 0;
}

static bool
list_all_exports (struct nbd_handle *nbd1, const char *uri)
{
  size_t i;
  bool list_okay = true;

  if (export_list.size == 0 && json_output)
    fprintf (fp, "\"exports\": []\n");

  for (i = 0; i < export_list.size; ++i) {
    const char *name = export_list.ptr[i].name;
    struct nbd_handle *nbd2;

    if (probe_content) {
      /* Connect to the original URI, but using opt mode to alter the export. */
      nbd2 = nbd_create ();
      if (nbd2 == NULL) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      nbd_set_uri_allow_local_file (nbd2, true); /* Allow ?tls-psk-file. */
      nbd_set_opt_mode (nbd2, true);
      nbd_set_full_info (nbd2, true);

      if (nbd_connect_uri (nbd2, uri) == -1 ||
          nbd_set_export_name (nbd2, name) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
    }
    else { /* ! probe_content */
      if (nbd_set_export_name (nbd1, name) == -1) {
        fprintf (stderr, "%s: %s\n", progname, nbd_get_error ());
        exit (EXIT_FAILURE);
      }
      nbd2 = nbd1;
    }

    /* List the metadata of this export. */
    if (!show_one_export (nbd2, export_list.ptr[i].desc, i == 0,
                          i + 1 == export_list.size))
      list_okay = false;

    if (probe_content) {
      nbd_shutdown (nbd2, 0);
      nbd_close (nbd2);
    }
  }
  return list_okay;
}
