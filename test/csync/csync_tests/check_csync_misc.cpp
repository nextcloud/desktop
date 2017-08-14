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

#include "csync_misc.h"
#include <stdlib.h>

static void check_csync_normalize_etag(void **state)
{
  char *str;

  (void) state; /* unused */

#define CHECK_NORMALIZE_ETAG(TEST, EXPECT) \
    str = csync_normalize_etag(TEST); \
    assert_string_equal(str, EXPECT); \
    free(str);


  CHECK_NORMALIZE_ETAG("foo", "foo");
  CHECK_NORMALIZE_ETAG("\"foo\"", "foo");
  CHECK_NORMALIZE_ETAG("\"nar123\"", "nar123");
  CHECK_NORMALIZE_ETAG("", "");
  CHECK_NORMALIZE_ETAG("\"\"", "");

  /* Test with -gzip (all combinaison) */
  CHECK_NORMALIZE_ETAG("foo-gzip", "foo");
  CHECK_NORMALIZE_ETAG("\"foo\"-gzip", "foo");
  CHECK_NORMALIZE_ETAG("\"foo-gzip\"", "foo");
}

int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_csync_normalize_etag),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

