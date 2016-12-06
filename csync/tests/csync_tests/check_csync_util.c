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
#include "torture.h"

#include "csync_util.h"

static void check_csync_instruction_str(void **state)
{
  const char *str;

  (void) state; /* unused */

  str = csync_instruction_str(CSYNC_INSTRUCTION_ERROR);
  assert_string_equal(str, "INSTRUCTION_ERROR");

  str = csync_instruction_str(0xFFFF);
  assert_string_equal(str, "ERROR!");
}

static void check_csync_memstat(void **state)
{
  (void) state; /* unused */

  csync_memstat_check();
}

int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_csync_instruction_str),
        cmocka_unit_test(check_csync_memstat),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

