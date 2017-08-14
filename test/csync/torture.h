/*
 * libcsync -- a library to sync a directory with another
 *
 * Copyright (c) 2008-2013 by Andreas Schneider <asn@cryptomilk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#ifndef _TORTURE_H
#define _TORTURE_H

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

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

#endif /* _TORTURE_H */
