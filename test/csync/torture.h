/*
 * libcsync -- a library to sync a directory with another
 *
 * SPDX-FileCopyrightText: 2020 Nextcloud GmbH and Nextcloud contributors
 * SPDX-FileCopyrightText: 2008-2013 Andreas Schneider <asn@cryptomilk.org>
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef _TORTURE_H
#define _TORTURE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h> // NOLINT sometimes compiled in C mode
#include <stddef.h> // NOLINT sometimes compiled in C mode
#include <setjmp.h> // NOLINT sometimes compiled in C mode

#include <cmocka.h>

/* Used by main to communicate with parse_opt. */
struct argument_s {
  char *args[2];
  int verbose;
};

void torture_cmdline_parse(int argc, char **argv, struct argument_s *arguments);

int torture_csync_verbosity(void);

/*
 * This function must be defined in every unit test file.
 */
int torture_run_tests(void);

#ifdef __cplusplus
}
#endif

#endif /* _TORTURE_H */
