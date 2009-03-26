/*
 * libcsync -- a library to sync a replica with another
 *
 * Copyright (c) 2006-2007 by Andreas Schneider <mail@cynapses.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * vim: ts=2 sw=2 et cindent
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <argp.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <csync.h>

#include "csync_auth.h"

enum {
  KEY_EXCLUDE_FILE = 129,
  KEY_TEST_UPDATE,
  KEY_TEST_RECONCILE,
  KEY_CREATE_STATEDB,
};

const char *argp_program_version = "csync commandline client "
  CSYNC_STRINGIFY(LIBCSYNC_VERSION);
const char *argp_program_bug_address = "<csync-devel@csync.org>";

/* Program documentation. */
static char doc[] = "csync -- a user level file synchronizer";

/* A description of the arguments we accept. */
static char args_doc[] = "SOURCE DESTINATION";

/* The options we understand. */
static struct argp_option options[] = {
  {
    .name  = "exclude-file",
    .key   = KEY_EXCLUDE_FILE,
    .arg   = "<file>",
    .flags = 0,
    .doc   = "Add an additional exclude file",
    .group = 0
  },
  {
    .name  = "disable-statedb",
    .key   = 'd',
    .arg   = NULL,
    .flags = 0,
    .doc   = "Disable the usage and creation of a statedb.",
    .group = 0
  },
  {
    .name  = "dry-run",
    .key   = KEY_TEST_RECONCILE,
    .arg   = NULL,
    .flags = 0,
    .doc   = "This runs only update detection and reconcilation.",
    .group = 0
  },
  {
    .name  = "test-statedb",
    .key   = KEY_CREATE_STATEDB,
    .arg   = NULL,
    .flags = 0,
    .doc   = "Test creation of the statedb. Runs update detection.",
    .group = 0
  },
  {
    .name  = "test-update",
    .key   = KEY_TEST_UPDATE,
    .arg   = NULL,
    .flags = 0,
    .doc   = "Test the update detection",
    .group = 0
  },
  {NULL, 0, 0, 0, NULL, 0}
};

/* Used by main to communicate with parse_opt. */
struct argument_s {
  char *args[2]; /* SOURCE and DESTINATION */
  char *exclude_file;
  int disable_statedb;
  int create_statedb;
  int update;
  int reconcile;
  int propagate;
};

/* Parse a single option. */
static error_t parse_opt (int key, char *arg, struct argp_state *state) {
  /* Get the input argument from argp_parse, which we
   * know is a pointer to our arguments structure.
   */
  struct argument_s *arguments = state->input;

  switch (key) {
    case 'd':
      arguments->disable_statedb = 1;
      break;
    case KEY_TEST_UPDATE:
      arguments->create_statedb = 0;
      arguments->update = 1;
      arguments->reconcile = 0;
      arguments->propagate = 0;
      break;
    case KEY_TEST_RECONCILE:
      arguments->create_statedb = 0;
      arguments->update = 1;
      arguments->reconcile = 1;
      arguments->propagate = 0;
      break;
    case KEY_EXCLUDE_FILE:
      arguments->exclude_file = strdup(arg);
      break;
    case KEY_CREATE_STATEDB:
      arguments->create_statedb = 1;
      arguments->update = 1;
      arguments->reconcile = 0;
      arguments->propagate = 0;
      break;
    case ARGP_KEY_ARG:
      if (state->arg_num >= 2) {
        /* Too many arguments. */
        argp_usage (state);
      }
      arguments->args[state->arg_num] = arg;
      break;
    case ARGP_KEY_END:
      if (state->arg_num < 2) {
        /* Not enough arguments. */
        argp_usage (state);
      }
      break;
    default:
      return ARGP_ERR_UNKNOWN;
  }

  return 0;
}

/* Our argp parser. */
static struct argp argp = {options, parse_opt, args_doc, doc, NULL, NULL, NULL};

int main(int argc, char **argv) {
  int rc = 0;
  CSYNC *csync;
  char errbuf[256] = {0};

  struct argument_s arguments;

  /* Default values. */
  arguments.exclude_file = NULL;
  arguments.disable_statedb = 0;
  arguments.create_statedb = 0;
  arguments.update = 1;
  arguments.reconcile = 1;
  arguments.propagate = 1;

  /*
   * Parse our arguments; every option seen by parse_opt will
   * be reflected in arguments.
   */
  argp_parse (&argp, argc, argv, 0, 0, &arguments);

  if (csync_create(&csync, arguments.args[0], arguments.args[1]) < 0) {
    fprintf(stderr, "csync_create: failed\n");
    exit(1);
  }

  csync_set_auth_callback(csync, csync_auth);
  if (arguments.disable_statedb) {
    csync_disable_statedb(csync);
  }

  if (csync_init(csync) < 0) {
    perror("csync_init");
    rc = 1;
    goto out;
  }

  if (arguments.exclude_file != NULL) {
    if (csync_add_exclude_list(csync, arguments.exclude_file) < 0) {
      fprintf(stderr, "csync_add_exclude_list - %s: %s\n",
          arguments.exclude_file,
          strerror_r(errno, errbuf, sizeof(errbuf)));
      rc = 1;
      goto out;
    }
  }

  if (arguments.update) {
    if (csync_update(csync) < 0) {
      perror("csync_update");
      rc = 1;
      goto out;
    }
  }

  if (arguments.reconcile) {
    if (csync_reconcile(csync) < 0) {
      perror("csync_reconcile");
      rc = 1;
      goto out;
    }
  }

  if (arguments.propagate) {
    if (csync_propagate(csync) < 0) {
      perror("csync_propagate");
      rc = 1;
      goto out;
    }
  }

  if (arguments.create_statedb) {
    csync_set_status(csync, 0xFFFF);
  }

out:
  csync_destroy(csync);

  return rc;
}

