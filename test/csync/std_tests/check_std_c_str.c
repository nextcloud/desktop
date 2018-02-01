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
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "torture.h"

#include "std/c_string.h"

static void check_c_streq_equal(void **state)
{
    (void) state; /* unused */

    assert_true(c_streq("test", "test"));
}

static void check_c_streq_not_equal(void **state)
{
    (void) state; /* unused */

    assert_false(c_streq("test", "test2"));
}

static void check_c_streq_null(void **state)
{
    (void) state; /* unused */

    assert_false(c_streq(NULL, "test"));
    assert_false(c_streq("test", NULL));
    assert_false(c_streq(NULL, NULL));
}



int torture_run_tests(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(check_c_streq_equal),
        cmocka_unit_test(check_c_streq_not_equal),
        cmocka_unit_test(check_c_streq_null),
    };

    return cmocka_run_group_tests(tests, NULL, NULL);
}

