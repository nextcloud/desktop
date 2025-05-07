/*
 * libcsync -- a library to sync a directory with another
 *
 * SPDX-FileCopyrightText: 2014 ownCloud GmbH
 * SPDX-FileCopyrightText: 2008-2013 Andreas Schneider <asn@cryptomilk.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "config_csync.h"

#include "torture.h"

static int verbosity;

int torture_csync_verbosity(void)
{
  return verbosity;
}

int main(int argc, char **argv)
{
  struct argument_s arguments;

  arguments.verbose = 0;
  torture_cmdline_parse(argc, argv, &arguments);
  verbosity = arguments.verbose;

  return torture_run_tests();
}

